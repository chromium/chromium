set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GOOGLE_API_KEY="no"
set GOOGLE_DEFAULT_CLIENT_ID="no"
set GOOGLE_DEFAULT_CLIENT_SECRET="no"
gn gen out\Release
gn args out\Release
copy /Y metachromium.cmd out\Release\metachromium.cmd
copy /Y example.html out\Release\example.html
autoninja -C out\Release mini_installer
copy /Y out\Release\chrome.packed.7z .\metachromium.7z