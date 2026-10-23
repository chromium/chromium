# Feature Showcase Code Review Checklist

Every item is verifiable from the diff plus the tree; confirm symbol names
against the patchset before citing them. Anything that needs the UX spec,
product priorities, or generic style judgment is intentionally absent.

## 0. Layer Map (for CLs that add a step)

A new step spans six layers. Establish which layers the CL claims to cover
(stacked CLs are fine when the description says what is deferred), then audit
each covered layer with sections 1–5. For the shape of each file, compare with
an existing step **at the patchset**, not with this document.

| Layer                   | Files (under `chrome/browser/` unless noted)                                                                                                                                   | Must contain                                                                                                                                                                                                          |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. Identifier + checker | `ui/views/profiles/feature_showcase/feature_showcase_constants.h`, `<name>_*eligibility_checker.{h,cc}`, `BUILD.gn`                                                            | `kFeatureShowcase<Name>StepIdentifier`; subclass of `FeatureShowcaseStepEligibilityChecker`; sources and unittest in the same `BUILD.gn` targets as sibling checkers                                                  |
| 2. Metrics              | `feature_showcase_metrics.{h,cc}`, `tools/metrics/histograms/metadata/profile/{enums,histograms}.xml`                                                                          | `FeatureShowcaseStep::k<Name>` appended; map entry in `GetFeatureShowcaseStep()`; `suffix` case in `RecordStepUserAction()`; `enums.xml` int; `{FeatureShowcaseStep}` variant                                         |
| 3. Mojo + handler + UI  | `ui/webui/feature_showcase/<name>.mojom`, `<name>_handler.{h,cc}`, `feature_showcase_ui.{h,cc}`, `BUILD.gn`, `chrome_browser_interface_binders_webui_parts*.cc`                | `<Name>PageHandler` + `<Name>PageHandlerFactory`; `FeatureShowcaseUI` inherits the factory, `BindInterface`, `Create<Name>PageHandler`, `Add<Name>StepResources`; binder next to existing `FeatureShowcaseUI` binders |
| 4. WebUI step           | `resources/feature_showcase/<name>/<name>_step.{ts,html.ts,css}`, `app.ts`, `app.html.ts`, `BUILD.gn`                                                                          | `<feature-showcase-<identifier>-step>`; `hasStep_('<identifier>')` block in `app.html.ts`; `ts_files`, `css_files`, `static_files`, `mojo_files` updated                                                              |
| 5. Registration         | `ui/views/profiles/first_run_flow_controller.cc`                                                                                                                               | `checkers.push_back(...)` in the `FeatureShowcaseStepController` constructor                                                                                                                                          |
| 6. Tests                | `<name>_*eligibility_checker_unittest.cc`, `<name>_handler_unittest.cc`, `chrome/test/data/webui/feature_showcase/app_test.ts`, `first_run_feature_showcase_ui_browsertest.cc` | Checker, handler, and WebUI tests in the same CL; identifier in pixel-test `kSteps`                                                                                                                                   |

**Identifier contract.** The kebab-case identifier must be identical in
`feature_showcase_constants.h`, `GetFeatureShowcaseStep()`'s map (which
`NOTREACHED()`s on unknown ids), `app.html.ts` (`hasStep_()` and the element
`id`), the pixel test `kSteps`, and any `--force-fre-feature-showcase-steps`
usage in tests.

## 1. Eligibility Checkers

- [ ] **At-most-once callback.** No path in `CheckEligibility()` can run the
  callback twice (for example both an observer event and `OnTimeout()`).
- [ ] **Timeout hygiene.** A checker that keeps member state while pending
  (`base::ScopedObservation`, a stored callback) overrides `OnTimeout()` to
  reset it and return a value derivable from local state (`false` when not
  derivable). Synchronous checkers and one-shot async workers that forward the
  callback need no override.
- [ ] **Service shutdown.** Observed keyed services have their shutdown hook
  handled (`OnSyncShutdown`, `OnIdentityManagerShutdown`, ...), and
  `raw_ptr`/`WeakPtr` usage cannot dangle past the service.
- [ ] **Policy and existing state.** Returns `false` when the feature is managed
  off / forced by policy, or when the user already has it.
- [ ] **Synced state read synchronously.** If the checker reads a pref
  registered as `SYNCABLE_PREF` (or a model backed by one) without waiting for
  sync, ask whether a user who just enabled sync could be offered something
  already configured on another device. A question, not a blocker.
- [ ] **Platform gating lives inside the checker** (`#if BUILDFLAG(IS_*)` in
  `CheckEligibility()`, `false` elsewhere), not in `BUILD.gn` or at
  registration.
- [ ] **Identifier.** `GetStepIdentifier()` returns the constant from
  `feature_showcase_constants.h`, and that constant is in
  `GetFeatureShowcaseStep()`'s map (which `NOTREACHED()`s otherwise).

## 2. Tracker and Flow Controller

- [ ] **Registration.** The checker is pushed in the
  `FeatureShowcaseStepController` constructor; a flag-gated step gates both its
  `push_back` and its `conflicting_steps` entry.
- [ ] **Conflicts** are `{higher_priority_id, lower_priority_id}` and the key is
  registered before the value; the tracker removes losers *before* applying
  `kMaxFeatureShowcaseSteps`.
- [ ] **Zero-step path** still runs `step_shown_callback_` with `success=false`
  and `step_completed_callback_` without loading the WebUI.
- [ ] **Eligibility consumers** (`FinishOrContinueStepController`'s `?showcase=`
  and `GetHatsSurveyTrigger()`) still read `is_feature_showcase_eligible()`
  correctly if the state moved.
- [ ] **Teardown symmetry.** Anything acquired in `OnLoadFinished()` (native
  "Start browsing" button, ambient sound, `FeatureShowcaseUI` callbacks) is
  released in `OnHidden()`.
- [ ] **IPC ordering.** New controller logic tolerates `StartBrowsing()` before
  the first `NextStepShown()` (`last_active_step_shown()` fallback) and never
  indexes `eligible_steps_` unchecked.
- [ ] **Tracker changes** keep the posted (never synchronous) completion
  callback and weak-pointer invalidation that guard against re-entrancy and
  use-after-free; the tracker records no histograms.
- [ ] **Forced steps** (`--force-fre-feature-showcase-steps`) still bypass the
  tracker and feed `OnEligibilityDetermined()` directly.

## 3. Mojo, Page Handlers, and Metrics

- [ ] **Minimal surface.** `.mojom` methods are semantic actions; no URLs, pref
  names, resource ids, or other privileged parameters cross from the renderer.
- [ ] **Factory wiring.** `FeatureShowcaseUI` inherits the factory, its
  `BindInterface` resets before binding, the handler is owned by a `unique_ptr`,
  and the factory is registered in the desktop-only block next to existing
  `FeatureShowcaseUI` binders in
  `chrome/browser/chrome_browser_interface_binders_webui_parts*.cc` (or adds
  `FeatureShowcaseUI` to the existing controller list when reusing a shared
  `cr_components` factory).
- [ ] **User action metrics.** Accept path records
  `FeatureShowcaseStepUserAction::kAccepted`, skip path `kDeclined`, with the
  right `FeatureShowcaseStep`. If one path is unreported, ask how and where it
  will be reported; do not prescribe the IPC shape.
- [ ] **Metrics contract.** `FeatureShowcaseStep` is append-only (`kMaxValue`
  updated) and mirrored in `enums.xml`; `RecordStepUserAction()`'s `suffix`
  switch matches the `{FeatureShowcaseStep}` token variants in `histograms.xml`,
  inside their `LINT.IfChange(FeatureShowcaseStepSuffix)` blocks.
- [ ] **Reversible previews.** Any state applied before confirmation is
  snapshotted and reverted on skip **and** in the destructor unless accepted;
  the revert is idempotent.
- [ ] **Checker and handler agree on service availability.** Services that
  always exist on a regular FRE profile are `CHECK`ed (`CHECK_DEREF`) rather
  than silently branched on; services that can legitimately be absent (for
  example `SyncService` under `--disable-sync`) are null-handled the same way in
  the checker and the handler, so the step is never offered when its action
  cannot be applied.

## 4. WebUI Frontend, A11y, Platform

- [ ] **Platform button order is expressed in DOM order.** `#confirm-button`
  before `#skip-button` only inside `<if expr="is_win">`; reversed under
  `<if expr="not is_win">` (button visual classes such as `action-button` or
  `tonal-button` are UX choices). Visual reordering with CSS
  (`flex-direction: row-reverse`, `order`) is not acceptable: screen-reader and
  focus order follow the DOM.
- [ ] **Double-click protection.** Both buttons bind
  `?disabled="${this.buttonsDisabled}"`, the click handler sets
  `buttonsDisabled = true` before firing `step-completed`, and the app passes
  `?buttons-disabled`.
- [ ] **App wiring.** `app.ts` imports the component; `app.html.ts` has a
  `hasStep_('<identifier>')` block with `id="<identifier>"`, `slot="view"`,
  `@step-completed`, and a nested `<feature-showcase-stepper slot="stepper">`.
- [ ] **Shared layout.** Uses `<feature-showcase-step>` slots; the stepper slot
  is forwarded (`<slot name="stepper" slot="stepper">`). Any change to shared
  files (`feature_showcase_step.*`, `feature_showcase_stepper.*`, `app.*`)
  applies across all steps — flag step-specific or leftover rules added to
  shared CSS/templates that the introducing step overrides or leaves unused.
- [ ] **Images.** Every `<img>` has an `alt` attribute (`alt=""` is the
  established default for step illustrations) and never combines `alt=""` with
  `aria-label`. Every image path referenced from HTML or CSS is registered
  (`static_files` in the resources `BUILD.gn`, or `AddResourcePath` in
  `feature_showcase_ui.cc`).
- [ ] **Localization.** All strings via `$i18n{}` / `AddLocalizedStrings`.
  Strings whose body names the product ("Chrome", "Chromium") live in both
  `chromium_strings.grd` and `google_chrome_strings.grd`, not in a `.grdp` part
  of `generated_resources.grd` (such as `profiles_strings.grdp`): the brand-name
  presubmit does not scan parts, and Chromium builds would show the wrong
  product name.

## 5. Tests and Build

- [ ] **Checker unittest** covers eligible / already-has-feature /
  policy-disabled / platform-ineligible, plus timeout and shutdown for checkers
  that keep state while pending.
- [ ] **Handler unittest** covers accept, skip, state mutation (and destructor
  revert for previews), and the `StepUserAction.<Name>` histogram.
- [ ] **WebUI test** (`app_test.ts`) covers both buttons through `TestMock`
  handlers; flag-gated steps are enabled in `feature_showcase_browsertest.cc`.
- [ ] **Pixel test** lists the identifier in `kSteps` and the name generator
  (baselines are triaged in Skia Gold, not in the CL).
- [ ] **Interactive UI test** expectations updated when default eligibility,
  priority, or histogram emission in the end-to-end flow change.
- [ ] **Tracker unittest** updated for conflict/cap/timeout behavior changes.
- [ ] **Build.** New files are in the right desktop-only `source_set`s; the
  resources `BUILD.gn` lists the new `ts_files`, `css_files`, `static_files`,
  and `mojo_files`; no blanket desktop-only `assert` is added to a `BUILD.gn`
  that also hosts non-desktop code.
