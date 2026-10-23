---
name: pixel-test
description: Create or update a Chromium desktop pixel test end-to-end up to the point of human Skia Gold triage. Covers writing the Screenshot step (Kombucha) or InvokeUi_ test (DialogBrowserTest), choosing a stable surface, registering the test in testing/buildbot/filters/pixel_tests.filter with a pattern that actually matches, the baseline_cl two-upload sequence, local sanity capture, what tryjobs do and do not verify, and bumping baselines on later intentional UI changes. Use when asked to add pixel/visual-regression coverage for a dialog, bubble, menu, toolbar region or WebUI surface, or when a pixel test fails after a UI change.
---

# Pixel Tests in Chromium

A pixel test screenshots a UI element inside a `browser_tests` /
`interactive_ui_tests` test and compares it to an approved golden image in
**Skia Gold**. It catches *unintended* visual changes. This skill automates
everything a developer can delegate; it stops where a human must act.

> [!IMPORTANT] **What stays manual.** Approving a new or changed golden in Skia
> Gold is a human decision made in the Gold web UI
> (<https://chrome-gold.skia.org/?corpus=gtest-pixeltests>). Never claim a
> baseline was approved, and never mark a pixel test "done" until the developer
> has triaged it. Pixel jobs currently run only on **Windows** bots.

______________________________________________________________________

## 1. Sources of Truth

- [`docs/testing/pixel_tests.md`](/docs/testing/pixel_tests.md) — the canonical
  guide (both APIs, flags, baseline procedure).
- Kombucha verbs `Screenshot` / `ScreenshotSurface` / `ScreenshotWebUi`:
  [`chrome/test/interaction/interactive_browser_window_test.h`](/chrome/test/interaction/interactive_browser_window_test.h)
- Legacy API `DialogBrowserTest` (`InvokeUi_*`, `set_baseline`, automatic dialog
  screenshot) and `UiBrowserTest` (`InvokeUi_*`; you call `VerifyPixelUi()`
  yourself in `VerifyUi()`):
  [`chrome/browser/ui/test/test_browser_dialog.h`](/chrome/browser/ui/test/test_browser_dialog.h),
  [`chrome/browser/ui/test/test_browser_ui.h`](/chrome/browser/ui/test/test_browser_ui.h)
- Platform gating (`SUPPORTS_PIXEL_TEST` = Win, Linux):
  [`chrome/browser/ui/test/test_browser_ui.cc`](/chrome/browser/ui/test/test_browser_ui.cc)
- Local PNG / Gold-bypass switches:
  [`ui/base/test/skia_gold_pixel_diff.cc`](/ui/base/test/skia_gold_pixel_diff.cc)
- Allowlist consumed by the pixel jobs:
  [`testing/buildbot/filters/pixel_tests.filter`](/testing/buildbot/filters/pixel_tests.filter)
  (syntax:
  [`testing/buildbot/filters/README.md`](/testing/buildbot/filters/README.md);
  wired, together with the tryjob Gold bypass, in
  [`infra/config/targets/tests.star`](/infra/config/targets/tests.star))
- Glossary entry on Skia Gold:
  [`docs/ui/learn/glossary.md`](/docs/ui/learn/glossary.md)

If this skill and those disagree, they win.

______________________________________________________________________

## 2. Decide Whether a Pixel Test Is Appropriate

Promote a surface to a pixel test only if it is **deterministic**:

| Good candidates                                         | Do **not** pixel-test                                                   |
| :------------------------------------------------------ | :---------------------------------------------------------------------- |
| Dialogs, bubbles, toasts, menus with fixed strings      | Anything showing dates/times, counters, progress, animations mid-flight |
| Toolbar / tab-strip regions with controlled tab content | Avatars, favicons from the network, live web content                    |
| WebUI nodes with mocked data                            | Surfaces whose size depends on screen/DPI you don't force               |

Mitigations when a surface is *mostly* stable: screenshot a sub-element or a
`clip_rect` instead of the whole surface; force state in the test (fixed
strings, `--force-device-scale-factor=1`, test images via
`gfx::test::CreateImage`); finish animations before capturing
(`gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION` or wait for
`is_animating()` to be false).

If the surface cannot be made stable, stop and say so; a flaky pixel test is
worse than none.

______________________________________________________________________

## 3. Step 1 — Write the Capture

### Option A (preferred): Kombucha `Screenshot` in an existing or new sequence

```cpp
// Baseline Gerrit CL number of the most recent CL that changed this UI.
// Placeholder until the CL exists (see Step 4).
constexpr char kScreenshotBaselineCL[] = "0";

IN_PROC_BROWSER_TEST_F(MyDialogUiTest, OpenAndVerifyContents) {
  RunTestSequence(
      // Required: the same test also runs on non-pixel jobs and on Mac,
      // where Screenshot reports kKnownIncompatible. Without this the test
      // FAILS there; with it the step is skipped.
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not supported in all environments."),
      PressButton(kMyToolbarButtonElementId),
      WaitForShow(MyDialog::kDialogElementId),
      ScreenshotSurface(MyDialog::kDialogElementId,
                        /*screenshot_name=*/"whole_dialog",
                        /*baseline_cl=*/kScreenshotBaselineCL));
}
```

Rules:

- One `SetOnIncompatibleAction(kIgnoreAndContinue, ...)` before the first
  screenshot. Use `kSkipTest` only if the test has no other value off-pixel.
- Each screenshot in a test needs a unique `screenshot_name`.
- `Screenshot` = exactly the element (optional 4th arg `clip_rect`);
  `ScreenshotSurface` = its whole widget;
  `ScreenshotWebUi(tab_id, DeepQuery, ...)` = a DOM node.
- The verb clears focus and parks the mouse by design; don't rely on hover or
  focus rings.
- Capture after the surface is fully shown/laid out (`WaitForShow`,
  `WaitForWebContentsPainted` for WebUI).

### Option B (legacy, only when the fixture already is one)

For `DialogBrowserTest`, tests named `InvokeUi_*` whose body is
`ShowAndVerifyUi();` are screenshotted automatically under the pixel flags
(golden name = `<Suite>_<Test>_<baseline>`). Call `set_baseline("<cl>")` before
`ShowAndVerifyUi()`; it is reset after each call.

For plain `UiBrowserTest` there is no `set_baseline()` and nothing is automatic:
call `VerifyPixelUi(...)` from your `VerifyUi()` override and bake the CL number
into `screenshot_name`.

Do not convert Kombucha tests to this API; the docs mark it as legacy.

______________________________________________________________________

## 4. Step 2 — Register in `pixel_tests.filter`

The pixel jobs run **only** tests matching
[`testing/buildbot/filters/pixel_tests.filter`](/testing/buildbot/filters/pixel_tests.filter).
A `Screenshot` step in a test that is not matched never compares a pixel, and
nothing warns you. Several suites in the tree are in that state today, so verify
rather than assume.

1. Determine the **exact gtest suite name** as the binary reports it
   (`out/Default/browser_tests --gtest_list_tests --gtest_filter='MySuite*'`).
   For parameterized suites the name may be `Instantiation/MySuite` and tests
   `Name/0`; for `TEST_F` it is just `MySuite`.
2. Add a line in alphabetical order in the main (positive) block:
   - `MySuite.*` to include every test in the suite (preferred when all tests
     are pixel-safe), or
   - `MySuite.SpecificTest` for a single test, or
   - a `*` prefix (`*MySuite.*`) when an instantiation prefix exists.
3. Keep the trailing `-Suite.Test` block for temporary disables only, with a
   `TODO(crbug.com/...)`.
4. **Verify the pattern matches** before uploading. `--gtest_list_tests`
   bypasses the test launcher and **ignores** `--test-launcher-filter-file`, so
   do not use it for this. Run through the launcher with both filters (the
   launcher intersects them):
   ```sh
   out/Default/browser_tests --gtest_filter='MySuite.*' \
     --test-launcher-filter-file=testing/buildbot/filters/pixel_tests.filter \
     --test-launcher-retry-limit=0
   ```
   If the output says `WARNING: No matching tests to run.` (or your test never
   appears as `[n/N] MySuite.MyTest`), the filter pattern is wrong.

Remember the test also keeps running in plain `browser_tests` /
`interactive_ui_tests`; only include tests that are safe there too.

______________________________________________________________________

## 5. Step 3 — Local Sanity Capture (Linux/Windows)

Before involving bots, look at the image once. Mirror the bot flags:

```sh
autoninja -C out/Default browser_tests
testing/xvfb.py out/Default/browser_tests --gtest_filter=MySuite.MyTest \
  --browser-ui-tests-verify-pixels --enable-pixel-output-in-tests \
  --bypass-skia-gold-functionality \
  --skia-gold-local-png-write-directory=/tmp/pixel_shots \
  --force-device-scale-factor=1 --test-launcher-retry-limit=0 \
  --test-launcher-jobs=1
```

(On Windows, drop `testing/xvfb.py`; the developer runs it in their own
terminal. Windows `ShowAndVerifyUi()` tests are skipped when the host is in dark
mode unless `--force-dark-mode` is passed.) Open the PNG(s) in
`/tmp/pixel_shots` and confirm they show the intended surface, fully rendered,
with nothing volatile. `Screenshot` cannot capture on macOS
(`SUPPORTS_PIXEL_TEST` is Win/Linux only); with `kIgnoreAndContinue` it is
skipped there.

______________________________________________________________________

## 6. Step 4 — The `baseline_cl` Two-Upload Sequence

`baseline_cl` must be the number of the CL that introduces or changes the image,
so older goldens are retired. The number does not exist until the first upload,
so:

1. Upload with the placeholder:
   ```sh
   git cl upload
   ```
2. Read the CL number from the upload output
   (`https://chromium-review.googlesource.com/c/chromium/src/+/<NUMBER>`) or
   `git cl issue`.
3. Replace **every** placeholder you introduced (`kScreenshotBaselineCL`,
   literal `"0"`s, `set_baseline("...")`) with `"<NUMBER>"`. Do not reuse a
   number copied from a neighbouring test.
4. Re-upload:
   ```sh
   git cl upload -m "Set pixel test baseline CL"
   ```
5. Grep to confirm no placeholder remains:
   ```sh
   git grep -n -E 'baseline_cl=\*/\s*"0"|set_baseline\("0"\)' -- '<your files>'
   ```

Do steps 1–5 as one uninterrupted sequence; forgetting step 4 is the most common
pixel-test mistake.

______________________________________________________________________

## 7. Hand-off: Bots and Skia Gold Triage (human)

1. **Check what tryjobs actually verify.** As of this writing,
   [`infra/config/targets/tests.star`](/infra/config/targets/tests.star) adds
   `--bypass-skia-gold-functionality` to the `precommit_args` of
   `pixel_browser_tests` / `pixel_interactive_ui_tests`
   (`TODO(crbug.com/574560342)`). While that bypass is present, **tryjobs do not
   upload to or compare against Gold**: the pixel job passes regardless of the
   image and prints no Gold triage link. Grep for the switch in `tests.star` and
   tell the developer which situation applies.
2. **If the bypass is present:** the golden is first produced by post-submit CI
   on the Windows builders *after landing*. The developer must then open
   <https://chrome-gold.skia.org/?corpus=gtest-pixeltests>, find the untriaged
   image for the test, and approve or reject it. Until they do, the CI pixel job
   is red for that test.
3. **If the bypass has been removed:** a CQ dry run (`git cl try`) includes the
   Windows pixel jobs; the first run fails for a new/changed image and the log
   contains a Gold link; the developer approves in Gold and re-runs to go green
   before landing.
4. In both cases approval is a human action; never claim it was done.

______________________________________________________________________

## 8. Later Intentional UI Changes

When a pixel test fails after a deliberate UI change:

1. Confirm from the Gold diff that the change is the intended one and nothing
   else moved.
2. Bump `baseline_cl` (or `set_baseline`) to the **current** CL's number via the
   Step 4 sequence.
3. Developer re-approves in Gold.

If the failure is *not* intended, fix the UI; do not bump the baseline to make
it pass.

______________________________________________________________________

## 9. Troubleshooting

| Symptom                                                | Cause / fix                                                                                                                                                        |
| :----------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Pixel job never ran / never compared my test           | Not matched by `pixel_tests.filter` (§4 step 4); test lives in a binary without a pixel job; or the job ran with `--bypass-skia-gold-functionality` (tryjobs, §7). |
| Test fails on non-pixel bots with "unsupported action" | Missing `SetOnIncompatibleAction(kIgnoreAndContinue, ...)` before the first `Screenshot`.                                                                          |
| Gold shows the image under an old CL number            | Placeholder/old `baseline_cl` left in; redo §6.                                                                                                                    |
| Flaky Gold diffs of a few pixels                       | Volatile content or animation; see §2 mitigations. Don't loosen the match algorithm without owner agreement.                                                       |
| Works locally on Linux, differs on Windows bot         | Expected: goldens are per-platform; the bot's image is the one you approve.                                                                                        |
| Mac developer can't reproduce                          | `Screenshot` cannot capture on Mac; rely on the Windows CI run and the Gold UI.                                                                                    |
