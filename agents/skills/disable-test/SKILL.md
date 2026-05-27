---
name: disable-test
description: >-
  Guide for disabling Chromium tests (C++ or WebUI).
  Use this skill when you need to disable a failing or flaky test case.
---

# Disabling Chromium Tests

This guide provides instructions on how to disable test cases in Chromium,
covering both regular C++ tests and WebUI tests.

## Step 1: Confirm Disabling Scope

Before disabling a test, determine the scope:

1. **All Platforms:** The test fails everywhere.
2. **Platform Specific:** The test fails only on specific platforms (e.g.,
   Linux, Windows, Mac, ChromeOS).
3. **Configuration Specific:** The test fails only in specific configurations
   (e.g., ASAN, MSAN, Debug builds).

**If the user hasn't specified the scope, ask for confirmation before
proceeding.**

## Step 2: Obtain Bug Reference

**If the user hasn't provided a Buganizer ID (e.g., crbug.com/123456), ask them
for one before proceeding.** Every disabled test MUST be tracked by a bug to
ensure it eventually gets fixed and re-enabled.

## Step 3: Disable Tests

### C++ Tests (GTest)

For regular C++ tests (including unit tests and browser tests), use the
`DISABLED_` prefix.

#### All Platforms

Prefix the test name directly with `DISABLED_`.

```c++
// TODO(crbug.com/123456): Flaky on all platforms.
TEST_F(MyTestFixture, DISABLED_MyTestCase) {
  ...
}
```

#### Platform or Configuration Specific

Use `#if` with `BUILDFLAGs` to define a `MAYBE_` macro.

```c++
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

// TODO(crbug.com/123456): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MyTestCase DISABLED_MyTestCase
#else
#define MAYBE_MyTestCase MyTestCase
#endif
TEST_F(MyTestFixture, MAYBE_MyTestCase) {
  ...
}
```

Common BUILDFLAGs:

- `IS_LINUX`, `IS_WIN`, `IS_MAC`, `IS_CHROMEOS`, `IS_ANDROID`, `IS_IOS`
- `ADDRESS_SANITIZER` (for ASAN), `MEMORY_SANITIZER` (for MSAN)

### WebUI Tests (Mocha)

WebUI tests should preferably be disabled at the Mocha (TypeScript) level. If it
can't be disabled at the Mocha level because the entire suite is flaky or
failing then the C++ suite can be disabled.

Use `test.skip()` or `suite.skip()`.

#### All Platforms

```ts
// TODO(crbug.com/123456): Flaky on all platforms.
test.skip('MyTestcase', function() {
  ...
});
```

#### Platform Specific

Use `<if expr>` in the `.ts` file.

```ts
// TODO(crbug.com/123456): Flaky on Mac.
// <if expr="not is_macosx">
test('MyTestCase', async () => {
  ...
});
// </if>
```

Common expressions: `is_linux`, `is_win`, `is_macosx`, `is_chromeos`,
`is_android`, `is_ios`.

## Step 4: Documentation Requirement

**ALWAYS** include a `TODO` comment with a link to the Buganizer issue obtained
in Step 2.

## Step 5: Validation

1. **Build** the relevant test target (e.g., `unit_tests`, `browser_tests`,
   `interactive_ui_tests`).
2. **Run** the test to ensure it is indeed skipped (it should show as `SKIPPED`
   or `DISABLED` in the output).
   ```sh
   ./out/Default/browser_tests --gtest_filter=MyTestFixture.MyTestCase
   ```
3. **Run Presubmits** to ensure everything is passing.
   ```sh
   git cl presubmit -u --force
   ```
