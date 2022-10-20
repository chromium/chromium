# Feature Flags and Switches

## Overview

This module provides an API to check flags and switches from Java code.

Feature flags and switches are used to control application behavior. They are
extensive described in the
[Configuration Documentation](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/configuration.md).

## Feature Flags

Feature flags are declared in C++ as a `base::Feature`. How to declare them is
covered in
[Adding a new feature flag](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/how_to_add_your_feature_flag.md).

To check the flag state in native code, call
`FeatureList::IsEnabled(kMyFeature))`.

To check the flag state in Java code, first export them to Java:

1.  Put a pointer to the `base::Feature` into `kFeaturesExposedToJava` in
    [`chrome_feature_list.cc`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/chrome_feature_list.cc)
2.  Create a String constant in
    [`ChromeFeatureList.java`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java)
    with the the flag name (passed as parameter to the `base::Feature`
    constructor) as value.

Then, from the Java code, check the value of the flag by calling
[`ChromeFeatureList.isEnabled(ChromeFeatureList.MY_FEATURE)`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java).

Note that this call requires native to be initialized. If native might not be
initialized when the check is run, there are two ways of proceeding:

1.  Use a cached value of the flag from the previous run by calling
    [`ChromeFeatureList.sMyFeature.isEnabled()`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/CachedFlag.java).
    This caching is only done for a subset of the flags. To register your flag,
    create a CachedFlag object in ChromeFeatureList and add it to
    [`ChromeCachedFlags.cacheNativeFlags()`](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/app/flags/ChromeCachedFlags.java)
2.  Check
    [`FeatureList.isInitialized()`](https://cs.chromium.org/chromium/src/base/android/java/src/org/chromium/base/FeatureList.java)
    before calling `ChromeFeatureList.isEnabled()`, and handle the case where
    native is not initialized.

## Switches

A switch is just a string, unlike feature flags, which are a `base::Feature`
objects. Switches are declared separately in native and in Java, though both
[`base::CommandLine`](https://cs.chromium.org/chromium/src/base/command_line.h)
in native are
[`CommandLine`](https://cs.chromium.org/chromium/src/base/android/java/src/org/chromium/base/CommandLine.java)
in Java return the same state.

To create a switch in Native, declare it as a `const char kMySwitch =
"my-switch"` and call
`base::CommandLine::ForCurrentProcess()->HasSwitch(kMySwitch)`.

To create a switch in Java, add it to
[ChromeSwitches.java.tmpl](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java_templates/ChromeSwitches.java.tmpl).
It will automatically be surfaced in the generated
[ChromeSwitches.java](https://cs.chromium.org/chromium/src/out/android-Debug/gen/chrome/browser/flags/java/generated_java/input_srcjars/org/chromium/chrome/browser/flags/ChromeSwitches.java?q=DISABLE_FULLSCREEN&dr=CSs).
Then, check it with
`CommandLine.getInstance().hasSwitch(ChromeSwitches.MY_SWITCH)`.

For switches used in both native and Java, simply declare them twice,
separately, as per instructions above, with the same string.

## Variations

Though feature flags are boolean, enabled feature flags can have multiple
variations of the same feature. The code generates these variations by getting
parameters from the field trial API.

In native, the field trial API can be accessed by the functions in
[field_trial_params.h](https://cs.chromium.org/chromium/src/base/metrics/field_trial_params.h),
passing the `base::feature`. For example,
`GetFieldTrialParamByFeatureAsInt(kMyFeature, "MyParameter", 0)` will return the
value that should be used for the parameter `"MyParameter"` of `kMyFeature`. If
not available the default value `0` is returned.

In Java,
[`ChromeFeatureList`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java)
offers the same API with
`ChromeFeatureList.getFieldTrialParamByFeatureAsInt(ChromeFeatureList.MY_FEATURE,
"MyParameter", 0)`. As with `isEnabled()`, this call requires native to be
started. If that is not guaranteed, the same options are available:

1.  Use a cached value of the parameter from the previous run. To do so, declare
    a
    [`static final IntCachedFieldTrialParameter`](https://cs.chromium.org/chromium/src/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/IntCachedFieldTrialParameter.java)
    MY_CACHED_PARAM object and call `MY_CACHED_PARAM.getValue()`. Again, this
    caching is only done for a subset of the parameters. To register yours, add
    it to
    [`ChromeCachedFlags.cacheNativeFlags()`](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/app/flags/ChromeCachedFlags.java)
2.  Check
    [`FeatureList.isInitialized()`](https://cs.chromium.org/chromium/src/base/android/java/src/org/chromium/base/FeatureList.java)before
    calling `ChromeFeatureList.getFieldTrialParamByFeatureAsInt()`, and handle
    the case where native is not initialized.

`Int` used as an example, but parameters can also be of the types `String`,
`Double` and `Boolean`.
