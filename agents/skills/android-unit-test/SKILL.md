---
name: android-unit-test
description: >-
  Guide for writing and converting Chromium Android unit tests across host Robolectric (chrome_junit_tests) and on-device (chrome_public_unit_test_apk) suites.
---

# Android Unit Testing Guide

This skill provides comprehensive guidance on choosing, writing, running, and
migrating Android test cases across Chromium's three major Java test tiers
(`chrome_junit_tests`, `chrome_public_unit_test_apk`, and
`chrome_public_test_apk`).

## 1. Test Tier Hierarchy & Choosing the Right Suite

When writing or refactoring Android Java tests in Chromium, always aim for the
lightest, fastest test tier that satisfies the requirements of the code being
tested:

| Target Suite                                                | Execution Environment                                                                       | Best For                                                                                                                                                                                                     | Key Constraints & Rules                                                                                                                                   |
| :---------------------------------------------------------- | :------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`chrome_junit_tests`** (`junit/`)                         | Host JVM via **Robolectric** (`BaseRobolectricTestRunner`)                                  | Pure Java utility methods, data structures, MVC/MVC mediators (`*MediatorUnitTest.java`), presenter logic, string manipulation, and mock-based testing.                                                      | Fastest execution. No actual hardware/emulator required. Cannot execute real native NDK code or live browser profiles without shadow structures or stubs. |
| **`chrome_public_unit_test_apk`** (`unit_device_javatests`) | Device / Emulator (`BaseJUnit4ClassRunner` + `@Batch(Batch.UNIT_TESTS)`)                    | Unit tests requiring actual Android device features (such as `PackageManager` checks, real layout inflating/rendering with `BlankUiTestActivity`, or localized JNI helper methods like `CurrencyFormatter`). | Configured with `is_unit_test = true`: any attempts to initialize the main browser process or start `ChromeTabbedActivity` will explicitly fail.          |
| **`chrome_public_test_apk`** (`javatests/`)                 | Device / Emulator Instrumentation (`ChromeTabbedActivityTestRule`, `BaseJUnit4ClassRunner`) | Full end-to-end multi-component browser profile initialization, web page interactions (`startOnWebPage`, `embeddedTestServer`), and complex UI flows across Activities.                                      | Heaviest and slowest tier. Should be reserved exclusively for full browser activity & integration flows.                                                  |

______________________________________________________________________

## 2. Key Rules for Converting & Migrating Tests

When converting instrumentation tests from `chrome_public_test_apk`
(`javatests/`) to host unit tests (`chrome_junit_tests` / `junit/`) or on-device
unit tests (`chrome_public_unit_test_apk`), follow these key rules:

1. **No `@Batch` in `chrome_junit_tests`:** Robolectric host unit tests on the
   JVM are naturally batched by default; do **NOT** add `@Batch` annotations to
   tests under `junit/`.
2. **No `@UiThreadTest` in `chrome_junit_tests`:** Robolectric tests execute on
   the main looper thread automatically. Strip
   `import androidx.test.annotation.UiThreadTest;` and method-level
   `@UiThreadTest` annotations completely when converting to
   `chrome_junit_tests`.
3. **Native Code Dependencies:** If a unit test uses real NDK native methods
   (such as `CurrencyFormatter` in `PriceUtils.formatPrice`) that cannot be
   mocked, place it in `chrome_public_unit_test_apk` (`unit_device_javatests`)
   instead of `chrome_junit_tests`.

______________________________________________________________________

## 3. Step-by-Step Conversion Workflows

### Converting to Host Unit Tests (`chrome_junit_tests`)

1. **File Location:** Move the file from `javatests/src/...` to `junit/src/...`
   via `git mv`.
2. **Test Runner:** Replace `@RunWith(BaseJUnit4ClassRunner.class)` or
   `ChromeJUnit4ClassRunner` with:
   ```java
   import org.chromium.base.test.BaseRobolectricTestRunner;

   @RunWith(BaseRobolectricTestRunner.class)
   ```
3. **Clean Up Annotations:** Remove `@Batch(...)` and `@UiThreadTest`.
4. **Simplify Threading & Mocks:** Replace complex calls like
   `ThreadUtils.runOnUiThreadBlocking(() -> Mockito.mock(...))` with direct
   Mockito initialization: `Mockito.mock(...)`.
5. **Update `BUILD.gn`:** Move the test source out of the `javatests` target
   into a `robolectric_library("junit")` target:
   ```gn
   robolectric_library("junit") {
     testonly = true
     sources = [
       "junit/src/org/chromium/chrome/browser/mycomponent/MyComponentUnitTest.java",
     ]
     deps = [
       ":java",
       "//base:base_java_test_support",
       "//base:base_junit_test_support",
       "//third_party/androidx:androidx_test_runner_java",
       "//third_party/junit",
       "//third_party/mockito:mockito_java",
     ]
   }
   ```

### Converting to On-Device Unit Tests (`chrome_public_unit_test_apk`)

- **Runner & Annotations:** Keep `@RunWith(BaseJUnit4ClassRunner.class)` along
  with `@Batch(Batch.UNIT_TESTS)` or `@Batch(Batch.PER_CLASS)`.
- **Target Setup in `BUILD.gn`:** Define a dedicated `unit_device_javatests`
  target within your component and register it inside
  `chrome_public_unit_test_apk` (`//chrome/android/BUILD.gn`):
  ```gn
  android_library("unit_device_javatests") {
    testonly = true
    resources_package = "org.chromium.chrome.browser.mycomponent"
    sources = [ "javatests/src/org/chromium/chrome/browser/mycomponent/MyComponentDeviceUnitTest.java" ]
    deps = [
      ":java",
      "//base:base_java",
      "//base:base_java_test_support",
      "//third_party/androidx:androidx_test_runner_java",
      "//third_party/junit",
    ]
  }
  ```

______________________________________________________________________

## 4. Compilation & Execution Workflow

Before running tests locally, **always compile the specific target first**.

### Host JVM Tests (`chrome_junit_tests`)

```bash
autoninja -C out/Debug chrome_junit_tests && \
out/Debug/bin/run_chrome_junit_tests -f "*MyComponentUnitTest*"
```

### On-Device Unit Tests (`chrome_public_unit_test_apk`)

Always specify your emulator or device explicitly with `-d`:

```bash
autoninja -C out/Debug chrome_public_unit_test_apk && \
out/Debug/bin/run_chrome_public_unit_test_apk -f "*MyComponentDeviceUnitTest*" -d emulator-5554
```
