---
mode: "agent"
description: "Refactor a C++ unit test file to an In-Process Browser Test in Chromium."
---
# Chromium Code Refactoring: Unit Test to Browser Test

You are an AI assistant with 10 years of experience writing Chromium unit tests
and browser tests. The C++ test file `${file}` (which should now be named
`*_browsertest.cc`) has already been renamed and its entry in
`chrome/test/BUILD.gn` has been updated. Your task is to continue refactoring
this file by updating the test code itself and apply the following
transformations to the C++ test file content.

## Step by step instructions

Let's proceed through the checklist one step at a time to refactor the test
file. Mark each task as complete once it is finished.

```markdown
[ ] 1. Review Expectations
[ ] 2. Modify Test Fixture Base Class
[ ] 3. Adapt `SetUp` Method Signature
[ ] 4. Adapt `TearDown` Method Signature
[ ] 5. Update Test Case Definition Macro
[ ] 6. Refactor Internal Test Logic
```

## Tell the user to proceed to `gtest`
After completing the refactoring steps, inform the user that they should
proceed to run the `/gtest` prompt to build, run and fix any errors in the
test.

## Review Expectations
Before executing the steps, audit if you believe you have enough information
to complete the task to refactor the ${file} with deterministic output. If you
are unsure, ask the user to clarify or provide more details specific unknowns.

## Modify Test Fixture Base Class
- Identify the primary test class declaration (e.g.,
  `class YourTestSuiteName : public ... {`).
- The class typically inherits from `BrowserWithTestWindowTest` or a similar
  unit test base class.
  - *Example (before)*:
    `class YourTestSuiteName : public BrowserWithTestWindowTest {`
- Modify the base class to `InProcessBrowserTest`.
  - *Example (after)*: `class YourTestSuiteName : public InProcessBrowserTest {`
- Ensure `#include "content/public/test/browser_test.h"` is present for
  `InProcessBrowserTest`. If `BrowserWithTestWindowTest` was used, its header
  (`#include "chrome/test/base/browser_with_test_window_test.h"`) should be
  removed if it is no longer needed.

## Adapt `SetUp` Method Signature
- Identify the `SetUp` method within your test fixture.
- The current signature is typically `void SetUp() override`.
- Change to the required signature: `void SetUpOnMainThread() override`.
    - This change reflects the browser test lifecycle, where setup specific to
      the browser's main thread is performed here. Call the base class's method:
      `InProcessBrowserTest::SetUpOnMainThread();`

## Adapt `TearDown` Method Signature
- Identify the `TearDown` method within your test fixture, if present.
- The current signature is typically `void TearDown() override`.
- Change to the required signature: `void TearDownOnMainThread() override`.
    - This aligns with the browser test lifecycle for teardown operations on the
      main thread. Call the base class's method if overriding:
      `InProcessBrowserTest::TearDownOnMainThread();`

## Update Test Case Definition Macro
- Identify all test case definitions using `TEST_F`.
- The current macro is `TEST_F(TestSuiteName, TestName)`.
- Change to the required macro:
  `IN_PROC_BROWSER_TEST_F(TestSuiteName, TestName)`.
    - Note: `TestSuiteName` and `TestName` arguments remain unchanged.
      This macro is standard for In-Process Browser Tests.

## Refactor Internal Test Logic (Using Browser Interaction Patterns)
- Adapt the internal logic of each test case to use APIs suitable for browser
  tests. This may involve interacting with browser components like tabs,
  windows, profiles, and web contents. Replace mocks or simulated behaviors with
  real browser interactions.
- Review each test's logic:
    - Ensure `browser()` (provided by `InProcessBrowserTest`) is used to access
      the current browser instance.
    - Include necessary headers for any new APIs used (e.g.,
      `chrome/browser/ui/tabs/tab_strip_model.h`,
      `content/public/test/browser_test_utils.h`,
      `chrome/test/base/ui_test_utils.h`).

## Common Browser Interaction Patterns (Examples)
Use these patterns as needed. They are not direct replacements for specific
lines but are examples of how to perform common actions in browser tests.

### Adding a new tab to the active browser
- Requires `#include "chrome/test/base/ui_test_utils.h"` for `AddTabAtIndex`.
- using `ui_test_utils` for more robust tab addition:
  ```cpp
  // Example: Opening a blank tab
  // ui_test_utils::NavigateToURL(AddTab(browser(), GURL("about:blank")), GURL("about:blank")); // If AddTab returns WebContents*
  // A more common pattern:
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ```
- With a specific URL:
  ```cpp
  // Example: Opening a specific Chrome settings page
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://settings"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ```

### Adding a tab at a specific index (using `ui_test_utils`)
- Requires `#include "chrome/test/base/ui_test_utils.h"`.
```cpp
// Example: Adding a tab at the beginning with a 'link' transition
// Note: AddTabAtIndex is deprecated, prefer NavigateToURLWithDisposition or similar.
// If you must use it:
// ui_test_utils::AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
// This assumes AddTabAtIndex is available and correctly scoped or called on browser()->tab_strip_model().
// A more modern approach:
NavigateParams params(browser(), GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
params.tabstrip_index = 0;
ui_test_utils::Navigate(&params);
```

### Accessing the active WebContents
- Requires `#include "chrome/browser/ui/tabs/tab_strip_model.h"` and
  `#include "content/public/browser/web_contents.h"`.
```cpp
content::WebContents* active_web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();
ASSERT_TRUE(active_web_contents);
// Now you can interact with active_web_contents.
```

### Navigating the current tab to a URL and waiting for completion
Requires `#include "chrome/test/base/ui_test_utils.h"` and `#include "url/gurl.h"`.
```cpp
ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
```

### Executing JavaScript in a tab
- Requires `#include "content/public/test/browser_test_utils.h"` and
  `#include "content/public/browser/web_contents.h"` (if not already included
  for `active_web_contents`).
```cpp
content::WebContents* web_contents = browser()->tab_strip_model()->GetActiveWebContents(); // Or a specific WebContents
ASSERT_TRUE(web_contents);
// To execute in the main frame:
EXPECT_TRUE(content::ExecJs(web_contents, "document.title = 'New Title';"));
// To get a result:
// EXPECT_EQ("New Title", content::EvalJs(web_contents, "document.title;"));
```

### Waiting for conditions
- Browser tests often require waiting for asynchronous operations.
  Use appropriate waiting mechanisms, e.g.,
  `content::TestNavigationObserver`, `base::RunLoop`, or specific
  `ui_test_utils` waiters.
