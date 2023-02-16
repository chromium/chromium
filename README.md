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
gn args out/Default

target_os = "android"
target_cpu = "arm64"
treat_warnings_as_errors = false
is_official_build = true
ffmpeg_branding = "Chrome"
proprietary_codecs = true
chrome_public_manifest_package = "com.baidu.duer.chromium"


# 编译apk
autoninja -C out/Default chrome_public_apk

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
