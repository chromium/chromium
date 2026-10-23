---
name: kombucha-testing
description: Author, update, and debug Chromium Kombucha interactive UI tests (tests deriving from InteractiveBrowserTest, InteractiveBrowserWindowTest, InteractiveFeaturePromoTest, or other Kombucha-derived fixtures and mixins) for Views desktop UI, WebUI (DOM via DeepQuery), menus, dialogs, and multi-window flows. Use whenever writing or fixing end-to-end interactive UI tests, scaffolding RunTestSequence suites, instrumenting WebContents, or triaging Kombucha failures and flakiness.
---

# Kombucha Interactive UI Testing in Chromium

Use this skill whenever authoring, updating, refactoring, or debugging
**Kombucha** tests: any test whose fixture derives from
`InteractiveBrowserTest`, `InteractiveBrowserWindowTest`,
`InteractiveViewsTest`, `InteractiveFeaturePromoTest`, or any further derived
fixture or `Interactive*TestMixin<T>` used by the test or by the component under
test.

Kombucha is a declarative, gray-box C++ test DSL: a test is a
`RunTestSequence(...)` of **verbs** that act on UI elements referenced by
`ui::ElementIdentifier`. Steps run in order; the first failure or timeout fails
the test with a description of the failing step, a nested step stack, and a dump
of all known elements.

______________________________________________________________________

## 1. Where the Truth Lives (read before writing code)

This skill intentionally does **not** duplicate the Kombucha docs or enumerate
verbs; those change and the headers are the source of truth. Read, in order:

1. **API overview & getting started**:
   [`chrome/test/interaction/README.md`](/chrome/test/interaction/README.md)
   ([go/kombucha-api](https://goto.google.com/kombucha-api)) — fixture choices,
   verb categories, custom verbs, custom fixtures, logging.
2. **Playbook** (cookbook, FAQ, troubleshooting):
   [`chrome/test/interaction/PLAYBOOK.md`](/chrome/test/interaction/PLAYBOOK.md)
   ([go/kombucha-playbook](https://goto.google.com/kombucha-playbook)).
3. **Verb declarations** — open the header and confirm the exact signature
   before emitting code:
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
   - **Any further derived fixture or mixin** used by the current test or by the
     component under test (grep for `InteractiveBrowserTestMixin<`,
     `: public Interactive`, and `*_interactive_uitest.cc` next to the feature);
     these often add feature-specific verbs you should reuse.
4. **Element identifiers**:
   [`chrome/browser/ui/browser_element_identifiers.h`](/chrome/browser/ui/browser_element_identifiers.h)
   for shared browser UI, plus the owning class of the component (see §3).
5. **Compiled, in-tree exemplars** (always current because they build on CQ;
   prefer copying from these over any snippet in this skill):
   - WebUI via app menu, `InstrumentTab`, `DeepQuery`, custom verbs, mixin:
     [`chrome/browser/ui/webui/downloads/downloads_page_interactive_uitest.cc`](/chrome/browser/ui/webui/downloads/downloads_page_interactive_uitest.cc)
   - Views dialog with `InAnyContext`:
     [`chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog_interactive_uitest.cc`](/chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog_interactive_uitest.cc)
   - Accelerators / keyboard:
     [`chrome/browser/ui/views/browser_accelerator_interactive_uitest.cc`](/chrome/browser/ui/views/browser_accelerator_interactive_uitest.cc)
   - IPH with `InteractiveFeaturePromoTest`:
     [`chrome/browser/ui/views/user_education/browser_user_education_service_interactive_uitest.cc`](/chrome/browser/ui/views/user_education/browser_user_education_service_interactive_uitest.cc)
   - Many more: search `RunTestSequence filepath:chrome/browser` in Code Search,
     and look for `*_interactive_uitest.cc` next to the feature.

> [!IMPORTANT] Never invent verbs or identifiers. If a verb is not in one of the
> headers above (or a derived fixture in use), it does not exist. If this skill
> and the docs/headers disagree, the docs/headers win — and the disagreement is
> a candidate for the Playbook update loop in §7.

______________________________________________________________________

## 2. Test Binary

| Binary                 | Required when the test...                                                                                                                                      |
| :--------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `interactive_ui_tests` | Moves the mouse (`MoveMouseTo`, `ClickMouse`, `DragMouseTo`, `ReleaseMouse`), activates windows, or depends on focus (menus/bubbles that close on focus loss). |
| `browser_tests`        | Uses only programmatic verbs (`PressButton`, `SelectMenuItem`, `CheckView*`, `CheckJsResultAt`, `WaitFor*`).                                                   |

Mouse and activation verbs `CHECK()`-fail outside `interactive_ui_tests`. Add
the file to the matching `sources` list in `chrome/test/BUILD.gn` and name it
`*_interactive_uitest.cc` or `*_browsertest.cc` accordingly. See the README's
"Using the Kombucha API" note for details.

______________________________________________________________________

## 3. Authoring Workflow

### Step 1: Find or add element identifiers

Identifiers are **often declared on the owning class**. Identifiers for elements
used only by one class/system and its tests should **preferentially live on that
class** (`DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE` in the class body,
`DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE` in the `.cc`). Shared browser chrome
uses the namespace-scoped `DECLARE_ELEMENT_IDENTIFIER_VALUE` /
`DEFINE_ELEMENT_IDENTIFIER_VALUE` in `browser_element_identifiers.h`.

1. Grep the component's own headers first (e.g.
   `AppMenuModel::kDownloadsMenuItem`), then
   `chrome/browser/ui/browser_element_identifiers.h`.
2. If the View has none, add one on the owning class:
   ```cpp
   // feature_view.h  (class-scoped: must be inside the class body)
   class FeatureView : public views::View {
    public:
     DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSaveButtonElementId);
     // ...
   };

   // feature_view.cc
   DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FeatureView, kSaveButtonElementId);
   // where the button is created:
   save_button_->SetProperty(views::kElementIdentifierKey, kSaveButtonElementId);
   ```
3. Test-local identifiers (tabs, WebContents, custom events, states):
   ```cpp
   DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
   DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPageReadyEvent);
   DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MyStateObserver, kMyState);
   ```
4. For unnamed Views, name them at runtime with the `Name*View*` verbs in
   `interactive_views_test.h`.

### Step 2: Pick the fixture

Follow the README's "Getting Started" ordering: prefer the pre-configured
fixtures (`InteractiveBrowserWindowTest` if no Views access is needed,
`InteractiveBrowserTest` if it is, `InteractiveFeaturePromoTest` for IPH), then
mixins onto an existing fixture, then the raw `*TestApi` classes. Reuse an
existing feature-specific derived fixture when one exists.

```cpp
class MyFeatureInteractiveUiTest : public InteractiveBrowserTest {
 public:
  MyFeatureInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kMyFeature);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
```

### Step 3: Compose the sequence

```cpp
IN_PROC_BROWSER_TEST_F(MyFeatureInteractiveUiTest, OpensDialogAndApplies) {
  RunTestSequence(
      PressButton(kMyFeatureToolbarButtonElementId),
      WaitForShow(MyFeatureDialog::kDialogElementId),
      CheckViewProperty(MyFeatureDialog::kApplyButtonElementId,
                        &views::LabelButton::GetText, u"Apply"),
      PressButton(MyFeatureDialog::kApplyButtonElementId),
      WaitForHide(MyFeatureDialog::kDialogElementId));
}
```

### Step 4: Factor repeated steps into custom verbs

```cpp
auto OpenDownloadsPage() {
  return Steps(InstrumentTab(kDownloadsTabId),
               PressButton(kToolbarAppMenuButtonElementId),
               SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
               WaitForWebContentsNavigation(kDownloadsTabId,
                                            GURL(chrome::kChromeUIDownloadsURL)));
}
```

Give custom verbs a description (`AddDescriptionPrefix` / `SetDescription`) so
the nested step stack in failure output is readable.

### Step 5: Review the tests locally

Use
[`references/review-checklist.md`](/agents/skills/kombucha-testing/references/review-checklist.md)
to review the tests for likely errors and adherence to best practices.

______________________________________________________________________

## 4. Contexts

Each browser/PWA window is one `ElementContext`. The window's Views, toolbar,
**menus**, and **bubble dialogs anchored to the window** share it and need no
special handling. These do **not** and require `InAnyContext(...)`:

- Tab-modal dialogs (they can move between windows).
- WebUI help-bubble anchors (`TrackedElementWebUI`).
- Elements in a second browser / PWA / incognito window.
- Anything whose location you do not know.

After an `InAnyContext` step, keep using `InAnyContext` or `InSameContext` for
later steps referring to that secondary context:

```cpp
RunTestSequence(
    InAnyContext(WaitForShow(kTabModalDialogElementId)),
    InSameContext(Steps(PressButton(kTabModalDialogApplyElementId),
                        WaitForHide(kTabModalDialogElementId))));
```

> [!NOTE] **Context modes nest outer-wins.** If a custom verb internally sets a
> context mode (e.g. `InAnyContext(...)`) and the calling test wraps that verb
> in a different mode (e.g. `InSameContext(MyVerb())`), the **outermost** mode
> applies to the whole verb. Check the verb's definition before wrapping it.

WebContents verbs (`WaitForWebContentsReady`, `CheckJsResultAt`, ...) already
search all contexts because instrumented IDs are globally unique. To run a whole
sequence in another window use `RunTestSequenceInContext(ctx, ...)`.

______________________________________________________________________

## 5. Non-Obvious Patterns (the rest is in the headers)

### WebContents / WebUI

```cpp
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
// Each DeepQuery hop pierces one shadow root.
const DeepQuery kSearchBox = {"downloads-manager", "downloads-toolbar",
                              "cr-toolbar", "#search"};

InstrumentTab(kTabId),            // existing active tab
InstrumentNextTab(kTabId),        // call BEFORE the action that opens the tab
WaitForWebContentsReady(kTabId, GURL(...)),
WaitForJsResultAt(kTabId, kSearchBox, "el => el.offsetParent !== null", true),
CheckJsResultAt(kTabId, kSearchBox, "el => el.disabled", false),
```

- `WaitForJsResult[At]` and `WaitForStateChange` are both fine ways to wait on
  DOM state; **use `WaitForStateChange` when it expresses the condition more
  simply** (custom events, `continue_across_navigation`,
  `kExistsAndConditionTrue`, etc.). It is often the easier choice.
- Call `WaitForWebContentsPainted` before screenshots or synthetic input.

### Mouse

Prefer `PressButton()` / `SelectMenuItem()` over `MoveMouseTo()` +
`ClickMouse()` unless the test is specifically about mouse input. Mouse verbs
take `ui_controls::LEFT`-style buttons; see `interactive_mouse_test.h`.

### IPH / User Education

```cpp
class MyPromoTest : public InteractiveFeaturePromoTest {
 public:
  MyPromoTest()
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({feature_engagement::kIPHMyFeature})) {}
};
```

### Waiting: preferences, not rules

Prefer event-driven verbs (`WaitForShow`, `WaitForJsResult*`,
`WaitForStateChange`, `WaitForViewProperty`, `ObserveState` + `WaitForState`,
custom events) over polling (`PollView*`) or run-loop spinning. Polling is
acceptable when the event-driven form is significantly harder to express;
`base::PlatformThread::Sleep` is never acceptable. When waiting on a transient
value, use a matcher (`WaitForState(k, testing::Ge(1))`) rather than an exact
value; `WithoutDelay` is a last resort and must carry a `TODO(crbug.com/...)`.

______________________________________________________________________

## 6. Triage Loop: Reading Failures Against the Playbook

When a test fails to compile, fails locally, or fails on trybots, do **not**
guess.

### Step 1: Read the failure block

Kombucha prints, in order:

- `Interactive test failed ... with reason <AbortedReason>` plus the failing
  step's description, type, and element id.
- **A nested step stack** — `Nested steps (outermost to innermost):` with one
  frame per enclosing verb, each giving the index of the action within that verb
  (e.g. `#0: In OpenDownloadsPage(), step 3: WaitForWebContentsNavigation()`).
  This is the primary way to locate the failing action; descriptions you added
  to custom verbs appear here. (Only when no stack is available does it fall
  back to `step number is N`, which counts underlying `InteractionSequence`
  steps rather than top-level verbs.)
- For `kSubsequenceFailed` / timeouts inside `InParallel`/`AnyOf`/`If`:
  per-subsequence failure blocks, each with its own stack.
- `UI Elements` — a tree of every known element per context, tagged
  `[CURRENT CONTEXT]`, `[ACTIVE]`, `[FOCUSED]`; instrumented WebContents DOM is
  included to a configurable depth.
- A C++ stack trace.

Capture: the aborted reason, the innermost frame of the step stack, and whether
the target identifier appears in the element dump and in which context.

### Step 2: Map to the Playbook

| Signature                                                     | Likely cause                                                             | Playbook section → fix                                                                                                       |
| :------------------------------------------------------------ | :----------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- |
| Timeout; element **is** in the dump but under another context | Context mismatch (tab-modal dialog, WebUI anchor, other window)          | *Troubleshooting → "The test can't find an element"*: wrap in `InAnyContext`, follow with `InSameContext`; check outer-wins. |
| Timeout; element **is not** in the dump                       | Element never created / wrong identifier / prerequisite action missing   | Same section: confirm the identifier is set on the owning class; add the missing action; check feature flags.                |
| `kElementHiddenDuringStep`                                    | Element appeared then vanished: focus loss, layout oscillation, shutdown | *"My test failed due to an element losing visibility"*: run in `interactive_ui_tests`; use matchers for transient state.     |
| WebContents step times out                                    | Tab not instrumented in time, wrong URL, or DOM never reaches state      | *"Interacting with WebContents"*: instrument earlier; check `expected_url`; use `DumpWebContentsAt` to see the real DOM.     |
| `CHECK` from `RequireInteractiveTest()` / cannot move mouse   | Mouse/activation verb in `browser_tests`                                 | README *"Using the Kombucha API"*: move to `interactive_ui_tests`.                                                           |
| Template/overload errors on `RunTestSequence`                 | Wrong callable arity/return type, missing `Steps()` for a `MultiStep`    | Open the verb's header; `Do` takes a zero-arg callable, `Check` returns `bool`, `CheckResult` takes a matcher.               |
| Can't tell which top-level verb failed                        | Custom verbs lack descriptions                                           | README *"Logging"* / Playbook *"Describe your steps"*: add descriptions so the nested step stack is readable.                |

### Step 3: Explain and fix

1. Cite the Playbook/README section that covers the failure.
2. State the invariant that was violated.
3. Re-emit the minimal corrected sequence.

______________________________________________________________________

## 7. Living Documentation: Improving the Playbook

If diagnosing or fixing a test required knowledge that is **missing, hard to
find, or wrong** in `PLAYBOOK.md` (new platform quirk, new WebUI pattern,
changed verb behavior, outdated failure-log description), improve the Playbook.

**Before doing anything, ask the developer:**

> This issue (`<summary>`) isn't covered (or is covered incorrectly) in
> `chrome/test/interaction/PLAYBOOK.md`. Want me to draft an FAQ/Troubleshooting
> update so others benefit?

If yes:

1. **Find the right section** under `# FAQ {#faq}` (e.g. *Writing Tests with
   Kombucha*, *Working with contexts*, *Interacting with WebContents*,
   *Troubleshooting*). Questions in each section use the format:
   `### Question? {#anchor}` heading, 1–2 short paragraphs, and a minimal
   good/bad code pair.
2. **Check for an existing question** that looks like it should cover the
   situation but doesn't, or is no longer correct. If so, **correct that
   question in place** rather than adding a near-duplicate.
3. **Otherwise add a new question** in the appropriate section, in the preferred
   format, placed near questions on similar topics. Keep the anchor stable and
   descriptive.
4. **What makes a good entry:** it states the symptom a developer actually sees
   (error text, behavior), the underlying reason in one or two sentences, the
   concrete fix with a short code pair, and links to the relevant verb or
   header. Avoid restating general API docs.
5. Include the doc change in the same CL (or a follow-up) for review by
   `chrome/test/interaction/OWNERS`.
