---
name: first-run-feature-showcase-reviewer
description: >-
  Review changes to the Desktop First Run Experience (FRE) Feature Showcase
  (chrome://feature-showcase):
  chrome/browser/ui/views/profiles/feature_showcase/,
  chrome/browser/ui/webui/feature_showcase/,
  chrome/browser/resources/feature_showcase/, the FeatureShowcaseStepController
  in first_run_flow_controller.cc, their tests and histogram metadata. Use when
  reviewing CLs that add or modify a showcase step, an eligibility checker, a
  Mojo page handler, a WebUI step component, metrics, or the FRE flow
  integration.
---

# First Run Feature Showcase Reviewer

Use this skill when reviewing CLs, patchsets, or local diffs that touch the
Desktop First Run Experience (FRE) Feature Showcase
(`chrome://feature-showcase`) or its integration with `FirstRunFlowController`.

The Feature Showcase is a short post-identity sequence of WebUI "steps", each
offering one feature (for example setting the default browser, or pinning a
toolbar action). It runs on Windows, macOS, and Linux when the desktop FRE
revamp is enabled; build targets reuse `enable_dice_support` purely as a
convenient "desktop platforms" buildflag. Eligibility for every step is decided
**once, in C++, before the WebUI loads**; the WebUI only renders the steps it is
told about.

## Scope & Trigger Paths

- **Flow integration**: `FeatureShowcaseStepController` in
  `chrome/browser/ui/views/profiles/first_run_flow_controller.{h,cc}`.
- **Eligibility checkers, step identifiers, metrics**:
  `chrome/browser/ui/views/profiles/feature_showcase/`.
- **WebUI controller, Mojo interfaces, page handlers**:
  `chrome/browser/ui/webui/feature_showcase/` and the `FeatureShowcaseUI` binder
  registrations in
  `chrome/browser/chrome_browser_interface_binders_webui_parts*.cc`.
- **WebUI frontend**: `chrome/browser/resources/feature_showcase/`.
- **Flags and switches**: `components/signin/public/base/signin_switches.{h,cc}`
  (`kFirstRunDesktopRevamp*`, `kForceFreFeatureShowcaseSteps`, per-step feature
  flags).
- **Histogram metadata**: `FeatureShowcase*` entries in
  `tools/metrics/histograms/metadata/profile/{histograms,enums}.xml`.
- **Tests**: `*_unittest.cc` next to the C++ above,
  `chrome/test/data/webui/feature_showcase/`,
  `chrome/browser/ui/views/profiles/first_run_feature_showcase_ui_browsertest.cc`,
  `chrome/browser/ui/views/profiles/first_run_interactive_uitest.cc`.

## Invariants

The checklist items in [review-checklist.md](references/review-checklist.md)
protect these; concrete classes are cited only as examples of a pattern.

1. **Desktop-only, feature-gated, uniform across platforms.** Shown only when
   `switches::IsFirstRunDesktopRevampEnabled(...)`; platform-specific steps gate
   inside `CheckEligibility()`, never at registration or in `BUILD.gn`.
2. **Single pre-WebUI evaluation.** Checkers are built in the controller
   constructor and run once by `FeatureShowcaseEligibilityTracker`; zero
   eligible steps means the WebUI is never created.
3. **Priority, conflicts, cap.** Registration order is priority; conflicts drop
   the lower-priority step; the list is capped at `kMaxFeatureShowcaseSteps`. A
   flag-gated step gates its conflict entry too.
4. **Bounded, at-most-once evaluation.** The tracker drops results after
   `kEvaluationTimeout`; checkers that keep state while pending override
   `OnTimeout()` to clean up and answer from local state.
5. **No assumptions about first-run signal availability; policy and existing
   state respected.** Late-arriving inputs are awaited with the timeout as the
   bound; a step is never offered when policy forbids it, the user already has
   it, or its action cannot be applied.
6. **Self-contained flow, clean teardown, reversible previews.** Everything
   acquired in `OnLoadFinished()` is released in `OnHidden()`; previews are
   reverted on decline and in the handler destructor unless accepted.
7. **Identifier contract, metrics, test parity.** One kebab-case identifier
   shared by constants, the metrics map, `app.html.ts`, and the pixel test;
   accept/decline recorded per step; checker, handler, and WebUI tests land with
   the code.

## Review Workflow

1. **Ground yourself in the CL's patchset, not `main`.** Companion files
   (binders, `BUILD.gn`, `feature_showcase_ui.cc`, `app.html.ts`, metrics XML,
   tests) are read at the patchset under review — in a presubmit the checkout
   *is* the patchset. If the CL description explicitly defers a layer (metrics,
   pixel coverage, tests) to a named follow-up, do not report it as missing.
2. **Classify the change** and load the matching reference:
   - Adding a step: start from the **Layer Map** in
     [review-checklist.md](references/review-checklist.md) — determine which of
     the 6 layers the CL covers and audit each covered layer.
   - Modifying the flow, the tracker, or an existing step:
     [architecture-and-flow.md](references/architecture-and-flow.md).
   - Any change: audit touched files against
     [review-checklist.md](references/review-checklist.md).
3. **Verify before asserting.** This skill names symbols, files, and histograms
   as they exist today; the code moves. Before a comment names a symbol, file,
   or histogram, or claims "X is missing", confirm it with a search of the
   patchset. Only check what the diff and the tree can prove; UX intent (copy,
   illustration variants, visual priority) is left to human reviewers. If you
   cannot verify, phrase it as a question.
4. **Inspect companion files.** A new identifier, enum value, `.mojom`
   interface, or WebUI component usually needs changes in at least two other
   layers (see the identifier contract).
5. **Report findings.** Group by severity: *Blocking / Correctness*, *Security /
   IPC / Policy*, *WebUI / A11y / Platform conventions*, *Metrics & Tests*,
   *Question*, *Nit*. Anchor each comment to a file and line in the diff and
   cite the checklist item. Post only findings and questions — never "checked,
   OK" or praise. Raise *what* is missing without prescribing *how* to build it.
   Generic C++/TypeScript style, anything a presubmit or formatter enforces
   (`Bug:` line, translation screenshots, histogram owners/expiry, clang-format,
   ESLint), and product/UX decisions are out of scope.

## Do Not Flag

- Proposals to consolidate per-step Mojo interfaces and factories into
  `feature_showcase.mojom`. One interface + factory per step is deliberate
  (design in crbug.com/498008195): steps share a wizard container but have no
  functional overlap and are meant to be owned by feature teams.

## Reference Guides

- [architecture-and-flow.md](references/architecture-and-flow.md): how it works
  — controller lifecycle, tracker mechanics, WebUI wiring, histograms, exemplar
  steps.
- [review-checklist.md](references/review-checklist.md): what to check — the
  per-layer file map for new steps and the audit list.
