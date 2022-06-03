# Optimizing Java Code

This doc describes how Java code is optimized in Chrome on Android and how to
deal with issues caused by the optimizer. For tips on how to write optimized
code, see [//docs/speed/binary_size/optimization_advice.md#optimizing-java-code](/docs/speed/binary_size/optimization_advice.md#optimizing-java-code).

[TOC]

## ProGuard vs R8

ProGuard is the original open-source tool used by many Android applications to
perform whole-program bytecode optimization. [R8](https://r8.googlesource.com/r8),
is a re-implementation that is used by Chrome (and the default for Android Studio).
The terms "ProGuard" and "R8" are used interchangeably within Chromium but
generally they're meant to refer to the tool providing Java code optimizations.

## What does ProGuard do?

1. Shrinking: ProGuard will remove unused code. This is especially useful
   when depending on third party libraries where only a few functions are used.

2. Obfuscation: ProGuard will rename classes/fields/methods to use shorter
   names. Obfuscation is used for minification purposes only (not security).

3. Optimization: ProGuard performs a series of optimizations to shrink code
   further through various approaches (ex. inlining, outlining, class merging,
   etc).

## Build Process

ProGuard is enabled only for release builds of Chrome because it is a slow build
step and breaks Java debugging. It can also be enabled manually via the GN arg:
```is_java_debug = false```

### ProGuard configuration files

Most GN Java targets can specify ProGuard configuration files by setting the
`proguard_configs` variable. [//base/android/proguard](/base/android/proguard)
contains common flags shared by most Chrome applications.

### GN build rules

When `is_java_debug = false` and a target has enabled ProGuard, the `proguard`
step generates the `.dex` files for the application. The `proguard` step takes
as input a list of `.jar` files, runs R8/ProGuard on those `.jar` files, and
produces the final `.dex` file(s) that will be packaged into your `.apk`

## Deobfuscation

Obfuscation can be turned off for local builds while leaving ProGuard enabled
by setting `enable_proguard_obfuscation = false` in GN args.

There are two main methods for deobfuscating Java stack traces locally:
1. Using APK wrapper scripts (stacks are automatically deobfuscated)
  * `$OUT/bin/chrome_public_apk logcat`  # Run adb logcat
  * `$OUT/bin/chrome_public_apk run`  # Launch chrome and run adb logcat

2. Using `java_deobfuscate`
  * build/android/stacktrace/java_deobfuscate.py $OUT/apks/ChromePublic.apk.mapping < logcat.txt`
    * ProGuard mapping files are located beside APKs (ex.
      `$OUT/apks/ChromePublic.apk` and `$OUT/apks/ChromePublic.apk.mapping`)

Helpful links for deobfuscation:

* [Internal bits about how mapping files are archived][proguard-site]
* [More detailed deobfuscation instructions][proguard-doc]
* [Script for deobfuscating official builds][deob-official]

[proguard-site]: http://goto.google.com/chrome-android-proguard
[proguard-doc]: http://goto.google.com/chromejavadeobfuscation
[deob-official]: http://goto.google.com/chrome-android-official-deobfuscation

## Debugging common failures

ProGuard failures are often hard to debug. This section aims to outline some of
the more common errors.

### Classes expected to be discarded

The `-checkdiscard` directive can be used to ensure that certain items are
removed by ProGuard. A common use of `-checkdiscard` it to ensure that ProGuard
optimizations do not regress in their ability to remove code, such as code
intended only for debug builds, or generated JNI classes that are meant to be
zero-overhead abstractions. Annotating a class with
[@CheckDiscard][checkdiscard] will add a `-checkdiscard` rule automatically.

[checkdiscard]: /base/android/java/src/org/chromium/base/annotations/CheckDiscard.java

```
Item void org.chromium.base.library_loader.LibraryPrefetcherJni.<init>() was not discarded.
void org.chromium.base.library_loader.LibraryPrefetcherJni.<init>()
|- is invoked from:
|  void org.chromium.base.library_loader.LibraryPrefetcher.asyncPrefetchLibrariesToMemory()
... more code path lines
|- is referenced in keep rule:
|  obj/chrome/android/chrome_public_apk/chrome_public_apk.resources.proguard.txt:104:1

Error: Discard checks failed.
```

Things to check
  * Did you add code that is referenced by code path in the error message?
  * If so, check the original class for why the `CheckDiscard` was added
    originally and verify that the reason is still valid with your change (may
    need git blame to do this).
  * Try the extra debugging steps listed in the JNI section below.

### JNI wrapper classes not discarded

Proxy native methods (`@NativeMethods`) use generated wrapper classes to provide
access to native methods. We rely on ProGuard to fully optimize the generated
code so that native methods aren't a source of binary size bloat. The above
error message is an example when a JNI wrapper class wasn't discarded (notice
the name of the offending class).
  * The ProGuard rule pointed to in the error message isn't helpful (just tells
    us a code path that reaches the not-inlined class).
  * Common causes:
    * Caching the result of `ClassNameJni.get()` in a member variable.
    * Passing a native wrapper method reference instead of using a lambda (i.e.
      `Jni.get()::methodName` vs. `() -> Jni.get.methodName()`).
  * For more debugging info, add to `base/android/proguard/chromium_code.flags`:
      ```
      -whyareyounotinlining class org.chromium.base.library_loader.LibraryPrefetcherJni {
          <init>();
      }
      ```

### Duplicate classes

```
Type YourClassName is defined multiple times: obj/jar1.jar:YourClassName.class, obj/jar2.jar:YourClassName.class
```

Common causes:
  * Multiple targets with overlapping `srcjar_deps`:
    * Each `.srcjar` can only be depended on by a single Java target in any
      given APK target. `srcjar_deps` are just a convenient way to depend on
      generated files and should be treated like source files rather than
      `deps`.
    * Solution: Wrap the `srcjar` in an `android_library` target or have only a
      single Java target depend on the `srcjar` and have other targets depend on
      the containing Java target instead.
  * Accidentally enabling APK level generated files for multiple targets that
    share generated code (ex. Trichrome or App Bundles):
    * Solution: Make sure the generated file is only added once.

Debugging ProGuard failures isn't easy, so please message java@chromium.org
or [file a bug](crbug.com/new) with `component=Build os=Android` for any
issues related to Java code optimization.
