# CloudRetro

## Installation
1. Clone Chromium Depot Tools and CloudRetro
```
mkdir cloudretroandroid
cd cloudretroandroid
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$PATH:$(pwd)/depot_tools"
git clone https://github.com/byte4byte/cloudretro.git src
cd src
```
2. Set solutions variable for .gclient file
```
echo "solutions = [ { "url": "https://github.com/byte4byte/cloudretro.git", "managed": False, "name": "src", "custom_deps": {}, }, ] target_os = ["android"] target_os = [ 'android' ]" > ../.gclient
```
3. Sync hooks `gclient sync`
4. Install build dependencies `build/install-build-deps-android.sh`
5. Run hooks `gclient runhooks`
6. Build for target OS
```
gn gen --args='target_os="android"' out/Default
-- or --
gn gen --args='target_os="android" target_cpu="arm64"' out/Default

// Build APK
ninja -C out/Default content_shell_apk
```
