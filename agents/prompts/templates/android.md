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

## JNI
Chrome on Android uses both Java and C++ frequently. Our JNI uses codegen from
`//third_party/jni_zero`.

Identifying JNI methods:
  * In Java, methods annotated with `@CalledByNative` are called by C++.
    * In C++, these appear as methods with a "Java_" prefix.
  * In Java, methods within interfaces annotated with `@NativeMethods` are calls
    into C++.
    * In C++, these appear as methods with a "JNI_" prefix.
    * If the first parameter is of type `long` and has a prefix of "native",
      then it maps to a method of the same name on the C++ class that comes
      after the "native" prefix. Eg. `void foo(long nativeBarImpl)` means you
      need to find `BarImpl::Foo` in C++.

Finding JNI methods:
  * To find the Java side of JNI: search for the file named `{JavaClass}.java`
    (where {JavaClass} is taken from the `#include "{JavaClass}_jni.h"`.
  * To find the C++ side of JNI: Search for the text `{JavaClass}_jni.h` in C++
    files (where {JavaClass} is taken from the .java file name).

When making changes to JNI methods, always make changes to both the .java and
.cc/.h files involved.
