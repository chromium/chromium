# Adding New XR Instrumentation Tests

## Introduction

This is a brief overview of general steps to adding new XR instrumentation
tests. If you want to add tests as fast as possible, keep reading and glance
through some existing tests, which should give you enough information to start
writing your own.

If you want to better understand what's going on under the hood or why we do
certain things, take a look at
[`xr_instrumentation_deep_dive.md`][xr_instrumentation_deep_dive].

### An Overview Of XR Test Frameworks

Pretty much all XR instrumentation tests with the exception of some VR Browser
tests interact with asynchronous (Promise based) JavaScript code. This is where
the XR Test Frameworks come in, with test classes defining `mXyzTestFramework`
for testing feature Xyz. Together with some JavaScript imports in your test's
HTML file, these allow you to run tests as a series of synchronous steps that
alternate between JavaScript and Java.

For a concrete example, take a look at
[`WebXrGvrTransitionTest`][webxr_vr_transition_test]'s
`testNonImmersiveStopsDuringImmersive` test and its corresponding HTML file
[test_non_immersive_stops_during_immersive.html][webxr_vr_transition_test_html].

The general flow in tests will be:

1. Load the HTML file with loadFileAndAwaitInitialization - this ensures that any
   pre-test setup in JavaScript is completed.
2. Run some code on Java's side.
3. Trigger some JavaScript code and wait for it to signal that it is finished.
   These can be identified as the `*AndWait` methods, and stop blocking once the
   JavaScript side calls `finishJavaScriptStep()`.
4. Repeat from 2 until done.
5. End the test.

## Adding Tests To Existing Test Classes

If you're adding a new test to an existing test class, all the per-class
boilerplate code should be around already, so you can get right to adding a new
test case using the following general components.

### Annotations

The following annotations can be applied before your test body to modify its
behavior.

#### @Test

Every test method must be annotated with the `@Test` annotation in order for the
test runner to identify it as an actual test.

#### Test Length

Every test method must also be annotated with a test length annotation,
typically `@MediumTest`. Eventually, the test length annotations should imply
the presence of `@Test`, but both must currently be present.

#### Supported Activities

Unless your test uses the VR Browser, you can use the `@XrActivityRestriction`
annotation to automatically run your test multiple times in different supported
activities. The currently supported activities are:

* ChromeTabbedActivity (regular Chrome)
* CustomTabActivity (used to open links in apps like GMail)
* WebappActivity (used for Progressive Webapps)

#### @Restriction

You can restrict your test or test class to only be run under certain
circumstances, such as only on Daydream-ready devices or only with the Daydream
View headset paired, using the `@Restriction` annotation.

#### Command Line Flags

You can add or remove command line flags that are set before the test runs using
`@CommandLineFlags.Add` and `@CommandLineFlags.Remove`. Note that if you want to
override a flag set by the test class on a per-test basis, you must remove and
re-add it.

### Test Body

#### HTML Test File

You will likely need an HTML file to load during your test, which should be
placed in `//chrome/test/data/xr/e2e_test_files/html`. The exact contents of
your file will depend on your test, but you will likely be importing some or all
of the following scripts from `//chrome/test/data/xr/e2e_test_files/resources`:

* `webxr_e2e.js` - Sets up the necessary code to communicate back
  and forth between Java and JavaScript
* `webxr_boilerplate.js` - Handles the WebXR and WebVR
  boilerplate code, such as getting an XRDevice and setting up a canvas.

Additionally, in order to use asserts in JavaScript, you must import
`//third_party/WebKit/LayoutTests/resources/testharness.js`.

#### Java Test Body

The exact contents of your test body are going to depend on the test you're
trying to write, so just keep the following guidelines in mind:

* Use the most specific version of a class as possible, e.g. use
`WebXrArTestFramework` for WebXR for AR testing instead of `WebXrTestFramework`.
* If you need to do something that involves the webpage/web contents, it's
  likely available through your test framework.
* If you need to do something that doesn't involve the webpage/web contents,
  it's likely available in one of the classes in `util/`.

## Adding A New Test Class

If you're adding a new test class instead of just adding a new test to an
existing class, there are a few additional bits of boilerplate code you will
need to add before being able to write your test.

### Test Parameterization

Test parameterization is how running a test multiple times in different
activities is handled. However, it adds some amount of overhead to test runtime.

See [`WebXrGvrTransitionTest`][webxr_vr_transition_test] for an example of a
parameterized class. The general things you will need to are:

* Set `@RunWith` to `ParameterizedRunner.class`.
* Add `@UseRunnerDelegate` and set it to `ChromeJUnit4RunnerDelegate.class`.
* Declare `sClassParams` as a static `List` of `ParameterSet`, annotate it with
  `@ClassParameter`, and set it to the value returned by either
  `XrTestRuleUtils.generateDefaultTestRuleParameters()` for AR tests or
  `GvrTestRuleUtils.generateDefaultTestRuleParameters()` for VR tests.
* Declare `mRuleChain` as a `RuleChain` and annotate it with `@Rule`.
* Declare `mTestRule` as a `ChromeActivityTestRule`.
* Declare any necessary test frameworks and initialize them using `mTestRule` in
  a setup method annotated with `@Before`.
* Define a constructor for your test class that takes a
  `Callable<ChromeActivityTestRule>`. This constructor must set `mVrTestRule` to
  the `Callable`'s `call()` return value and set `mRuleChain` to the return
  value of `XrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule)`
  for AR tests or `GvrTestRuleUtils.wrapRuleInActivityRestrictionRule
  (mTestRule)` for VR tests.

### Add The New File

Add the new test class to [`//chrome/android/BUILD.gn`][build_gn]. If it is a VR
test class, it should be added to the `sources` list of the
`chrome_test_vr_java` `android_library` target. If it is an AR test class, it
should be added to the `sources` list of the `chrome_test_ar_java`
`android_library` target.

## AR Playback Datasets

If you are adding an AR test and none of the existing datasets work for it, you
can create and upload a new dataset that fits your needs. Dataset creation
requires some internal tools, see go/arcore-chrome-collect-recordings (internal
link) or contact either bsheedy@ or bialpio@ for instructions.

Once you have your playback dataset (.mp4 file), simply place it in
`//chrome/test/data/xr/ar_playback_datasets/` and upload it using
`upload_to_google_storage.py` to the `chromium-ar-test-apks/playback_datasets`
bucket.


[xr_instrumentation_deep_dive]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/xr_instrumentation_deep_dive.md
[webxr_vr_transition_test]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/WebXrGvrTransitionTest.java
[webxr_vr_transition_test_html]: https://chromium.googlesource.com/chromium/src/+/main/chrome/test/data/xr/e2e_test_files/html/test_non_immersive_stops_during_immersive.html
[vr_browser_transition_test]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/VrBrowserTransitionTest.java
[build_gn]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/BUILD.gn
