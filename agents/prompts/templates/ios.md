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

To run unit tests inside `//ios/chrome`, you can use the following.

```sh
out/Debug-iphonesimulator/iossim -d "iPhone 16 Pro" \
  -c "--gtest_filter=ExampleTest1.*:ExampleTest2.*" \
  out/Debug-iphonesimulator/ios_chrome_unittests.app
```

## Running EG Tests

To run EG tests inside `ios_chrome_ui_eg2tests_module` without building, you can
use the following.

```sh
xcodebuild test-without-building \
  -project out/build/all.xcodeproj \
  -scheme ios_chrome_ui_eg2tests_module \
  -destination "platform=iOS Simulator,name=iPhone 16 Pro" \
  -only-testing:"ios_chrome_ui_eg2tests_module/ExampleTestCase/testExample"
```