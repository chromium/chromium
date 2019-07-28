mkdir cloudretroandroid

cd cloudretroandroid

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

export PATH="$PATH:$(pwd)/depot_tools"

git clone https://github.com/byte4byte/cloudretro.git src

cd src

echo "solutions = [ { "url": "https://github.com/byte4byte/cloudretro.git", "managed": False, "name": "src", "custom_deps": {}, }, ] target_os = ["android"] target_os = [ 'android' ]" > ../.gclient

gclient sync

build/install-build-deps-android.sh

gclient runhooks

gn gen --args='target_os="android"' out/Default

-- or --

gn gen --args='target_os="android" target_cpu="arm64"' out/Default

ninja -C out/Default content_shell_apk