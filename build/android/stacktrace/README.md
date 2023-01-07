# java_deobfuscate.py

A wrapper around ProGuard's ReTrace tool, which:

1) Updates the regular expression used to identify stack lines, and
2) Streams its output.

The second point here is what allows you to run:

    adb logcat | build/android/stacktrace/java_deobfuscate.py out/Default/apks/ChromePublic.apk.mapping

And have it actually show output without logcat terminating.


## Update Instructions:

    ninja -C out/Release java_deobfuscate_java
    cp out/Release/lib.java/build/android/stacktrace/java_deobfuscate_java.jar build/android/stacktrace

# stackwalker.py

Extracts Breakpad microdumps from a log file and uses `stackwalker` to symbolize
them.


# crashpad_stackwalker.py

Fetches Crashpad dumps from a given device, walks and symbolizes the stacks.
