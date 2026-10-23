---
name: kombucha-test-reviewing
description: Audits browser and interaction tests for best practices in the context of Chromium Kombucha interactive UI tests (tests deriving from InteractiveBrowserTest, InteractiveBrowserWindowTest, InteractiveFeaturePromoTest, or other Kombucha-derived fixtures and mixins) desktop UI, WebUI (DOM via DeepQuery), menus, dialogs, and multi-window flows. Use whenever reviewing end-to-end interactive UI tests which use (or could use) Kombucha.
---

# Kombucha Test Review

## 1. Before You Begin (read before reviewing)

> [!IMPORTANT] This is an experimental review skill. Most of the rules here are
> suggestions; if there is a good reason not to follow them then they can be
> ignored. If a CL owner is consistently rejecting suggestions, suggest that
> they reach out to the OWNERS of this skill to provide feedback.

1. Where the Truth Lives (background on Kombucha)

   - **API overview & getting started**:
     [`chrome/test/interaction/README.md`](/chrome/test/interaction/README.md)
     ([go/kombucha-api](https://goto.google.com/kombucha-api)) — fixture
     choices, verb categories, custom verbs, custom fixtures, logging.
   - **Playbook** (cookbook, FAQ, troubleshooting):
     [`chrome/test/interaction/PLAYBOOK.md`](/chrome/test/interaction/PLAYBOOK.md)
     ([go/kombucha-playbook](https://goto.google.com/kombucha-playbook)).
   - **Verb declarations** — open the header and verify usage against the exact
     signatures:
     - [`ui/base/interaction/interactive_test.h`](/ui/base/interaction/interactive_test.h)
       (base verbs, flow control, contexts, state observation)
     - [`ui/views/interaction/interactive_views_test.h`](/ui/views/interaction/interactive_views_test.h)
       (Views checks, naming, `WaitForViewProperty`)
     - [`ui/views/interaction/mouse/interactive_mouse_test.h`](/ui/views/interaction/mouse/interactive_mouse_test.h)
       (mouse; `interactive_ui_tests` only)
     - [`chrome/test/interaction/interactive_browser_window_test.h`](/chrome/test/interaction/interactive_browser_window_test.h)
       (WebContents / WebUI / `DeepQuery` / screenshots)
     - [`chrome/test/interaction/interactive_browser_test.h`](/chrome/test/interaction/interactive_browser_test.h)
       (browser + Views fixture)
     - [`chrome/test/user_education/interactive_feature_promo_test.h`](/chrome/test/user_education/interactive_feature_promo_test.h)
       (IPH / User Education)
     - **Any further derived fixture or mixin** used by the current test or by
       the component under test (grep for `InteractiveBrowserTestMixin<`,
       `: public Interactive`, and `*_interactive_uitest.cc` next to the
       feature); these often add feature-specific verbs you should reuse.
   - **Element identifiers**:
     [`chrome/browser/ui/browser_element_identifiers.h`](/chrome/browser/ui/browser_element_identifiers.h)
     for shared browser UI, plus the owning class of the component referenced by
     the test.

2. Determine if Kombucha is the correct tool

   - This skill is for reviewing Desktop Chrome tests (tests that run on
     Windows, Linux, MacOS, and/or ChromeOS), not Android, or iOS tests. If this
     is not a Desktop Chrome test, stop and do not use this skill.
   - This skill is for reviewing browser and interaction tests. If the code
     being reviewed is not test code, stop and do not use this skill.
   - If a small portion of an existing test suite is being modified that does
     not already use Kombucha, or unit tests that do not depend on `Browser`,
     `BrowserView`, or `BrowserWindowInterface` are being added or modified,
     then stop and do not use this skill to evaluate the test.
   - If a new test class is being added that depends on `Browser`,
     `BrowserView`, or `BrowserWindowInterface`, or the tests being modified
     already use `InteractiveBrowserTest`, `InteractiveBrowserTestMixin`,
     `InteractiveBrowserWindowTest`, `InteractiveBrowserWindowTestMixin`,
     `InteractiveFeaturePromoTest` or other Kombucha-derived fixtures, then use
     this skill to review the test.
   - An indication a new test _might_ need to use Kombucha is introducing a new
     test fixture derived directly or indirectly from `InProcessBrowserTest` but
     not derived from one of the Kombucha test classes. Continue to the next
     section.

## 2. Preliminary Review Checklist

1. Is this a test that does not use Kombucha but should?

   - Consider the following:
     - It is derived directly or indirectly from `InProcessBrowserTest`
     - It is not currently derived from any of the Kombucha test classes.
     - It is a new test fixture, or an existing test fixture where a large
       number of test cases are being added.
   - If so, then ask the CL owner the following before proceeding:
     > It looks like you're writing a browser or interaction test. Have you
     > considered using Kombucha (go/kombucha-api, go/kombucha-playbook)?

2. Is this a Kombucha test that should not be?

   - Kombucha, and specifically `InteractiveViewsTest` and
     `InteractiveViewsTestMixin`, can be used to unit-test classes derived from
     `views::View`.
   - Otherwise, Kombucha should _generally_ not be used to create new unit tests
     (test file names matching the regex `.*_unittest\.cc`).

## 3. Reviewing Kombucha Tests

Do not proceed to this section until you and the CL owner have determined that
Kombucha is both the correct tool for the test and is the tool being used.

### A. Common Pitfalls and Style Errors

01. **Overuse of `Do` and `WithView`**
    - Determine if code inside a `Do` or `WithView` verb could easily be
      replaced by other existing verbs, and if so, suggest to do so.
    - *Reason:* Kombucha is a declarative, interaction-testing framework. Avoid
      excess procedural code wherever possible. Procedural code makes failures
      harder to reason about.
    - *Fix:* Replace the procedural code inside the `Do` or `WithView` with
      equivalent Kombucha verbs.
    - *Exceptions:* There are common patterns, such as
      `WithView(kBrowserViewElementId, ....)` which are used when access to the
      current browser window is needed. If you see a common pattern across
      tests, be more lenient with this rule.
02. **Failure to Name Custom Verbs**
    - **Always** use `SetDescription` or `AddDescriptionPrefix` to name the
      steps inside a custom verb, before returning the step(s).
    - *Reason:* Step names help in debugging. When a test fails, it prints a
      "call stack" that relies on the action names.
    - *Fix:* Use `SetDescription` or `AddDescriptionPrefix` as appropriate. Look
      at other examples in similar tests for guidance.
03. **Failure to Specify Step Descriptions when Available**
    - **Always** specify the `description` or `check_description` parameter of a
      verb which has one.
    - *Reason:* Step descriptions help in debugging. Without a description, it
      will be harder to trace test failures.
    - *Fix:* Determine an appropriate description from the content of the verb
      and add it as the `description` or `check_description`.
04. **Unnecessary Use of Message Pumps**
    - Try to avoid use of `RunLoop`, `RunUntil`, or any asynchronous wait inside
      a Kombucha step. Especially avoid `RunLoop` that do not explicitly use
      `RunLoop::Type::kNestableTasksAllowed`.
    - *Reason:* Kombucha itself is asynchronous and has many ways to wait for
      asynchronous events built-in. Nesting loops may cause tests to behave
      unpredictably or deadlock. Prefer to use declarative wait conditions.
    - *Fix:* Suggest a way to replace the code with an equivalent verb with
      "Wait" in the name, such as `WaitForShow`, `WaitForStateChange`,
      `WaitForJsResult`, `WaitForState`, etc.
    - *Exceptions:* If the code would be significantly more complex (e.g.
      creating a non-trivial `StateObserver`-derived class that would only be
      used by one test case), then the wait inside the step may be acceptable.
05. **Unnecessary Use of Mouse Input**
    - Unless you are specifically testing mouse input, prefer verbs like
      `PressButton`, `SelectMenuItem`, `SelectTab`, `ClickElement`, etc.
    - *Reason:* Mouse input is less reliable due to different ways mouse events
      are processed on different systems. If you are not specifically testing
      mouse input, prefer more direct verbs.
    - *Fix:* Replace `MoveMouseTo` and `ClickMouse` with the appropriate verb
      for the element being clicked.
06. **Copy-Paste of Action Sequences**
    - Avoid having the same sequences of steps over and over in different test
      cases.
    - *Reason:* Duplicate code is harder to maintain and makes tests more
      verbose. If a specific sequence of steps needs to happen in many test
      cases, it also probably represents a single conceptual action, and can be
      replaced with a custom verb.
    - *Fix:* Combine the sequences into a custom verb.
07. **Manual Layouts**
    - There is **almost never** a reason to manually trigger Views layouts
      during a Kombucha test.
    - *Reason:* Kombucha is asynchronous and the browser or window under test
      will automatically lay out. You should observe the result of the layout
      and wait for that before proceeding.
    - *Fix:* Observe or wait for the desired result of the layout instead.
    - *Exceptions:* When there is no change to the layout other than geometry,
      and nothing to reliably observe, a manual layout may be necessary.
08. **Direct Observation of Implementation Details**
    - **Strongly prefer** to observe changes in the UI instead of/before
      checking a model or implementation detail. Avoid checking implementation
      details whenever possible.
    - *Reason:* Kombucha is an interaction-testing framework. It operates by
      observing and acting on the same UI elements the user sees and interacts
      with. If your test relies on code details not directly reflected to the
      user, your test cannot be used as a critical user journey regression test.
      Observing implementation details can also lead to race conditions.
    - *Fix:* Observe some change in the UI first that guarantees that the
      underlying model should be in the correct state, then (if it's warranted)
      verify the model using a `CheckResult` or similar step.
09. **Checking Discrete Events in Sequence**
    - Avoid waiting for a discrete event or transient state after an input, or
      for multiple discrete events. Discrete events look like `WaitForEvent` or
      steps with "transition only on event" set to `true`.
    - *Reason:* This introduces a race condition. If the event happens early
      enough in the step that produces the input, or events happen in a
      different order, the test may flake or fail.
    - *Fix:* Perform the input and wait for each resulting event in parallel
      using an `InParallel` step.
    - *Exception:* If you need to test that a sequence of discrete events happen
      in a particular order, you do not need to wait for them in parallel.
10. **Mouse Input in browser_tests**
    - Certain verbs are disallowed in the `browser_tests` binary, including all
      verbs in `interactive_mouse_tests.h`.
    - *Reason:* Simulated mouse input is only allowed in `interactive_ui_tests`
      and a handful of other binaries, and not in `browser_tests` or most unit
      test binaries.
    - *Fix:* Either switch away from using mouse verbs, or move the test to the
      `interactive_ui_tests` binary. This may involve changing the name of the
      test fixture and test file to conform to naming conventions.
11. **Screenshot Without `OnIncompatibleAction` or `pixel_test.filter`**
    - Every verb which takes a screenshot **must** be preceded by an
      `OnIncompatibleAction` so the test won't fail in non-screenshot test jobs,
      and must have a corresponding entry in `pixel_tests.filter`.
    - *Reason:* A screenshot will cause the test to fail if the current
      environment doesn't support pixel tests. A test will not be run in the
      `pixel_browser_tests` or `pixel_interactive_ui_tests` test jobs if it is
      not listed in `pixel_tests.filter`.
    - *Fix:* Ensure an `OnIncompatibleAction` precedes the screenshot verb, and
      make sure a pattern in `pixel_tests.filter` covers your screenshot tests
      (and _only_ your screenshot tests).

### B. Best Practices

1. **Interact-Observe-Verify**
   - Each Kombucha test should be a sequence of interactions, followed by
     observing changes in the UI, followed (optionally) by checking underlying
     models or implementations. The initial interaction can just be starting the
     browser/common test setup.
   - *Reason:* Tests that violate this pattern are much more likely to flake or
     be fragile to implementation changes.
   - *Fix:* Re-order the test or insert observations of UI changes to restore
     the interact-observe-verify pattern.
