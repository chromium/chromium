---
name: disable-test
description: >-
  Guide for disabling Chromium tests (C++, Android Java, or WebUI).
  Use this skill when you need to disable a failing or flaky test case.
---

# Disabling Chromium Tests

This guide provides instructions on how to disable test cases in Chromium,
covering C++ tests, WebUI tests, Android Java tests, and `.filter` files.

## C++ and WebUI Tests

### Step 1: Verify Failures at HEAD and Confirm Disabling Scope

Before disabling a test (including adding entries to `.filter` files), use the
`luci-test-results` skill
(`third_party/depot_tools/agents/skills/luci-test-results`) to confirm that it
is actively failing or flaking at `HEAD` on the target branch and to determine
the scope:

1. **Check CI test history (`--bucket ci`):**
   ```sh
   vpython3 third_party/depot_tools/agents/skills/luci-test-results/scripts/test_history.py \
     --test-id '<Class#method_or_FULL_ID>' --bucket ci [--builder <BUILDER>] [--project <chromium|chrome>]
   ```
   (ResultDB structured IDs use `#` between fixture/class and method for both
   C++ GTests and Java JUnit tests, e.g. `MyTestFixture#MyTestCase`; or pass
   `--test-substring '<MethodName>'`.) Confirm the test has recent
   failures/flakes on `ci` at `HEAD` on the target builder — not an issue
   already fixed, failing only on a different branch or builder configuration,
   or tryjob noise from CLs under test. (Pass `--raw` if you need a failing
   `<BUILD_ID>` from `"invocationId": "build-<BUILD_ID>"`.)
2. **Fetch a failure log and check for batch/fixture state bleed:**
   ```sh
   vpython3 third_party/depot_tools/agents/skills/luci-test-results/scripts/check_test.py \
     --build-id <BUILD_ID> --test-regex '<TEST_REGEX>'
   vpython3 third_party/depot_tools/agents/skills/luci-test-results/scripts/fetch_log.py \
     --res '<RES_NAME>'
   ```
   Confirm the failure mode matches the bug and is not caused by an earlier test
   in the same `@Batch` class or fixture — either an explicit
   `CascadingFailureException` or silent state bleed from a prior passing test
   (e.g., an unclosed dialog, snackbar, tab, or singleton override; see the
   `android-test-batching` skill). Disabling a victim of state bleed merely
   shifts the failure onto the next test in the batch.
3. **Determine the disabling scope** from the affected builders:
   - **All Platforms:** The test fails everywhere.
   - **Platform Specific:** The test fails only on specific platforms (e.g.,
     Linux, Windows, Mac, ChromeOS, Android).
   - **Configuration Specific:** The test fails only in specific configurations
     (e.g., ASAN, MSAN, Debug builds).

**If the user hasn't specified the scope, ask for confirmation before
proceeding.**

### Step 2: Obtain Bug Reference

**If the user hasn't provided a Buganizer ID (e.g., crbug.com/123456), ask them
for one before proceeding.** Every disabled test MUST be tracked by a bug to
ensure it eventually gets fixed and re-enabled.

### Step 3: Disable Tests

#### C++ Tests (GTest)

For regular C++ tests (including unit tests and browser tests), use the
`DISABLED_` prefix.

##### All Platforms

Prefix the test name directly with `DISABLED_`.

```c++
// TODO(crbug.com/123456): Flaky on all platforms.
TEST_F(MyTestFixture, DISABLED_MyTestCase) {
  ...
}
```

##### Platform or Configuration Specific

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

- `IS_LINUX`, `IS_WIN`, `IS_MAC`, `IS_CHROMEOS`, `IS_ANDROID`,
  `IS_DESKTOP_ANDROID`, `IS_IOS`
- `ADDRESS_SANITIZER` (for ASAN), `MEMORY_SANITIZER` (for MSAN)

#### WebUI Tests (Mocha)

WebUI tests should preferably be disabled at the Mocha (TypeScript) level. If it
can't be disabled at the Mocha level because the entire suite is flaky or
failing then the C++ suite can be disabled.

Use `test.skip()` or `suite.skip()`.

##### All Platforms

```ts
// TODO(crbug.com/123456): Flaky on all platforms.
test.skip('MyTestcase', function() {
  ...
});
```

##### Platform Specific

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

### Step 4: Documentation Requirement

**ALWAYS** include a `TODO` comment with a link to the Buganizer issue obtained
in Step 2, and include `Test history:` and `Failed build:` lines in the CL
description for each disabled test:

```text
Test history: https://ci.chromium.org/ui/test/chromium/<URL_ENCODED_TEST_ID>
Failed build: https://ci.chromium.org/ui/b/<BUILD_ID>
```

### Step 5: Validation

1. **Build** the relevant test target (e.g., `unit_tests`, `browser_tests`,
   `interactive_ui_tests`).
2. **Run the entire test fixture or `@Batch` class** (not just the single
   disabled test) to confirm the disabled test is skipped (`SKIPPED` or
   `DISABLED`) AND that skipping it does not cause sibling tests in the same
   fixture or batch to fail:
   ```sh
   ./out/Default/browser_tests --gtest_filter=MyTestFixture.*
   ```
3. **Run Presubmits** to ensure everything is passing.
   ```sh
   git cl presubmit -u --force
   ```

## Android Java Tests

Follow Steps 1, 2, 4, and 5 above to verify failures at `HEAD`, obtain a bug ID,
include `Test history:` and `Failed build:` in the CL description, and validate
the full test class. Android Java tests use JUnit annotations rather than name
prefixes or preprocessor macros, and the supported annotations depend on whether
the test is a host-side Robolectric test or an on-device instrumentation test.
Every temporary disablement MUST link to a bug (via
`message = "https://crbug.com/<BUG_ID>"` where supported, or a comment
`// https://crbug.com/<BUG_ID>` for annotations like `@DisableIf.Device` that
have no `message` parameter).

### Robolectric Host Tests (`chrome_junit_tests`, etc.)

Only `@DisabledTest` works in Robolectric tests (`@DisableIf` annotations are
**not** evaluated by the Robolectric test runner):

```java
import org.chromium.base.test.util.DisabledTest;

@Test
@DisabledTest(message = "https://crbug.com/123456")
public void testMyTestCase() {
  ...
}
```

### Instrumentation Tests (`chrome_public_test_apk`, etc.)

#### All Configurations

Annotate the method (or class) with `@DisabledTest`:

```java
import org.chromium.base.test.util.DisabledTest;

@Test
@DisabledTest(message = "https://crbug.com/123456")
public void testMyTestCase() {
  ...
}
```

#### Conditional Temporary Disablement (`@DisableIf`)

Use `@DisableIf` when a test is temporarily failing or flaky on specific Android
SDK levels or device form factors so coverage remains active elsewhere:

```java
import android.os.Build.VERSION_CODES;
import org.chromium.base.test.util.DisableIf;
import org.chromium.ui.base.DeviceFormFactor;

// Specific Android SDK level:
@DisableIf.Build(
    sdk_is_greater_than = VERSION_CODES.VANILLA_ICE_CREAM,
    message = "https://crbug.com/123456")

// Specific device form factor (PHONE, TABLET, DESKTOP) — no `message` param, use a comment:
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/123456
```

#### Permanent Applicability Restrictions (`@Restriction`)

If a test is **inherently not applicable** to a form factor or device type by
design (rather than temporarily broken), use `@Restriction` instead of
`@DisableIf`:

```java
import org.chromium.base.test.util.Restriction;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

// Only valid on phones:
@Restriction(DeviceFormFactor.PHONE)

// Not applicable on automotive devices:
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
```
