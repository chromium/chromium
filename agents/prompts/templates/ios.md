# Chrome for iOS Instructions

You are building specifically for **Chrome for iOS**. Most of the time your
changes will touch folders with `/ios/` in their path e.g. the folder `//ios/`
at the root of the repository. We **rarely** have to make changes outside that
scope but this can happen when the code to change is shared across platforms.

## Source Files

Source files in **Chrome for iOS** are usually **Objective-C++**, so there are
C++ classes and Objective-C classes interoperating often in the same file. File
names use snake case and end with `.h` file extension for header files or `.mm`
extension for source files. Class names use CamelCase.

If you need to create files, the following rules must be followed, unless stated
otherwise.

  * A class `Foo` must have `foo.h` as header file and `foo.mm` as source file.
  * Unit test for class `Foo` must be implemented in a file `foo_unittest.mm`.
  * EG tests for feature `Bar` must be implemented in a file `bar_egtest.mm`.

## Output Directories

On iOS, the following output directories are often used.

  * `Debug-iphonesimulator` - for a debug build running in iOS simulator.
  * `Debug-iphoneos` - for a debug build running in iOS device.

See the content of `//out/` for the full list of available output directories.

## Build Targets

If you need to build, here is a list of targets often used on iOS.

  * `chrome` - for the main Chrome app in iOS.
  * `ios_chrome_unittests` - for unit tests inside `//ios/chrome/`.
  * `components_unittests` - for unit tests inside `//components/`.
  * `ios_chrome_ui_eg2tests_module` - for most EG tests.

If you need to build `ios_chrome_ui_eg2tests_module` EG tests, do the following.

```sh
autoninja --quiet -C out/Debug-iphonesimulator ios_chrome_ui_eg2tests_module
```

## Running Unit Tests

To build and run unit tests inside `//ios/chrome`, use the `ios/tools/run_unittests.py` script.

```sh
# Run specific tests using gtest_filter
ios/tools/run_unittests.py --gtest_filter="ExampleTest1.*:ExampleTest2.*"

# Run tests on a specific device
ios/tools/run_unittests.py --gtest_filter="ExampleTest1.*" --device="iPhone 16 Pro"

# Run tests on a specific OS version
ios/tools/run_unittests.py --gtest_filter="ExampleTest1.*" --device="iPhone 15" --os="17.5"
```

## Running EG Tests

To run EG tests, use the `ios/tools/run_egtests.py` script. This script handles
building and running the tests on a simulator.

```sh
# Run a specific test case on the default simulator
ios/tools/run_egtests.py --tests=ExampleTestCase/testExample

# Run a test on a specific device
ios/tools/run_egtests.py --tests=ExampleTestCase/testExample --device="iPhone 16 Pro"

# Run a test on a specific device and OS version
ios/tools/run_egtests.py --tests=ExampleTestCase/testExample --device="iPhone 15" --os="17.5"
```
