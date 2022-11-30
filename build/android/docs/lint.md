# Lint

Android's [**lint**](https://developer.android.com/tools/help/lint.html) is a
static analysis tool that Chromium uses to catch possible issues in Java code.

This is a list of [**checks**](http://tools.android.com/tips/lint-checks) that
you might encounter.

[TOC]

## How Chromium uses lint

Chromium only runs lint on apk or bundle targets that explicitly set
`enable_lint = true`. Some example targets that have this set are:

 - `//chrome/android:monochrome_public_bundle`
 - `//android_webview/support_library/boundary_interfaces:boundary_interface_example_apk`
 - `//remoting/android:remoting_apk`

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

The following are some common scenarios where you may need to update baseline
files.

### I updated `cmdline-tools` and now there are tons of new errors!

This happens every time lint is updated, since lint is provided by
`cmdline-tools`.

Baseline files are defined via the `lint_baseline_file` gn variable. It is
usually defined near a target's `enable_lint` gn variable. To regenerate the
baseline file, delete it and re-run the lint target. The command will fail, but
the baseline file will have been generated.

This may need to be repeated for all targets that have set `enable_lint = true`,
including downstream targets. Downstream baseline files should be updated and
first to avoid build breakages. Each target has its own `lint_baseline_file`
defined and so all these files can be removed and regenerated as needed.

### I updated `library X` and now there are tons of new errors!

This is usually because `library X`'s aar contains custom lint checks and/or
custom annotation definition. Follow the same procedure as updates to
`cmdline-tools`.
