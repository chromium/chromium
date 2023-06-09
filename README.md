# 编译说明

```shell

# 安装depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="/path/to/depot_tools:$PATH"


# 获得代码
mkdir ~/chromium && cd ~/chromium
fetch --nohooks android
git tags ##选一个版本，不要用master编译


# 安装编译
build/install-build-deps.sh --android

# 指定编译目标
echo "target_os = [ 'android' ]" >> ../.gclient
gclient sync


# 指定编译参数
# gn args out/* 会影响是否开启strick_mode，BUILD.gn中有一段这样的判断：
# if (is_java_debug || android_channel == "default" || android_channel == "dev") { defines = [ "STRICT_MODE_CHECKING" ] }
gn args out/Release

# Build arguments go here.
# See "gn args <out_dir> --list" for available build arguments.
target_os = "android"
target_cpu = "arm64"
treat_warnings_as_errors = false
# is_official_build: 设置为 true 以构建官方版本，移除调试和开发相关的特性。
is_official_build = true
# is_component_build: 设置为 false 以构建非 Chrome 的组件化版本。
is_component_build = false
# is_debug: 设置为 false 以编译发布版本，排除调试符号和额外的调试功能。
is_debug = false
# symbol_level: 设置为 0 以移除符号表。
symbol_level = 0
ffmpeg_branding = "Chrome"
proprietary_codecs = true
is_java_debug = false

# android package name
chrome_public_manifest_package = "com.baidu.duer.chromium"

# override android apk version
android_override_version_code = "548111316"
android_override_version_name = "110.0.5481.114"

# android v2 sign
use_android_unwinder_v2 = true


# 编译apk
autoninja -C out/Release chrome_public_apk

```


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
