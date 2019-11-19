# Class Verification Failures

[TOC]

## What's this all about?

This document aims to explain class verification on Android, how this can affect
app performance, how to identify problems, and chromium-specific solutions. For
simplicity, this document focuses on how class verification is implemented by
ART, the virtual machine which replaced Dalvik starting in Android Lollipop.

## What is class verification?

The Java language requires any virtual machine to _verify_ the class files it
loads and executes. Generally, verification is extra work the virtual machine is
responsible for doing, on top of the work of loading the class and performing
[class initialization][1].

A class may fail verification for a wide variety of reasons, but in practice
it's usually because the class's code refers to unknown classes or methods. An
example case might look like:

```java
public class WindowHelper {
    // ...
    public boolean isWideColorGamut() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            return mWindow.isWideColorGamut();
        }
        return false;
    }
}
```

### Why does that fail?

In this example, `WindowHelper` is a helper class intended to help callers
figure out wide color gamut support, even on pre-OMR1 devices. However, this
class will fail class verification on pre-OMR1 devices, because it refers to
[`Window#isWideColorGamut()`][2] (new-in-OMR1), which appears to be an undefined
method.

### Huh? But we have an SDK check!

SDK checks are completely irrelevant for class verification. Although readers
can see we'll never call the new-in-OMR1 API unless we're on >= OMR1 devices,
the Oreo version of ART doesn't know `isWideColorGamut()` was added in next
year's release. From ART's perspective, we may as well be calling
`methodWhichDoesNotExist()`, which would clearly be unsafe.

All the SDK check does is protect us from crashing at runtime if we call this
method on Oreo or below.

### Class verification on ART

While the above is a mostly general description of class verification, it's
important to understand how the Android runtime handles this.

Since class verification is extra work, ART has an optimization called **AOT
("ahead-of-time") verification**¹. Immediately after installing an app, ART will
scan the dex files and verify as many classes as it can. If a class fails
verification, this is usually a "soft failure" (hard failures are uncommon), and
ART marks the class with the status `RetryVerificationAtRuntime`.

`RetryVerificationAtRuntime`, as the name suggests, means ART must try again to
verify the class at runtime. ART does so the first time you access the class
(right before class initialization/`<clinit>()` method). However, depending on
the class, this verification step can be very expensive (we've observed cases
which take [several milliseconds][3]). Since apps tend to initialize most of
their classes during startup, verification significantly increases startup time.

Another minor cost to failing class verification is that ART cannot optimize
classes which fail verification, so **all** methods in the class will perform
slower at runtime, even after the verification step.

*** aside
¹ AOT _verification_ should not be confused with AOT _compilation_ (another ART
feature). Unlike compilation, AOT verification happens during install time for
every application, whereas recent versions of ART aim to apply AOT compilation
selectively to optimize space.
***

## Chromium's solution

In Chromium, we try to avoid doing class verification at runtime by
manually out-of-lining all Android API usage like so:

```java
public class ApiHelperForOMR1 {
    public static boolean isWideColorGamut(Window window) {
        return window.isWideColorGamut();
    }
}

public class WindowHelper {
    // ...
    public boolean isWideColorGamut() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            return ApiHelperForOMR1.isWideColorGamut(mWindow);
        }
        return false;
    }
}
```

This pushes the class verification failure out of `WindowHelper` and into the
new `ApiHelperForOMR1` class. There's no magic here: `ApiHelperForOMR1` will
fail class verification on Oreo and below, for the same reason `WindowHelper`
did previously.

The key is that, while `WindowHelper` is used on all API levels, it only calls
into `ApiHelperForOMR1` on OMR1 and above. Because we never use
`ApiHelperForOMR1` on Oreo and below, we never load and initialize the class,
and thanks to ART's lazy runtime class verification, we never actually retry
verification. **Note:** `list_class_verification_failures.py` will still list
`ApiHelperFor*` classes in its output, although these don't cause performance
issues.

### Creating ApiHelperFor\* classes

There are several examples throughout the code base, but such classes should
look as follows:

```java
/**
 * Utility class to use new APIs that were added in O_MR1 (API level 27).
 * These need to exist in a separate class so that Android framework can successfully verify
 * classes without encountering the new APIs.
 */
@VerifiesOnOMR1
@TargetApi(Build.VERSION_CODES.O_MR1)
public class ApiHelperForOMR1 {
    private ApiHelperForOMR1() {}

    // ...
}
```

* `@VerifiesOnO_MR1`: this is a chromium-defined annotation to tell proguard
  (and similar tools) not to inline this class or its methods (since that would
  defeat the point of out-of-lining!)
* `@TargetApi(Build.VERSION_CODES.O_MR1)`: this tells Android Lint it's OK to
  use OMR1 APIs since this class is only used on OMR1 and above. Substitute
  `O_MR1` for the [appropriate constant][4], depending when the APIs were
  introduced.
* Don't put any `SDK_INT` checks inside this class, because it must only be
  called on >= OMR1.

## Investigating class verification failures

Class verification is generally surprising and nonintuitive. Fortunately, the
ART team have provided tools to investigate errors (and the chromium team has
built helpful wrappers).

### Listing failing classes

The main starting point is to figure out which classes fail verification (those
which ART marks as `RetryVerificationAtRuntime`). This can be done for **any
Android app** (it doesn't have to be from the chromium project) like so:

```shell
# Install the app first. Using Chrome as an example.
autoninja -C out/Default chrome_public_apk
out/Default/bin/chrome_public_apk install

# List all classes marked as 'RetryVerificationAtRuntime'
build/android/list_class_verification_failures.py --package="org.chromium.chrome"
W    0.000s Main  Skipping deobfuscation because no map file was provided.
first.failing.Class
second.failing.Class
...
```

"Skipping deobfuscation because no map file was provided" is a warning, since
many Android applications (including Chrome's release builds) are built with
proguard (or similar tools) to obfuscate Java classes and shrink code. Although
it's safe to ignore this warning if you don't obfuscate Java code, the script
knows how to deobfuscate classes for you (useful for `is_debug = true` or
`is_java_debug = true`):

```shell
build/android/list_class_verification_failures.py --package="org.chromium.chrome" \
  --mapping=<path/to/file.mapping> # ex. out/Release/apks/ChromePublic.apk.mapping
android.support.design.widget.AppBarLayout
android.support.design.widget.TextInputLayout
...
```

Googlers can also download mappings for [official
builds](http://go/clank-webview/official-builds).

### Understanding the reason for the failure

ART team also provide tooling for this. You can configure ART on a rooted device
to log all class verification failures (during installation), at which point the
cause is much clearer:

```shell
# Enable ART logging (requires root). Note the 2 pairs of quotes!
adb root
adb shell setprop dalvik.vm.dex2oat-flags '"--runtime-arg -verbose:verifier"'

# Restart Android services to pick up the settings
adb shell stop && adb shell start

# Optional: clear logs which aren't relevant
adb logcat -c

# Install the app and check for ART logs
adb install -d -r out/Default/apks/ChromePublic.apk
adb logcat | grep 'dex2oat'
...
... I dex2oat : Soft verification failures in boolean org.chromium.content.browser.selection.SelectionPopupControllerImpl.b(android.view.ActionMode, android.view.Menu)
... I dex2oat : boolean org.chromium.content.browser.selection.SelectionPopupControllerImpl.b(android.view.ActionMode, android.view.Menu): [0xF0] couldn't find method android.view.textclassifier.TextClassification.getActions ()Ljava/util/List;
... I dex2oat : boolean org.chromium.content.browser.selection.SelectionPopupControllerImpl.b(android.view.ActionMode, android.view.Menu): [0xFA] couldn't find method android.view.textclassifier.TextClassification.getActions ()Ljava/util/List;
...
```

*** note
**Note:** you may want to avoid `adb` wrapper scripts (ex.
`out/Default/bin/chrome_public_apk install`). These scripts cache the package
manager state to optimize away idempotent installs. However in this case, we
**do** want to trigger idempotent installs, because we want to re-trigger AOT
verification.
***

In the above example, `SelectionPopupControllerImpl` fails verification on Oreo
(API 26) because it refers to [`TextClassification.getActions()`][5], which was
added in Pie (API 28). If `SelectionPopupControllerImpl` is used on pre-Pie
devices, then `TextClassification.getActions()` must be out-of-lined.

## See also

* Bugs or questions? Contact ntfschr@chromium.org
* ART team's Google I/O talks: [2014](https://youtu.be/EBlTzQsUoOw) and later
  years
* Analysis of class verification in Chrome and WebView (Google-only
  [doc](http://go/class-verification-chromium-analysis))
* Presentation on class verification in Chrome and WebView (Google-only
  [slide deck](http://go/class-verification-chromium-slides))

[1]: https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-5.html#jvms-5.5
[2]: https://developer.android.com/reference/android/view/Window.html#isWideColorGamut()
[3]: https://bugs.chromium.org/p/chromium/issues/detail?id=838702
[4]: https://developer.android.com/reference/android/os/Build.VERSION_CODES
[5]: https://developer.android.com/reference/android/view/textclassifier/TextClassification.html#getActions()
