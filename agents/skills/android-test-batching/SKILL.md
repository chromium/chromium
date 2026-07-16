---
name: android-test-batching
description: >-
  Guide for auditing and applying batching annotations (@Batch) and AutoReset rules
  (AutoResetCtaTransitTestRule) to Android javatests in Chromium. Use this skill when
  optimizing Java instrumentation test runtimes, resolving test state leaks, or migrating tests to batched execution.
---

# Android Instrumentation Test Batching Guide

This skill provides step-by-step instructions for auditing, batching, and
migrating Chromium Android instrumentation tests (`javatests`) to
`@Batch(Batch.PER_CLASS)` and `AutoResetCtaTransitTestRule`.

## Core Invariants

1. **`AutoReset` requires `@Batch`**: `AutoResetCtaTransitTestRule`
   (`ChromeTransitTestRules.fastAutoResetCtaActivityRule()`) is **only useful
   when `@Batch` annotations are present**. Without `@Batch`, the Android test
   runner restarts the browser process between test methods anyway, rendering
   `AutoReset` ineffective.
2. **`@Batch` is useful independently of `AutoReset`**:
   `@Batch(Batch.PER_CLASS)` avoids test runner restarts across test methods.

### 1. Inspect Existing Annotations & Rules

- Check the test class for existing annotations:
  - If `@DoNotBatch` is present, inspect the documented reason before modifying.
  - If `@Batch` is already present, check if `FreshCtaTransitTestRule` can be
    upgraded to `AutoResetCtaTransitTestRule`.

### 2. Adding Annotations & Activity Rules

- Add `@Batch(Batch.PER_CLASS)` to the class definition.

- Prefer `AutoResetCtaTransitTestRule` over `FreshCtaTransitTestRule` for faster
  execution:

  ```java
  // Change:
  @Rule
  public FreshCtaTransitTestRule mActivityTestRule =
          ChromeTransitTestRules.freshChromeTabbedActivityRule();

  // To:
  @Rule
  public AutoResetCtaTransitTestRule mActivityTestRule =
          ChromeTransitTestRules.fastAutoResetCtaActivityRule();
  ```

### 3. API Adaptations & Code Caveats

- **`startOnUrl` vs `startOnWebPage`**:
  `FreshCtaTransitTestRule.startOnUrl(url)` does not exist on
  `AutoResetCtaTransitTestRule`. Replace calls with
  `mActivityTestRule.startOnWebPage(mTestServer.getURL(url))` or
  `mActivityTestRule.startOnWebPage(url)`.
- **FreshCta-Specific Methods**: Methods like `skipWindowAndTabStateCleanup()`
  in `tearDown()` are specific to `FreshCtaTransitTestRule`. Keep
  `FreshCtaTransitTestRule` if such cleanup overrides are required.

______________________________________________________________________

## Cleaning Up State Bleed Across Batched Tests

State bleed across consecutive test methods in a batch can stem from various
sources. Below are common, non-exhaustive examples and cleanup mechanisms to
attempt before giving up or resorting to `@DoNotBatch`:

### 1. Finishing Stray Non-ChromeActivity Instances

If secondary or settings activities launched during a test method linger into
subsequent test runs:

```java
ThreadUtils.runOnUiThreadBlocking(() -> {
    for (Activity activity : ApplicationStatus.getRunningActivities()) {
        if (!(activity instanceof ChromeTabbedActivity) && !activity.isFinishing()) {
            activity.finish();
        }
    }
});
```

Or use `ApplicationTestUtils.finishActivity(activity)`.

### 2. Clearing Snackbar Bleed

If snackbars shown in one test method leak into subsequent tests:

```java
ThreadUtils.runOnUiThreadBlocking(() -> {
    SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
    if (snackbarManager != null) {
        snackbarManager.dismissAllSnackbars();
    }
});
```

### 3. Clearing Dialog Bleed

If modal dialogs remain open across tests:

```java
ThreadUtils.runOnUiThreadBlocking(() -> {
    ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
    if (modalDialogManager != null && modalDialogManager.isShowing()) {
        modalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
    }
});
```

### 4. Adding `ForTesting()` Reset Methods

If production components or singletons retain state across test methods, add
explicit reset methods to production or test helper classes:

```java
// Example in production/helper component:
public static void resetForTesting() {
    sInstance = null;
    // Clear static observers or registered handlers
}

// Invoke in test @After or @Before:
@After
public void tearDown() {
    MySingletonComponent.resetForTesting();
}
```

### 5. When to Use `@DoNotBatch`

Only annotate with `@DoNotBatch(reason = "<detailed reason>")` if process-level
state persists that cannot be easily reset via `ForTesting()` methods, cleanup
utilities, or test rules.

______________________________________________________________________

## Test Verification

Verify the batched test suite using `autotest.py`:

```bash
./tools/autotest.py -C out/<build_dir> <filepath> --avd-config <avd_config_path>
```

> [!TIP] **Finding AVD Configs**: It is strongly encouraged to pass
> `--avd-config` when running non-JUnit Android tests. Available emulator
> configurations can be found by inspecting the files in
> `tools/android/avd/proto/` (e.g.
> `tools/android/avd/proto/android_36_google_apis_x64.textpb`). Ensure all test
> methods pass sequentially in a single run without hanging or failing due to
> state bleed.
