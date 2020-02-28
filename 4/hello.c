#include <gtk/gtk.h>
#include <dirent.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> //注意：这个字母是小写的L，而不是数字1。
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_LEN 256

GtkWidget *window;
GtkWidget *cpu_percent;
GtkWidget *mem_used; 
GtkWidget *mem_percent;
GtkWidget *mem_left;
GtkWidget *mem_total;
GtkWidget *mem_progress;
GtkWidget *swp_used ;
GtkWidget *swp_left;
GtkWidget *swp_total;
GtkWidget *swp_percent;
GtkWidget *swp_progress;
GtkWidget *modules_roll;
GtkWidget *modules_clist;
GtkWidget *pid_roll;
GtkWidget *pid_clist;
GtkWidget *label_total;
GtkWidget *label_running;
GtkWidget *label_sleep;
GtkWidget *label_zombie;
GtkWidget *label_idle;
GtkWidget *button_kill;
GtkWidget *button_refresh;
GtkWidget *label_nowtime;
GtkWidget *label_runtime;
static int pid_total;
static int pid_running;
static int pid_sleep;
static int pid_zombie;
static int pid_idle;
//gboolean g_update = TRUE;

char *cpu = "/proc/cpuinfo";
char *os = "/proc/version";
char *mem = "/proc/meminfo";
char *module = "/proc/modules";
GtkWidget *cpu_picture;
static GdkPixmap *cpu_graph = NULL;
GtkWidget *mem_picture;
static GdkPixmap *mem_graph = NULL;
GtkWidget *swap_picture;
static GdkPixmap *swap_graph = NULL;
int mem_ratio;
int swap_ratio;
static float cpu_ratio = 0.0;
static int counter = 5;
gint draw[200];
gint memdraw[200];
gint swapdraw[200];

void setProgress(GtkWidget *progress, GtkWidget *percent,
				 GtkWidget *label1, GtkWidget *label2, GtkWidget *label3, char *buf, const char *content);

void setPidList();
void setPidLabel();
void setTime();
void setMemAndSwap();


char *_(char *c)
{
return g_locale_to_utf8(c,-1,NULL,NULL,NULL);
}

void ShowErrorDialog(char *info)
{
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", info);
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void ShowInfoDialog(char *info)
{
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK, "%s", info);

    gtk_window_set_title(GTK_WINDOW(dialog), "Notice");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


static gboolean cpu_expose_event(GtkWidget *widget, GdkEventExpose *event,
								 gpointer data)
{
	GdkPixmap *current = NULL;
	if(widget == cpu_picture) current = cpu_graph;
	else if(widget == mem_picture) current = mem_graph;
	else current = swap_graph;
	gdk_draw_drawable(widget->window,
					  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
					  current,
					  event->area.x, event->area.y,
					  event->area.x, event->area.y,
					  event->area.width, event->area.height);
	return TRUE;
}

static gboolean cpu_configure_event(GtkWidget *widget,
									GdkEventConfigure *event, gpointer data)
{
	if (cpu_graph)
	{
		g_object_unref(cpu_graph);
	}
	cpu_graph = gdk_pixmap_new(widget->window,
							   widget->allocation.width,
							   widget->allocation.height, -1);
	gdk_draw_rectangle(cpu_graph, widget->style->white_gc,
					   TRUE, 0, 0, widget->allocation.width,
					   widget->allocation.height);
	return TRUE;
}
static gboolean mem_configure_event(GtkWidget *widget,
									GdkEventConfigure *event, gpointer data)
{
	if (mem_graph)
	{
		g_object_unref(mem_graph);
	}
	mem_graph = gdk_pixmap_new(widget->window,
							   widget->allocation.width,
							   widget->allocation.height, -1);
	gdk_draw_rectangle(mem_graph, widget->style->white_gc,
					   TRUE, 0, 0, widget->allocation.width,
					   widget->allocation.height);
	return TRUE;
}

static gboolean swap_configure_event(GtkWidget *widget,
									GdkEventConfigure *event, gpointer data)
{
	if (swap_graph)
	{
		g_object_unref(swap_graph);
	}
	swap_graph = gdk_pixmap_new(widget->window,
							   widget->allocation.width,
							   widget->allocation.height, -1);
	gdk_draw_rectangle(swap_graph, widget->style->white_gc,
					   TRUE, 0, 0, widget->allocation.width,
					   widget->allocation.height);
	return TRUE;
}

void DrawCPUGraph(GtkWidget *picture, GdkPixmap * graph, gint *mydraw, int myratio)
{
	int width, height, ratio;
    float step_w, step_h;
    int i;

    // 如果pixmap没有创建成功，则不绘图
    if (graph == NULL)
        return;

    // 设置风格
    GdkGC *gc = gdk_gc_new(GDK_DRAWABLE(graph));
    

    // 清除位图，并初始化为灰色
    gdk_draw_rectangle(GDK_DRAWABLE(graph), window->style->dark_gc[4], TRUE, 0, 0,
                       picture->allocation.width,
                       picture->allocation.height);

    // 获得绘图区大小
    width = picture->allocation.width;
    height = picture->allocation.height;

    // 获得当前CPU利用率
    ratio = myratio;
    // 移动数据 向前移动
    mydraw[200 - 1] = 200 - (float)ratio / 100 * 200 - 1;
    for (i = 0; i < 200 - 1; i++)
    {
        mydraw[i] = mydraw[i + 1];
    }

    // 计算步长
    step_w = (float)width / 200;
    step_h = (float)height / 200;
    // 设置颜色
    GdkColor color;
    gdk_color_parse("#ffcc80", &color);
    // 设置前景色的函数……
    gdk_gc_set_rgb_fg_color(gc, &color);


    // 连线
    for (i = 200 - 1; i >= 1; i--)
    {
        gdk_draw_line(graph, gc, i * step_w, mydraw[i] * step_h,
                      (i - 1) * step_w, mydraw[i - 1] * step_h);
    }

    gtk_widget_queue_draw(picture); //触发信号,刷新图片的整个区域
}

void getCpuUseRatio(void)
{
	long int user, nice, sys, idle, iowait, irq, softirq;
	static long int all_o = 0;
	static long int idle_o = 0;
	int fd = open("/proc/stat", O_RDONLY);
	char buf[128];
	int count = read(fd, buf, sizeof(buf));
	close(fd);
	buf[count] = '\0';
	sscanf(buf, "cpu %ld%ld%ld%ld%ld%ld%ld", &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
	long int all = user + nice + sys + idle + iowait + irq + softirq;
	//long int id = idle;
	cpu_ratio = (float)((all - all_o) - (idle - idle_o)) / (all - all_o) * 100.0;
	all_o = all;
    idle_o = idle;	
}

gint UpdateRatio(gpointer data)
{
	getCpuUseRatio();
	setMemAndSwap();
    if(counter == 5){
		char temp[10] = {0};
		sprintf(temp, "%.2f%%", cpu_ratio);
		gtk_label_set_text(GTK_LABEL(cpu_percent), temp);
		setTime();
		counter = 0;
	}
	counter ++;
	DrawCPUGraph(cpu_picture, cpu_graph, draw, (int)cpu_ratio);
	DrawCPUGraph(mem_picture, mem_graph, memdraw, mem_ratio);
	DrawCPUGraph(swap_picture, swap_graph, swapdraw, swap_ratio);
    return 1;
}
//获取proc信息，并且设置label
void setLabel(GtkWidget *label, char *buf, const char *content)
{
	char *res = strstr(buf, content);
	int i = 0;
	while (res[i] != ':')
		i++;
	i++;
	char temp[50];
	int j = 0;
	while (res[i] != '\n')
		temp[j++] = res[i++];
	temp[j] = '\0';
	gtk_label_set_text(GTK_LABEL(label), temp);
}

//os_version内容结构有些不一样，
void setOS(GtkWidget *label, char *buf, const char *content)
{
	char *res = strstr(buf, content);
	int i = 0;
	int j = 0;
	char temp[50];
	// if(strlen(buf) == 1) {
	// 	while(buf[i] != ' ') temp[j++] = buf[i++];
	// }
	// else
	if (content[0] == 'v')
	{
		while (res[i] != ' ')
			i++;
		i++;
		while (res[i] != ' ')
			temp[j++] = res[i++];
	}
	else
	{
		while (res[i] != 'n')
			i++;
		i += 2;
		while (res[i] != ')')
			temp[j++] = res[i++];
		temp[j++] = ')';
	}
	temp[j] = '\0';
	gtk_label_set_text(GTK_LABEL(label), temp);
}

void setMemAndSwap()
{
	FILE *stream;
	stream = popen("free -k", "r");
	char buf[MAX_LEN];
	fread(buf, sizeof(char), sizeof(buf), stream);
	pclose(stream);
	setProgress(mem_progress, mem_percent, mem_used, mem_left, mem_total, buf, "Mem");
	setProgress(swp_progress, swp_percent, swp_used, swp_left, swp_total, buf, "Swap");
}

//进度条设置
void setProgress(GtkWidget *progress, GtkWidget *percent,
				 GtkWidget *label1, GtkWidget *label2, GtkWidget *label3, char *buf, const char *content)
{
	char str1[20] = "已用:";
	char str2[20] = "剩余:";
	char str3[20] = "总共:";
	char temp[20];
	char kb[5] = " KB";
	memset(temp, '\0', sizeof(temp));
	char *res = strstr(buf, content);
	int i = 0;
	while (!(res[i] >= '0' && res[i] <= '9'))
		i++;
	int len = strlen(str3);
	int total = 0;
	int used = 0;
	while (res[i] != ' ')
	{
		total = total * 10 + (res[i] - '0');
		str3[len++] = res[i++];
	}
	strcat(str3, kb);
	//str3[len++]=' ';str3[len++]='K';str3[len++]='B';str3[len]='\0';
	while (res[i] == ' ')
		i++;
	len = strlen(str1);
	while (res[i] != ' ')
	{
		used = used * 10 + (res[i] - '0');
		str1[len++] = res[i++];
	}
	strcat(str1, kb);
	//str2[len++]=' ';str2[len++]='K';str2[len++]='B';str2[len]='\0';
	sprintf(temp, "%d", (total - used));
	strcat(str2, temp);
	len = strlen(str2);
	strcat(str2, kb);
	//str1[len++]=' ';str1[len++]='K';str1[len++]='B';str1[len]='\0';
	gdouble fraction = used / (1.0 * total);
	memset(temp, '\0', sizeof(temp));
	sprintf(temp, "%.0f%%", fraction * 100);
	if(progress == mem_progress) mem_ratio = (int)(fraction*100);
	else swap_ratio = (int)(fraction*100);
	if(counter == 5){
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);
		gtk_label_set_text(GTK_LABEL(percent), temp);
		gtk_label_set_text(GTK_LABEL(label1), str1);
		gtk_label_set_text(GTK_LABEL(label2), str2);
		gtk_label_set_text(GTK_LABEL(label3), str3);

	}	
}

//填写pid列表内容
void setPidList()
{
	FILE   *stream; 
    char buf[4096];
	memset( buf, '\0', sizeof(buf) );
	stream = popen( "ls /proc/ | grep '[0-9]'", "r" );
	fread( buf, sizeof(char), sizeof(buf), stream);
	pclose( stream );
	char res[10] = {'\0'};
	char dir[30] = {'\0'};  
	char pid[10];
    char name[20];
    char state[3] = {0};
    char priority[5];
    char mem[20];
    char other[20] = {'\0'};
	gchar *info[5];
	int fd;
	int i = 0;
    while(buf[i] != '\0'){
        sscanf(buf+i, "%s", res);
		sprintf(dir, "/proc/%s/stat", res);

		freopen(dir, "r", stdin);
		scanf("%s ", pid);
		scanf("(%s", name);
		name[strlen(name) - 1] = '\0';
		getchar();
		state[0] = getchar();
		int k = 0;
		for (; k < 14; k++) scanf("%s", other);
		scanf("%s", priority);
		for(k = 0; k<4; k++) scanf("%s", other);
		scanf("%s", mem);
		fclose(stdin);
		long mem_B = 0;
		for(k = 0; k<strlen(mem); k++) mem_B = mem_B*10 + mem[k] - '0';
		mem_B /= 1024;
		sprintf(mem, "%ld", mem_B);
		info[0] = _(pid);
		info[1] = _(name);
		info[2] = _(state);
		info[3] = _(priority);
		info[4] = _(mem);
		gtk_clist_append((GtkCList *)pid_clist, info);
		pid_total++;
		char c = state[0];
		if(c== 'R') pid_running++;
		else if(c == 'S') pid_sleep++;
		else if(c == 'Z') pid_zombie++;
		else if(c == 'I') pid_idle++;
        while(buf[i] != '\n') i++;
        i++;
	}
	//printf("%d %d %d %d %d\n", pid_total, pid_running, pid_sleep, pid_zombie, pid_idle);
}

//填写pid标签内容
void setPidLabel()
{
	char temp[20];
	sprintf(temp, "总进程数:%d", pid_total);
	gtk_label_set_text(GTK_LABEL(label_total), temp);
	sprintf(temp, "运行进程:%d", pid_running);
	gtk_label_set_text(GTK_LABEL(label_running), temp);
	sprintf(temp, "睡眠进程:%d", pid_sleep);
	gtk_label_set_text(GTK_LABEL(label_sleep), temp);
	sprintf(temp, "僵死进程:%d", pid_zombie);
	gtk_label_set_text(GTK_LABEL(label_zombie), temp);
	sprintf(temp, "空闲进程:%d", pid_idle);
	gtk_label_set_text(GTK_LABEL(label_idle), temp);
}

void pidRefresh(GtkWidget *widget)
{
	gtk_clist_freeze(GTK_CLIST(pid_clist));
	gtk_clist_clear(GTK_CLIST(pid_clist));
	pid_total = pid_running = pid_sleep = pid_zombie = pid_idle = 0;
	setPidList();
	setPidLabel();
	gtk_clist_thaw(GTK_CLIST(pid_clist));
}

void pidKill(GtkWidget *widget)
{
	char *pid_temp;
	gtk_clist_get_text(GTK_CLIST(pid_clist), GTK_CLIST(pid_clist)->focus_row, 0, &pid_temp);
	char op[32];
	sprintf(op, "kill -9 %s", pid_temp);
	printf("%s\n", op);
	int ret = system(op);
	if(ret != 0){
		ShowErrorDialog("进程结束失败！");
	}
	else {
		ShowInfoDialog("进程成功！");
		char *state_temp;
		gtk_clist_get_text(GTK_CLIST(pid_clist), GTK_CLIST(pid_clist)->focus_row, 2, &state_temp);
		char c = state_temp[0];
		if(c== 'R') pid_running--;
		else if(c == 'S') pid_sleep--;
		else if(c == 'Z') pid_zombie--;
		else if(c == 'I') pid_idle--;
		pid_total--;
		setPidLabel();
		gtk_clist_remove (GTK_CLIST(pid_clist),GTK_CLIST (pid_clist)->focus_row);
	}
}


//刷新进程
gint UpdatePid(gpointer data)
{
	pidRefresh(NULL);
	return 1;
}

//填写module内容
void setModules()
{
	gchar *info[3];
	char name[20];
    char mem[20];
    char times[20];
    FILE   *stream; 
    char buf[2048];
    memset( buf, '\0', sizeof(buf) );
    stream = popen( "cat /proc/modules | awk '{print $1\" \"$2\" \"$3}'", "r" );
    fread( buf, sizeof(char), sizeof(buf), stream); 
    pclose( stream ); 
    int i = 0;
    while(buf[i] != '\0'){
        sscanf(buf+i, "%s%s%s", name, mem, times);
		info[0] = name;
		info[1] = mem;
		info[2] = times;
		gtk_clist_append((GtkCList *)modules_clist, info);
        while(buf[i] != '\n') i++;
        i++;
    }
}

//模块刷新按钮
void moduleRefresh(GtkWidget *widget)
{
	gtk_clist_freeze(GTK_CLIST(modules_clist));
	gtk_clist_clear(GTK_CLIST(modules_clist));
	setModules();
	gtk_clist_thaw(GTK_CLIST(modules_clist));
}

//装载模块
void moduleLoad(GtkWidget *widget)
{
	GtkWidget *dialog;  
    GtkWidget *table;  
    GtkWidget *dir;
    GtkWidget *lb;
	dialog = gtk_dialog_new_with_buttons("装载模块(需要管理员权限)",NULL,  
                                         GTK_DIALOG_MODAL,  
                                         GTK_STOCK_OK,GTK_RESPONSE_OK,  
                                         GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,  
                                         NULL);  
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);  
	lb = gtk_label_new("模块路径:");  
	dir = gtk_entry_new();  
	gtk_entry_set_text(GTK_ENTRY(dir)," ");  
	table = gtk_table_new(1,2,FALSE); 
	gtk_table_attach_defaults(GTK_TABLE(table),lb,0,1,0,1); 
	gtk_table_attach_defaults(GTK_TABLE(table),dir,1,2,0,1);  

	gtk_table_set_row_spacings(GTK_TABLE(table),5);  
    gtk_table_set_col_spacings(GTK_TABLE(table),5);  
    gtk_container_set_border_width(GTK_CONTAINER(table),5);  
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),table);  
    gtk_widget_show_all(dialog);  

	gint result = gtk_dialog_run(GTK_DIALOG(dialog));  
	int ret;
	if(result == GTK_RESPONSE_OK)
	{
		char op[256] = {'\0'};
		sprintf(op, "insmod %s", gtk_entry_get_text(GTK_ENTRY(dir)));
		ret = system(op);
		if(ret != 0){
			ShowErrorDialog("模块卸载失败！");
		}
		else ShowInfoDialog("模块卸载成功！");
	}
    gtk_widget_destroy(dialog); 
	if(ret == 0) moduleRefresh(NULL); 
}


//卸载模块
void moduleUnload(GtkWidget *widget)
{
	char *name;
	gtk_clist_get_text(GTK_CLIST(modules_clist), GTK_CLIST(modules_clist)->focus_row, 0, &name);
	//printf("++++++%s\n", text);
	char op[256] = {'\0'};
	sprintf(op, "rmmod %s", name);
	int ret = system(op);
	if(ret != 0){
		ShowErrorDialog("模块卸载失败！");
	}
	else {
		ShowInfoDialog("模块卸载成功！");
		gtk_clist_remove (GTK_CLIST(modules_clist),GTK_CLIST (modules_clist)->focus_row);
	}
}

//重启响应函数
void reboot(GtkWidget *widget)
{
	printf("reboot\n");
	//system("sudo shutdown -r now");
}

//关机响应函数
int coount_shutdown = 0;
void shutdown(GtkWidget *widget)
{
	if (coount_shutdown != 0)
	{
		printf("shutdown\n");
		//system("sudo shutdown -h now");
	}
	coount_shutdown++;
}

void setTime()
{
	FILE   *stream; 
   	FILE   *timestream;
    char   buf[64]; 
    char time[64];
    memset( buf, '\0', sizeof(buf) );
    stream = popen( "cat /proc/uptime", "r" ); 
    timestream = popen("date", "r");

    fread( buf, sizeof(char), sizeof(buf), stream); 
    fread( time, sizeof(char), sizeof(time), timestream); 
    pclose( stream );  
    pclose(timestream);
    double secs;
    sscanf(buf, "%lf", &secs);
    int run_days = (int)(secs / 86400);
    int run_hour = ((int)secs % 86400)/3600;
    int run_minute = ((int)secs % 3600)/60;
    int run_second = ((int)secs % 60);
	char nowTime[128];
	char runTime[128];
	int m = strlen(time);
	while(!(time[m] >= '0' && time[m] <= '9')) m--;
	time[m+1] = '\0';
	sprintf(nowTime, "系统当前时间:%s", time);
	sprintf(runTime, "系统已运行：%dday(s) %d:%d:%d", run_days,run_hour,run_minute,run_second);
	gtk_label_set_text(GTK_LABEL(label_nowtime), nowTime);
	gtk_label_set_text(GTK_LABEL(label_runtime), runTime);
}

int main(int argc, char *argv[])
{
	//文件读取以及缓冲区
	int file;
	char buf[MAX_LEN];
	memset(buf, '\0', sizeof(buf));
	int read_size;
	//1.gtk初始化
	gtk_init(&argc, &argv);
	//2.创建GtkBuilder对象，GtkBuilder在<gtk/gtk.h>声明
	GtkBuilder *builder = gtk_builder_new();
	//3.读取test.glade文件的信息，保存在builder中
	if (!gtk_builder_add_from_file(builder, "test.glade", NULL))
	{
		printf("connot load file!");
		return -1;
	}
	//4.获取窗口指针，注意"window1"要和glade里面的标签名词匹配
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
					   GTK_SIGNAL_FUNC(gtk_exit), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
					   GTK_SIGNAL_FUNC(gtk_exit), NULL);
	//页面1内容
	
	mem_used = GTK_WIDGET(gtk_builder_get_object(builder, "label32"));
	mem_left = GTK_WIDGET(gtk_builder_get_object(builder, "label33"));
	mem_total = GTK_WIDGET(gtk_builder_get_object(builder, "label34"));
	mem_percent = GTK_WIDGET(gtk_builder_get_object(builder, "label31"));
	mem_progress = GTK_WIDGET(gtk_builder_get_object(builder, "progressbar1"));
	swp_used = GTK_WIDGET(gtk_builder_get_object(builder, "label35"));
	swp_left = GTK_WIDGET(gtk_builder_get_object(builder, "label36"));
	swp_total = GTK_WIDGET(gtk_builder_get_object(builder, "label37"));
	swp_percent = GTK_WIDGET(gtk_builder_get_object(builder, "label39"));
	swp_progress = GTK_WIDGET(gtk_builder_get_object(builder, "progressbar2"));
	GtkWidget *cpu_frame = GTK_WIDGET(gtk_builder_get_object(builder, "frame9"));
	cpu_percent = GTK_WIDGET(gtk_builder_get_object(builder, "label45"));
	cpu_picture = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(cpu_picture, TRUE);
	gtk_widget_set_size_request(cpu_picture, 400, 300);
	gtk_container_add(GTK_CONTAINER(cpu_frame), cpu_picture);
	g_signal_connect(cpu_picture, "expose_event",
					 G_CALLBACK(cpu_expose_event), NULL);
	g_signal_connect(cpu_picture, "configure_event",
					 G_CALLBACK(cpu_configure_event), NULL);
	int i; for(i=0; i<200; i++) draw[i] = 200;
	g_timeout_add(200, UpdateRatio, NULL);
	getCpuUseRatio();
	setMemAndSwap();

	GtkWidget *mem_frame = GTK_WIDGET(gtk_builder_get_object(builder, "frame11"));
	mem_picture = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(mem_picture, TRUE);
	gtk_widget_set_size_request(mem_picture, 400, 300);
	gtk_container_add(GTK_CONTAINER(mem_frame), mem_picture);
	g_signal_connect(mem_picture, "expose_event",
					 G_CALLBACK(cpu_expose_event), NULL);
	g_signal_connect(mem_picture, "configure_event",
					 G_CALLBACK(mem_configure_event), NULL);
	for(i=0; i<200; i++) memdraw[i] = 200;

	GtkWidget *swap_frame = GTK_WIDGET(gtk_builder_get_object(builder, "frame10"));
	swap_picture = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(swap_picture, TRUE);
	gtk_widget_set_size_request(swap_picture, 400, 300);
	gtk_container_add(GTK_CONTAINER(swap_frame), swap_picture);
	g_signal_connect(swap_picture, "expose_event",
					 G_CALLBACK(cpu_expose_event), NULL);
	g_signal_connect(swap_picture, "configure_event",
					 G_CALLBACK(swap_configure_event), NULL);
	for(i=0; i<200; i++) swapdraw[i] = 200;
	
	//页面2内容
	pid_roll = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow2"));
	label_total  = GTK_WIDGET(gtk_builder_get_object(builder, "label40"));
	label_running = GTK_WIDGET(gtk_builder_get_object(builder, "label41"));
	label_sleep = GTK_WIDGET(gtk_builder_get_object(builder, "label42"));
	label_zombie = GTK_WIDGET(gtk_builder_get_object(builder, "label43"));
	label_idle = GTK_WIDGET(gtk_builder_get_object(builder, "label47"));
	button_kill = GTK_WIDGET(gtk_builder_get_object(builder, "button7"));
	button_refresh = GTK_WIDGET(gtk_builder_get_object(builder, "button8"));
	gchar *pid_titles[5] = {"PID", "名称", "状态", "优先级", "占用内存(KB)"};
	pid_clist = gtk_clist_new_with_titles(5, pid_titles);
	gtk_clist_set_column_width (GTK_CLIST(pid_clist), 0, 50);
	gtk_clist_set_column_width (GTK_CLIST(pid_clist), 1, 150);
	gtk_clist_set_shadow_type(GTK_CLIST(pid_clist), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(pid_roll), pid_clist);
	g_signal_connect(button_refresh, "clicked", G_CALLBACK(pidRefresh), NULL);
	g_signal_connect(button_kill, "clicked", G_CALLBACK(pidKill), NULL);
	setPidList();
	setPidLabel();
	//g_timeout_add(1000, UpdatePid, NULL);

	//页面3内容
	modules_roll = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow1"));	
	gchar *titles[3] = {"模块名称", "占用内存", "使用次数"};
	modules_clist = gtk_clist_new_with_titles(3, titles);
	gtk_clist_set_column_width (GTK_CLIST(modules_clist), 0, 150);
	gtk_clist_set_shadow_type(GTK_CLIST(modules_clist), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(modules_roll), modules_clist);
	GtkWidget *module_load = GTK_WIDGET(gtk_builder_get_object(builder, "button4"));
	GtkWidget *module_unload = GTK_WIDGET(gtk_builder_get_object(builder, "button5"));
	GtkWidget *module_refresh = GTK_WIDGET(gtk_builder_get_object(builder, "button6"));
	g_signal_connect(module_refresh, "clicked", G_CALLBACK(moduleRefresh), NULL);
	g_signal_connect(module_load, "clicked", G_CALLBACK(moduleLoad), NULL);
	g_signal_connect(module_unload, "clicked", G_CALLBACK(moduleUnload), NULL);
	setModules();



	//页面4内容
	GtkWidget *cpu_name = GTK_WIDGET(gtk_builder_get_object(builder, "label19"));
	GtkWidget *cpu_type = GTK_WIDGET(gtk_builder_get_object(builder, "label20"));
	GtkWidget *cpu_fren = GTK_WIDGET(gtk_builder_get_object(builder, "label21"));
	GtkWidget *cpu_cache = GTK_WIDGET(gtk_builder_get_object(builder, "label22"));

	file = open(cpu, O_RDONLY);
	read_size = read(file, buf, MAX_LEN);
	buf[read_size - 1] = '\0';
	setLabel(cpu_name, buf, "vendor_id");
	setLabel(cpu_type, buf, "model name");
	setLabel(cpu_fren, buf, "cpu MHz");
	setLabel(cpu_cache, buf, "cache size");
	close(file);

	GtkWidget *os_type = GTK_WIDGET(gtk_builder_get_object(builder, "label26"));
	GtkWidget *os_version = GTK_WIDGET(gtk_builder_get_object(builder, "label27"));
	GtkWidget *os_gcc = GTK_WIDGET(gtk_builder_get_object(builder, "label28"));
	file = open(os, O_RDONLY);
	read_size = read(file, buf, MAX_LEN);
	buf[read_size - 1] = '\0';
	gtk_label_set_text(GTK_LABEL(os_type), "Linux");
	setOS(os_version, buf, "version");
	setOS(os_gcc, buf, "gcc version");
	close(file);
	

	//页面5的内容
	GtkWidget *button_reboot;
	GtkWidget *button_shutdown;
	GtkWidget *button_exit;
	button_reboot = GTK_WIDGET(gtk_builder_get_object(builder, "button1"));
	button_shutdown = GTK_WIDGET(gtk_builder_get_object(builder, "button2"));
	button_exit = GTK_WIDGET(gtk_builder_get_object(builder, "button3"));
	g_signal_connect(button_reboot, "clicked", G_CALLBACK(reboot), NULL);
	g_signal_connect(button_shutdown, "clicked", G_CALLBACK(shutdown), NULL);
	g_signal_connect(button_exit, "clicked", G_CALLBACK(gtk_main_quit), NULL);



	//页面6内容
	label_nowtime = GTK_WIDGET(gtk_builder_get_object(builder, "label49"));
	label_runtime = GTK_WIDGET(gtk_builder_get_object(builder, "label50"));
	setTime();


	//显示所有内容
	gtk_widget_show_all(window);
	gtk_main();
	return 0;
}