# Storing preferences

This guide is intended for developers of Chrome for Android who need to read
and/or write small amounts of data from Java to a persistent key-value store.

## SharedPreferencesManager

[`SharedPreferencesManager`][0] is a lightweight wrapper around Android
[SharedPreferences][1] to handle additional key management logic in Chrome. It
supports reading and writing simple key-value pairs to a file that is saved
across app sessions.

## PrefService

[`PrefService`][2] is a JNI bridge providing access to a native Chrome
[PrefService][3] instance associated. This interface can be used to read and
write prefs once they're registered through the `PrefRegistry` and exposed to
Java by way of a `java_cpp_strings` build target such as [this one][4].

## FAQ

**Should I use SharedPreferences or PrefService?**

Ask yourself the following questions about the preference to be stored:

* Will the preference need to be accessed from native C++ code?
* Should the preference be configured as syncable, so that its state can be
  managed by Chrome Sync at Backup and Restore?
* Does the preference need a managed policy setting?

If the answer to one or more of the above questions is Yes, then the preference
should be stored in PrefService. If the answer to all of the above questions is
No, then SharedPreferences should be preferred.

**What if the PrefService type I need to access is not supported by
PrefService (i.e. double, Time, etc.)?**

If a base value type is supported by PrefService, then PrefService should
be extended to support it once it's needed.

**How do I access a PrefService pref associated with local state rather than
browser profile?**

Most Chrome for Android preferences should be associated with a specific
profile. If your preference should instead be associated with [local state][5]
(for example, if it is related to the First Run Experience), then you should not
use PrefService and should instead create your own feature-specific JNI
bridge to access the correct PrefService instance (see [`first_run_utils.cc`][6]).

**Can I move a pref from SharedPreferences to PrefService?**

In general, SharedPreferences are not exposed to C++. There is limited
support in [`shared_preferences_migrator_android.h`][7] for reading,
writing, and clearing values from SharedPreferences.



[0]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/preferences/android/java/src/org/chromium/chrome/browser/preferences/SharedPreferencesManager.java
[1]: https://developer.android.com/reference/android/content/SharedPreferences
[2]: https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/android/java/src/org/chromium/components/prefs/PrefService.java
[3]: https://chromium.googlesource.com/chromium/src/+/main/services/preferences/README.md
[4]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/preferences/BUILD.gn;drc=4ae1b7be67cd9b470ebcc90f2747a9f31f155b00;l=28
[5]: https://www.chromium.org/developers/design-documents/preferences#TOC-Introduction
[6]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/first_run/android/first_run_utils.cc
[7]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/android/preferences/shared_preferences_migrator_android.h
