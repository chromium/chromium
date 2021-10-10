# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory Structure
](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.

If you found a bug, please file it at https://crbug.com/new. 

# 

# 概述
Chromium 主要分为三个部分：Browser, Renderer, WebKit. Browser(浏览器)是主进程，负责所有UI和I/O操作；Renderer(渲染器)是每个页面的渲染进程，它是browser的子进程，通常作为是由Browser所调用的标签的子进程，Renderer嵌入WebKit中对页面进行布局和渲染；Webkit是Chrome对浏览器Webkit内核的一个封装，是浏览器内核心与外部调用的一个中间层。

# 顶层目录

* apps : 应用平台代码，与国际化有关，此该目录中的工程源代码是与主流OS平台相关的系统应用代码。正对不同OS，提供了不同的C++实现文件。例如：剪贴板板调用、操作系统数据交换接口、系统资源管理等。

* base : 基础设施代码，此目录包含了一些公用库，包含大量的框架代码的实现，比如进程、线程以及消息循环的封装，对字符串的处理，C++对象生命周期管理，json解析处理、路径、日期时间以及日志服务等。建议从该部分代码开始学习分析Chrome。因为通过此处的代码的分析，对理解chrome的基础架构设计将会有很大帮助。

* breakpad : 谷歌开源的崩溃上报项目。辅助库，用于崩溃服务框架。当Chrome因为一场而崩溃(Crash)时，捕获异常，并将崩溃现场数据发送给google进行分析。

* build : 编译构建相关的工具支持库，其中包括了Google自己的URL解析库。

* cc : chromium compositor(合成器) 实现

* chrome : 浏览器主程序模块实现代码，是核心代码，将是后续代码分析的重点之一。Chrome目录包括了UI实现和Render部分两大部分。其中重要工程是：Browser、Renderer和Plugin等。其中Renderer部分是对webkit的封装。该目录中代码数量巨大，Google自己的代码，后续改动频繁的代码大部分集中在这里。

* chrome_frame : 该目录下是google针对IE开发的一个插件代码，使得IE可以使用chrome的Renderer渲染引擎来显示网页。

* courgette : 辅助库，昵称：小胡瓜。该目录包含一个用于生成浏览器升级二进制包的工具。该工具的目的是减少升级过程中数据下载的大小。例如，升级需要替换一个比较大的DLL文件，假设该文件大小超过5M，而新版本可能只是添加了一行代码，变化很小。在Chrome在升级时，通过courgette这个小工具比较新旧两个DLL，找到差异部分，并提取差异部分生成升级包下在到本地进行升级，这样下载的升级包可能只有几十K甚至几K。这将大大缩短用户的升级时间，对于网速慢的用户来说无疑是巨大的福音。
* gpu : GPU加速模块，利用GPU渲染页面，提高浏览器性能。

* ipc : 该目录里是Chrome的核心库之一：进程通信基础设施库。chrome浏览器采用多进程架构，进程间的通信基于IPC。 在windows下的该IPC库采用命名管道、异步IO（完成端口）、共享内存来实现进程间数据传输，效率比较高。IPC库不仅封装了IO机制，而且还定义了统一的消息传输格式，对多进程感兴趣的童鞋应该仔细阅读这里的代码。

* jingle : 本意是 “叮当声”，该目录是XMPP（The eXtensible Messaging and Presence Protocol可扩展通讯和表示协议）的扩展协议模块。通过Jingle可以实现点对点(P2P)的多媒体交互会话控制。例如：语音交互(VOIP)、视频交互等。Jinggle是由Google和XMPP基金会共同设计的。

* media : 该目录包含多媒体音频和视频解码相关的模块。

* native_client : 该目录是在浏览器中运行native代码的模块。Native Client是一种可以使本地代码在浏览器上运行的技术。该技术被视为微软ActiveX技术的继任者。项目具体细节可参考native client官网。尽管ActiveX因为其脆弱的安全性而饱受用户和开发者的诟病，但Native Client是否能克服这些问题依然值得考验。

* chrometest : 测试用数据

* content : 与浏览器页面处理相关的部分。在早期的Chrome版本中，content内容包含在chrome目录中。在新的版本中，Google将浏览器页面处理部分从chrome模块摘出来，单独形成一个工程目录。

* device : 对底层硬件接口进行抽象，使其可以跨平台调用

* net : 该目录是具体的网络协议实现基础库，其中包括ftp、http等客户端协议栈的实现代码。

* ppapi : 该目录是一个浏览器插件(Plugin）API模块，全称为Pepper(胡椒) Plugin API，是Google在NPAPI(Netscape(网景公司) Plugin API)基础上的发展。PPAPI对NPAPI进行了一些修改，使其更方便而且更安全。该扩展模块被设计用来缓解进程外部拆建执行的实现，并且提供一个框架使得插件完全跨平台。该模块的主要包括: 跨浏览器的NPAPI的统一语义；扩展运行与独立于渲染器(Renderer）/浏览器(Browser）之外的进程；使用浏览器的合成过程规范渲染；定义标准化事件和2D光栅功能；提供3D图形访问的初步尝试；插件注册。

* printing : 该目录包含打印模块，实现页面的打印以及打印预览。

* remoteing : 该目录包含通过终端服务运行应用程序的模块，就是大家听说过的Chromoting这个东东。该功能可以在Chrome/Chrome OS上远程执行其他平台上的本地应用程序，其方式为终端服务或者使用RDP或VNC连接到远程主机执行应用。简单说就是Chrome的远程桌面功能,目前该功能正在完善中。

* rlz：该目录非常特殊，因为它是chrome项目中唯一不提供源代码的。该模块主要用于用户行为追踪就是将用户行为收集报告给google。该模块虽然这对Chrome产品的改善计划提供了很大帮助，但其内在的用户隐私也存在安全问题，因为Google会怎么收集数据、收集什么数据、数据流向都是一个秘密。

* sandbox：该目录包含沙盒安全技术模块。该技术用于在浏览网页的时候，保护计算机不被恶意代码侵入。简单说就是虚拟出一个内存空间，将浏览Web时插件对系统功能的调用放到这个虚拟空间中进行，如果发现调用非法，则立刻回卷这部分内容，确保用户系统关键数据不会被恶意应用程序或者病毒修改。该技术伴随windows2000操作系统出现。沙箱是相对安全的，但不是绝对安全，因为已经有攻击成功案例。

* skia : 该模块是google收购的SKIA公司提供的2D图形渲染引擎库。通常图形渲染库的优劣决定了浏览器的显示效果。

* sql : 该目录是包含Chrome数据库方面的模块。Chrome采用了SQLITE3数据库引擎。在该模块中包含了对SQLITE3的封装以及对SQL语句的封装和处理。

* testing : C++单元测试框架库，谷歌开源的测试工具 GTest。用于进行单元测试。

* third_party : 一系列第三方库，例如 图片解码，解压缩库。chrome/third_party 里包含了一些专门给 chrome 用的第三方库，该目录下是第三方开源支持库，包含了Chrome项目中所有第三方的开源库，其中最重要的是webkit内核。

* tools : 该目录包含Chrome项目所使用的工具模块，比如堆栈调用、内存监测钩子等等。

* ui : 该目录是Chrome的界面库。

* views : 该目录是Chrome的界面控件元素库，针对不同OS平台进行了统一封装，其绘制采用skia引擎实现。Views包括UI事件交互机制、各种控件（如按钮、菜单、树、选择框等等）。

* url : 谷歌开源的 url 解析和标准化库

* v8 : 该目录是Javascript引擎，库，也是chrome的重要内核库。

* webkit : 该目录并不是Webkit，而是Chrome项目对webkit内核的一个封装层。封装的目的是在上层应用调用和webkit内核之间提供一个中间接口层，使Webkit内核功能透明，方便其上层的应用开发。在该目录下的support中有一个名字叫glue的工程。
