# Feature Showcase Architecture & Flow Reference

How the First Run Experience (FRE) Feature Showcase
(`chrome://feature-showcase`) works and plugs into `FirstRunFlowController`.
Symbol names reflect the tree at the time of writing; confirm them against the
patchset under review before citing them.

## 1. Where the Showcase Sits

`FirstRunFlowController` (`first_run_flow_controller.{h,cc}`) registers
`Step::kFeatureShowcase` (`FeatureShowcaseStepController`) followed by
`Step::kFinishOrContinue` as post-identity steps when
`switches::IsFirstRunDesktopRevampEnabled(...)` is true; otherwise the legacy
`Step::kDefaultBrowser` promo runs instead. There is no stored "was shown"
boolean: the flow controller keeps a `WeakPtr<FeatureShowcaseStepController>`
and exposes `is_feature_showcase_eligible()` (= `is_eligible()` =
`!eligible_steps_.empty()`), consumed by

- `FinishOrContinueStepController` (`?showcase=true|false` on
  `chrome://intro/finish-or-continue`), and
- `FirstRunFlowController::GetHatsSurveyTrigger()` (`*Completed` vs
  `*NoFeatureShowcaseCompleted`).

A change that moves eligibility state, or makes it available before the tracker
has finished, must keep both consumers correct.

## 2. `FeatureShowcaseStepController` Lifecycle

Defined in `first_run_flow_controller.cc` (not in the header).

- **Constructor** builds the checker list in **priority order (highest first)**
  and the `conflicting_steps` map, then creates the
  `FeatureShowcaseEligibilityTracker`. Per-step feature flags are checked here
  and must gate both the `push_back` and the conflict entry.
- **`Show(step_shown_callback, reset_state)`**: if
  `--force-fre-feature-showcase-steps=a,b` is present the parsed list is used
  verbatim, bypassing the tracker; otherwise
  `tracker_->EvaluateEligibleSteps(*profile_, ...)`.
- **`OnEligibilityDetermined(eligible_steps)`**: zero steps → run
  `step_shown_callback_` with `success=false`, then `step_completed_callback_`;
  the WebUI is never created. Otherwise record
  `ProfilePicker.FREFlow.FeatureShowcase.StepEligible` per step and show
  `chrome://feature-showcase/?steps=a,b`.
- **`OnLoadFinished(...)`**: runs `step_shown_callback_` (`success=true`), shows
  the native toolbar "Start browsing" button, installs `SetFinishCallback` and
  `SetNextStepShownCallback` on `FeatureShowcaseUI`, and switches the ambient
  sound on.
- **`OnHidden()`**: hides the native button and switches the ambient sound back.
  This is the *only* teardown hook; the destructor is defaulted.
- **`OnNextStepShown()`** (from the WebUI's argument-less `NextStepShown()`
  IPC): first call sets `last_active_step_index_ = 0`; later calls increment it
  and play the progress sound. Records
  `ProfilePicker.FREFlow.FeatureShowcase.StepShown`. Step-specific "on shown"
  side effects are dispatched here on the resolved `FeatureShowcaseStep`.
- **`last_active_step_shown()`** falls back to index 0 when no IPC has been
  processed yet: the native "Start browsing" button
  (`FirstRunFlowController::StartBrowsing()`, which records
  `ProfilePicker.FREFlow.FeatureShowcase.StartBrowsing` and jumps to
  `Step::kFinishFlow`) can be clicked before the first `NextStepShown()`
  arrives. Controller code must not assume Mojo ordering relative to native UI.
- Audio (ambient and progress sounds) and global effect toggling live on
  `FirstRunFlowController`; the step only receives callbacks for them. Back
  navigation is unsupported (`CanNavigateBack()` default), and
  `ProfilePickerView::AddNewContents` routes external links into a popup.

## 3. Eligibility: Checkers and Tracker

Directory: `chrome/browser/ui/views/profiles/feature_showcase/`.

### `FeatureShowcaseStepEligibilityChecker`

```cpp
virtual void CheckEligibility(Profile& profile,
                              base::OnceCallback<void(bool)> callback) = 0;
virtual std::string GetStepIdentifier() const = 0;
virtual bool OnTimeout();  // Defaults to false.
```

No identifier constructor; subclasses return the constant from
`feature_showcase_constants.h` (`kFeatureShowcase<Name>StepIdentifier`,
kebab-case value, global namespace).

### `FeatureShowcaseEligibilityTracker`

`feature_showcase_eligibility_tracker.{h,cc}`; `kMaxFeatureShowcaseSteps` and
`kEvaluationTimeout` are file-local constants there.

- `EvaluateEligibleSteps()` starts every checker, keeps
  `std::vector<std::optional<bool>>` results plus a completion counter, and arms
  a `base::OneShotTimer`. A checker that runs its callback twice corrupts the
  counter.
- `FinishEvaluation()` (all done or timer fired): stops the timer, invalidates
  weak pointers so late callbacks are dropped, fills missing results from
  `OnTimeout()`, then collects eligible identifiers in registration order,
  removes every `conflicting_steps_[winner]` whose winner is eligible, truncates
  to `kMaxFeatureShowcaseSteps`, and **posts** the completion callback (never
  synchronous, to avoid destroying the tracker while it iterates).
- The tracker records **no histograms**.

### Exemplar checkers (patterns, not an exhaustive list)

- **Synchronous, local state only**: `GoogleLensStepEligibilityChecker`,
  `ThemesAndCustomizationStepEligibilityChecker`. No `OnTimeout()`.
- **Platform-specific inside the checker, one-shot async worker**:
  `DefaultBrowserStepEligibilityChecker` — registered everywhere,
  `#if BUILDFLAG(IS_WIN)` inside `CheckEligibility()` (policy, capability, async
  `DefaultBrowserWorker`), other platforms post `false`; no `OnTimeout()`
  because the callback is simply forwarded.
- **Waits on a keyed service, non-`false` fallback**:
  `PasswordManagerFeatureShowcaseEligibilityChecker` observes `SyncService`
  until preferences are active, handles `OnSyncShutdown`, and its `OnTimeout()`
  answers from the local `PinnedToolbarActionsModel`.
- **Waits on several late-arriving signals**: `GeminiStepEligibilityChecker`
  observes more than one source (account information and region data), resolves
  once all have arrived, handles service shutdown, and its `OnTimeout()` stops
  observing, drops the callback, and returns `false`.

## 4. WebUI Controller and Mojo

Directory: `chrome/browser/ui/webui/feature_showcase/`.

- `FeatureShowcaseUIConfig::IsWebUIEnabled` mirrors the flow gate; the config is
  added in `chrome/browser/ui/webui/chrome_web_ui_configs.cc`.
- `FeatureShowcaseUI` (`ui::MojoWebUIController`) implements one
  `*PageHandlerFactory` per step plus the main
  `FeatureShowcasePageHandlerFactory`, and any shared `cr_components` factories
  a step needs. Each `BindInterface` overload does
  `receiver.reset(); receiver.Bind(...)`; handlers are owned in
  `std::unique_ptr`s.
- The constructor registers per-step strings and images through
  `Add<Name>StepResources(source)` helpers. `disableAnimations` is exposed to
  the page for tests (`kDisableFirstRunAnimationsForTesting`).
- Bespoke, not a pattern: the default browser step pushes a Windows-only
  `can_pin` flag through `FeatureShowcaseUI::SetCanPinToTaskbar` and swaps its
  strings with `content::WebUIDataSource::Update`. Do not expect or suggest this
  plumbing for other steps.
- Main interface (`feature_showcase.mojom`): `FinishFeatureShowcase()` and
  `NextStepShown()`, both argument-less. Step interfaces expose only high-level
  semantic actions.
- Binders: every `*PageHandlerFactory` is registered for `FeatureShowcaseUI` in
  the desktop-only block alongside existing `FeatureShowcaseUI` binders in
  `chrome/browser/chrome_browser_interface_binders_webui_parts*.cc` (or adds
  `FeatureShowcaseUI` to the controller list when reusing a shared
  `cr_components` factory).
- `OWNERS` routes `*.mojom` to `ipc/SECURITY_OWNERS`.

## 5. WebUI Frontend (`chrome/browser/resources/feature_showcase/`)

- `app.ts` (`<feature-showcase-app>`) parses `?steps=` in the constructor
  (split, trimmed, no allow-list), asserts at least one step in
  `firstUpdated()`, switches `<cr-view-manager>` to `steps[0]`, and calls
  `nextStepShown()`. `app.html.ts` renders each step only when
  `hasStep_('<identifier>')`, with `id="<identifier>"`, `slot="view"`,
  `@step-completed`, `?buttons-disabled`, and a nested
  `<feature-showcase-stepper slot="stepper">`.
- `onStepCompleted_()` sets `areButtonsDisabled_ = true` for every transition,
  re-enables after `switchView()` resolves, and on the last step awaits
  `stepper.waitForAnimationComplete()` before `finishFeatureShowcase()`. The app
  owns the background transition Lottie animations and the `ColorChangeUpdater`;
  steps do not touch them.
- `feature_showcase_step.{ts,html.ts,css}`: shared layout with slots
  `illustration`, `stepper`, `title`, `description`, `disclosure`, `button`, and
  a responsive breakpoint.
- `feature_showcase_stepper.{ts,html.ts}`: `role="progressbar"` with
  `aria-value*`; product logo only for fewer than 3 steps, otherwise checkmarks
  (static when animations are disabled), logo at the active index, dots.
  `waitForAnimationComplete()` resolves immediately in the static case.
- Step components (`<feature-showcase-<identifier>-step>`): expose
  `buttonsDisabled`, set it `true` on click, call their Mojo handler, then
  `fire('step-completed')`. `#confirm-button` precedes `#skip-button` only
  inside `<if expr="is_win">`; elsewhere the order is reversed — in the DOM, not
  CSS, so screen-reader and focus order match.

## 6. Histograms

All in `tools/metrics/histograms/metadata/profile/histograms.xml`:

| Histogram                                                                          | Recorded by                               |
| ---------------------------------------------------------------------------------- | ----------------------------------------- |
| `ProfilePicker.FREFlow.FeatureShowcase.StepEligible`                               | controller, once per eligible step        |
| `ProfilePicker.FREFlow.FeatureShowcase.StepShown`                                  | controller, on every `NextStepShown()`    |
| `ProfilePicker.FREFlow.FeatureShowcase.StartBrowsing`                              | `FirstRunFlowController::StartBrowsing()` |
| `ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.{FeatureShowcaseStep}`       | step handlers via `RecordStepUserAction`  |
| `ProfilePicker.FREFlow.MediaEffects.{Enable,Disable}` (`.FeatureShowcase` variant) | `ToggleMediaEffects`                      |
| Step-specific extras (for example `...ThemesAndCustomization.ThemeChanged`)        | the owning handler                        |

`FeatureShowcaseStep` / `FeatureShowcaseStepUserAction` are declared in
`feature_showcase_metrics.h` and mirrored in `enums.xml`; the
`{FeatureShowcaseStep}` token variants are mirrored by the `suffix` switch in
`RecordStepUserAction`. All pairs are tied with `LINT.IfChange` /
`LINT.ThenChange`.

## 7. Testing Hooks

- `--force-fre-feature-showcase-steps` bypasses the tracker; used by pixel tests
  and by interactive UI tests via a `GetForcedFeatureShowcaseSteps()` override.
- `switches::kDisableFirstRunAnimationsForTesting` makes the UI static.
- The WebUI mocha fixture
  (`chrome/test/data/webui/feature_showcase/feature_showcase_browsertest.cc`)
  enables the revamp and per-step flags; `app_test.ts` installs `TestMock`
  handlers through each `browserProxyFactory.setInstance(...)`.
