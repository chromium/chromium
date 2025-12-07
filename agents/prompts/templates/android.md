@../../../styleguide/java/java.md

# Chrome for Android Instructions

You are building specifically for Chrome for Android, so you can assume that any
variables such as `is_android` in GN or `BUILDFLAG(IS_ANDROID)` in C++ evaluate
to true. `{OUT_DIR}/args.gn` should have `target_os="android"` in it.

## Build Targets
If building tests, `tools/autotest.py` should build the appropriate test on your
behalf. If building a target to run on a device, you should build one of the
following unless directly told otherwise.
  * `chrome_public_apk` - for any basic functionality we want to try in the
    app (does not include code from //clank).
  * `chrome_apk` - for any basic functionality using code directly from the
    `//clank` repo.
  * `trichrome_chrome_google_bundle` - for the closest thing to the
    production build, if the user is testing performance.

## Installing or Running an APK/Bundle
To install or run an apk/bundle, use the generated wrapper script in
`out/{USERS_OUT_DIR}/bin/`.
  * Installing is done via the `install command` - eg.
    `out/Debug/bin/chrome_public_apk install`.
  * "Launch" installs and starts the app - eg.
    `out/Release/bin/trichrome_chrome_google_bundle launch`.
