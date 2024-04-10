# Lint

[Android Lint] is [one of the static analysis tools] that Chromium uses to catch
possible issues in Java code.

This is a [list of checks] that you might encounter.

[Android Lint]: https://googlesamples.github.io/android-custom-lint-rules/book.md.html
[one of the static analysis tools]: static_analysis.md
[list of checks]: https://googlesamples.github.io/android-custom-lint-rules/checks/index.md.html

[TOC]

## How Chromium uses lint

Chromium only runs lint on apk or bundle targets that explicitly set
`enable_lint = true`. You can run lint by compiling the apk or bundle target
with ninja; once the code finishes compiling, ninja will automatically run lint
on the code.

Some example targets that have lint enabled are:

 - `//chrome/android:monochrome_public_bundle`
 - `//android_webview/support_library/boundary_interfaces:boundary_interface_example_apk`
 - Other targets with `enable_lint` enabled: https://source.chromium.org/search?q=lang:gn%20enable_lint%5C%20%3D%5C%20true&ss=chromium

If you think lint is not running and already verified your GN
target has `enable_lint = true`, then you can double check that
`android_static_analysis` is set to `"on"` (this is the default value):

```shell
$ gn args out/Default --list=android_static_analysis
android_static_analysis
    Current value (from the default) = "on"
      From //build/config/android/config.gni:85
```

## My code has a lint error

If lint reports an issue in your code, there are several possible remedies.
In descending order of preference:

### Fix it

While this isn't always the right response, fixing the lint error or warning
should be the default.

### Suppress it locally

Java provides an annotation,
[`@SuppressWarnings`](https://developer.android.com/reference/java/lang/SuppressWarnings),
that tells lint to ignore the annotated element. It can be used on classes,
constructors, methods, parameters, fields, or local variables, though usage in
Chromium is typically limited to the first three. You do not need to import it
since it is in the `java.lang` package.

Like many suppression annotations, `@SuppressWarnings` takes a value that tells
**lint** what to ignore. It can be a single `String`:

```java
@SuppressWarnings("NewApi")
public void foo() {
    a.methodThatRequiresHighSdkLevel();
}
```

It can also be a list of `String`s:

```java
@SuppressWarnings({
        "NewApi",
        "UseSparseArrays"
        })
public Map<Integer, FakeObject> bar() {
    Map<Integer, FakeObject> shouldBeASparseArray = new HashMap<Integer, FakeObject>();
    another.methodThatRequiresHighSdkLevel(shouldBeASparseArray);
    return shouldBeASparseArray;
}
```

For resource xml files you can use `tools:ignore`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:tools="http://schemas.android.com/tools">
    <!-- TODO(crbug/###): remove tools:ignore once these colors are used -->
    <color name="hi" tools:ignore="NewApi,UnusedResources">@color/unused</color>
</resources>
```

The examples above are the recommended ways of suppressing lint warnings.

### Suppress it in a `lint-suppressions.xml` file

**lint** can be given a per-target XML configuration file containing warnings or
errors that should be ignored. Each target defines its own configuration file
via the `lint_suppressions_file` gn variable. It is usually defined near its
`enable_lint` gn variable.

These suppressions files should only be used for temporarily ignoring warnings
that are too hard (or not possible) to suppress locally, and permanently
ignoring warnings only for this target. To permanently ignore a warning for all
targets, add the warning to the `_DISABLED_ALWAYS` list in
[build/android/gyp/lint.py](https://source.chromium.org/chromium/chromium/src/+/main:build/android/gyp/lint.py).
Disabling globally makes lint a bit faster.

The exception to the above rule is for warnings that affect multiple languages.
Feel free to suppress those in lint-suppressions.xml files since it is not
practical to suppress them in each language file and it is a lot of extra bloat
to list out every language for every violation in lint-baseline.xml files.

Here is an example of how to structure a suppressions XML file:

```xml
<?xml version="1.0" encoding="utf-8" ?>
<lint>
  <!-- Chrome is a system app. -->
  <issue id="ProtectedPermissions" severity="ignore"/>
  <issue id="UnusedResources">
    <!-- 1 raw resources are accessed by URL in various places. -->
    <ignore regexp="gen/remoting/android/.*/res/raw/credits.*"/>
    <!-- TODO(crbug.com/###): Remove the following line.  -->
    <ignore regexp="The resource `R.string.soon_to_be_used` appears to be unused"/>
  </issue>
</lint>
```

## What are `lint-baseline.xml` files for?

Baseline files are to help us introduce new lint warnings and errors without
blocking on fixing all our existing code that violate these new errors. Since
they are generated files, they should **not** be used to suppress lint warnings.
One of the approaches above should be used instead. Eventually all the errors in
baseline files should be either fixed or ignored permanently.

Most devs do not need to update baseline files and should not need the script
below. Occasionally when making large build configuration changes it may be
necessary to update baseline files (e.g. increasing the min_sdk_version).

Baseline files are defined via the `lint_baseline_file` gn variable. It is
usually defined near a target's `enable_lint` gn variable. To regenerate all
baseline files, run:

```
$ third_party/android_build_tools/lint/rebuild_baselines.py
```

This script will also update baseline files in downstream //clank if needed.
Since downstream and upstream use separate lint binaries, it is usually safe
to simply land the update CLs in any order.
