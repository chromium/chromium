# XR Instrumentation Test Deep Dive

## Introduction

This documentation aims to provide a more in-depth view of how various aspects
of the XR instrumentation tests work under the hood and explain why certain
design decisions were made. If you just want an overview on how to build/run the
tests or add new ones, see [`README.md`][readme] and
[`adding_new_tests.md`][adding_new_tests].

## Naming Convention

Classes, files, and variables are named using the following (hopefully
self-explanatory) naming convention:

* `XR`/`Xr` - The most general term. Used for things that are applicable to any
  XR feature.
* `VR`/`Vr` - A subset of `XR`. Used for things that are applicable to any VR
  feature.
* `AR`/`Ar` - A subset of `XR`. Used for things that are applicable to any AR
  feature.
* `WebXR`/`WebXr` - A subset of `XR`. Used for things that are applicable to any
  XR-related web APIs.
* `WebXR for AR`/`WebXrAr` - A subset of `WebXR` and `AR`. Used for things that
  are applicable to the `WebXR Devices API` in its AR usecase.
* `WebXR for VR`/`WebXrVr` - A subset of `WebXR` and `VR`. Used for things that
  are applicable to the `WebXR Devices API` in its VR usecase.

## Test Framework Structure

### Hierarchy

Based on the above naming scheme, the various `TestFramework` classes are
structured in the following hierarchy:

* `XrTestFramework`
  * `WebXrTestFramework`
    * `WebXrArTestFramework`
    * `WebXrVrTestFramework`

### Static vs. Non-Static Methods

Most methods in the `TestFramework` classes have both a static and non-static
version, with the non-static version simply calling the static one with the
framework's `mFirstTabWebContents` reference. This is because the vast majority
of use cases are interacting with the web page that is in the current tab, but
some rare cases require interacting with other tabs. Thus, we need to provide a
way of using the frameworks with arbitrary tabs/`WebContents`
(the static methods), but offering the non-static versions cuts down the clutter
of calling `getCurrentWebContents()` everywhere.

## Parameterization

Parameterization is a JUnit4 concept. You can read the official guide about it
on the [JUnit4 GitHub Wiki][junit4_wiki_parameterization], but the TL;DR is that
it allows a test method or class to be automatically run multiple times with
varying inputs.

When used in XR tests, the `List` of `ParameterSet`s annotated with
`@ClassParameter` is what will be iterated over. Specifically,
`XrTestRuleUtils.generateDefaultTestRuleParameters` or
`GvrTestRuleUtils.generateDefaultTestRuleParameters` will generate a `List` of
`ParameterSet`s each containing a single `Callable` whose `call()` returns a
`ChromeActivityTestRule`. Each `ParameterSet` corresponds to one of the activity
types that XR features are supported in. This is why constructors of
parameterized test classes must accept a `Callable<ChromeActivityTestRule>` -
they will be called once per element of the `ParameterSet` `List` and passed the
contents of that element.

`Callable` is used instead of `ChromeActivityTestRule` directly due to an
implementation detail in `ParameterSet` ([source code][parameter_set_source]).
`ParameterSet` only accepts primitives, `Callable`s, and a certain set of
`Class`es such as `String`, which means we can't pass a `ChromeActivityTestRule`
directly to a `ParameterSet`.

## Rules

Rules are another JUnit4 concept. You can read a fairly comprehensive guide
about them on the [JUnit4 GitHub Wiki][junit4_wiki_rules], but the TL;DR of them
is that they allow pre- and post-test code to be easily shared across multiple
test classes. Any rule annotated with `@Rule` will be automatically applied to
the test in an arbitrary order.

### Activity Restriction Rule

`XrActivityRestrictionRule` and the `@XrActivityRestriction` annotation are what
allow tests to only be run in the activities they support, as otherwise
parameterization would cause all tests to be run in all activities. The method
for this is simple - if the activity type for the current
`ChromeActivityTestRule` is in the list of supported activities provided by
`@XrActivityRestriction`, continue running the test as normal. Otherwise, don't
run the test, and instead, generate a statement that throws an assumption
failure, which the test runner treats as a signal that the test was skipped.

#### RuleChain

Every place where `XrActivityRestrictionRule` is used makes use of a `RuleChain`
and `XrTestRuleUtils.wrapRuleInActivityRestrictionRule()`. The reason for this
is simply optimization. By using a `RuleChain` to wrap a given
`ChromeActivityTestRule` in an `XrActivityRestrictionRule`, we can ensure that
the decision to skip a test due to being unsupported in an activity is made
before we go through the (slow) process of starting said activity.

### XR And VR Test Rules

XR instrumentation tests use special versions of `ChromeActivityTestRule` that
implement `XrTestRule` or `VrTestRule`.

Rules that implement `XrTestRule` simply open the specified activity type to a
blank page and implement a method that allows it to work with
`XrActivityRestrictionRule`.

Rules that implement `VrTestRule` do the same, but also perform some additional
VR-specific setup such as ensuring that the test is not started in VR.

## Dynamic VrCore Settings

Most tests simply use whatever VrCore settings are set via the
`--shared-prefs-file` option before testing starts. This is normally beneficial
since only having to apply settings once per test suite is more efficient.
However, since `--shared-prefs-file` is per-test-suite, it's less desirable for
tests that need uncommon settings (e.g. DON flow enabled) and completely
unusable by itself if a test needs to change VrCore settings mid-test.

Thus, tests that use less common settings instead use a shared preference file
that enables VrCore's VR settings service and apply settings on a per-test
basis using the service. These tests must be kept separate from the regular
tests that don't use dynamic settings because the setting changes are permanent.
So, unless all or none of the tests in a given run use dynamic settings, we risk
running tests with incorrect VrCore settings.

Skipping over these tests is done using an `@Restriction` annotation instead of
a rule like for test parameterization because skipping via `@Restriction` is
faster.

Why not use this for all tests? Since many tests are run twice, once with
Cardboard and once with Daydream View, we would need to use parameterization and
annotations for supported settings types to run with. Add in additional, less
commonly used settings types, and you could end up with most tests being tried
in three different activity types and something like five different settings
types, for a total of fifteen attempted runs. While most of these would end up
being skipped, starting the rule application process does have some overhead, so
the end result would be a noticeable increase in test run time.

## VR Controller Input

There are currently two ways of injecting Daydream controller input into tests,
each with their own pros and cons.

### EmulatedGvrController

The `EmulatedGvrController` class is the older of the two approaches and works by
setting VrCore to accept Android `Intent`s as controller input instead of using
an actual controller.

The main benefit of this is that it allows complete end-to-end
testing of controller-related Chrome code, as it still receives controller input
from VrCore the same as if a real controller was in use. It also allows the use
of the home button to recenter the view or go to Daydream Home.

A downside to this downside is that `Intent`s do not allow for precise timing,
leading to flakiness. Additionally, since we're essentially sending raw
quaternions to VrCore to turn into the controller's orientation, pointing at
specific UI elements is tedious and prone to breaking if the UI changes.

### NativeUiUtils

The methods in the `NativeUiUtils` class work by causing Chrome to start reading
controller input from a test-only queue instead of getting data from VrCore.

The downsides to this are that it causes Chrome to use some non-production code
and doesn't test the controller-related interaction with VrCore.

This approach does have quite a few benefits though. First it should not be
flaky while also being faster since everything is handled Chrome-side.
Additionally, it allows interaction with specific UI elements by name, which is
both easier and less prone to breaking than specifying a position in space.


[readme]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/README.md
[adding_new_tests]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/adding_new_tests.md
[junit4_wiki_parameterization]: https://github.com/junit-team/junit4/wiki/parameterized-tests
[parameter_set_source]: https://chromium.googlesource.com/chromium/src/+/main/base/test/android/javatests/src/org/chromium/base/test/params/ParameterSet.java
[junit4_wiki_rules]: https://github.com/junit-team/junit4/wiki/rules
