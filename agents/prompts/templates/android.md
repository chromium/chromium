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

## Running Tests and Emulators
When no physical Android device is connected, use prebuilt AVD (Android Virtual
Device) configurations. Run `tools/android/avd/avd.py list` to see available
configurations (e.g.
`tools/android/avd/proto/android_35_google_apis_x64.textpb`). See
[`//docs/android_emulator.md`](../../../docs/android_emulator.md) for full
details.
  * **Running tests on an emulator:** test wrapper scripts in
    `out/{USERS_OUT_DIR}/bin/` can automatically start, test against, and tear
    down an emulator instance using `--avd-config` - eg.
    `out/Debug/bin/run_base_unittests --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb`.
    As a faster alternative when an emulator is already running, specify which
    emulator to use with `-d emulator-5554`.
  * **Running a standalone emulator:** to start an emulator independently when
    installing or launching an APK - eg.
    `tools/android/avd/avd.py start --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb`.
  * **Stopping the emulator:** to terminate a running emulator, use
    `tools/android/avd/avd.py stop` - eg.
    `tools/android/avd/avd.py stop --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb`
    (or omit `--avd-config` to stop all running emulators).
