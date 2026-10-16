# Progress Indicator Spec (Clank)

This document maps the specifications of the **Progress Indicator** (Clank)
component across Figma and code.

______________________________________________________________________

## Overview

The **Progress Indicator** is the linear, determinate Material 3 progress bar.
It has a `primary` active segment, a `secondaryContainer` track separated from
it by a gap, and a `primary` stop dot at the trailing end. In code, generic
linear progress uses Chromium's custom `MaterialProgressBar` (a flat rectangle,
no gap or stop). Page-load progress uses `ToolbarProgressBar`, which adds
rounded ends and a gap behind the `AndroidProgressBarVisualUpdate` flag.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Progress Indicator`                                                                                                                                                           | `MaterialProgressBar`<br>`ToolbarProgressBar` (page load; extends `ClipDrawableProgressBar`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Source Files**   | [Figma Link: `Components/Progress Indicator` (`63417:70133`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=63417-70133&m=dev) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialProgressBar.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialProgressBar.java)<br>[chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/ToolbarProgressBar.java](//src/chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/ToolbarProgressBar.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ClipDrawableProgressBar.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ClipDrawableProgressBar.java) |

> Source page:
> [`↳ Spinner + Progress Indicator` (`38310:633`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=38310-633).
> The circular indicator on the same page has its own spec:
> [Spinner_Spec_Clank.md](./Spinner_Spec_Clank.md).
>
> **The main `Progress Indicator` component couldn't be resolved.** The page
> only has 12 `Progress Indicator` instances (6 progress values × light/dark,
> e.g. `63417:70185`–`63417:70190`). Their internal node IDs (`54070:*`) point
> to a library component that the Figma MCP can't fetch, so the link above goes
> to the documentation frame. Variant property names and values couldn't be
> read.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Progress Indicator`: **unknown** (the main component isn't accessible). The
  instances show 6 progress values: `0%` (track and stop only), about `10%`,
  `20%`, `50%`, `80%`, and `100%` (active only, no track or stop). The active
  indicator is made of a `Segment - start` (`40px`) plus a flexible `Segment`.

| Feature / Variant      | Figma Component                                   | Clank (Android)                                                                                                                                      |
| :--------------------- | :------------------------------------------------ | :--------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Determinate**        | 6 progress instances                              | `MaterialProgressBar.setProgress(int 0–100)`; `ToolbarProgressBar.setProgress(float)`                                                                |
| **Indeterminate**      | *(Not modeled)*                                   | `MaterialProgressBar.setIndeterminate(true)` (3s loop); `ToolbarProgressBar` animating view (`ToolbarProgressBarAnimatingView`)                      |
| **Secondary / buffer** | *(Not modeled)*                                   | `MaterialProgressBar.setSecondaryProgress()` + `colorSecondaryProgress` (`@color/progress_bar_secondary`)                                            |
| **Track ↔ active gap** | Present (about `4px` visible)                     | `MaterialProgressBar`: no. `ToolbarProgressBar`: yes, only with `AndroidProgressBarVisualUpdate` (**disabled by default**); gap = bar height (`4dp`) |
| **Stop indicator**     | `4px` `primary` dot at the trailing end           | Not implemented in either class                                                                                                                      |
| **Rounded ends**       | Track `2px` radius, active stroke with round caps | `MaterialProgressBar`: square (`drawRect`). `ToolbarProgressBar`: `height / 2` corner radius with the flag on                                        |
| **RTL**                | *(Not modeled)*                                   | Both mirror for `LAYOUT_DIRECTION_RTL`                                                                                                               |
| **Dark theme**         | "Dark Theme" showcase (token mode switch)         | Night-mode resolution of `?attr/colorPrimary` / `?attr/colorSecondaryContainer`                                                                      |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute       | Figma Design Token                                                             | Clank (Android)                                                                                                                                                                                                                              |
| :--------------------- | :----------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Active indicator**   | `--clank/sys/color/primary` (light `#0b57d0` / dark `#a8c7fa`)                 | `SemanticColorUtils.getProgressBarForeground()` → `getDefaultControlColorActive()` → `?attr/colorPrimary` ✅ (both classes)                                                                                                                  |
| **Track**              | `--clank/sys/color/secondarycontainer, -sc` (light `#c2e7ff` / dark `#004a77`) | `@color/progress_bar_bg_color_list` → `@macro/progress_bar_background` → `?attr/colorSecondaryContainer` ✅; `ToolbarProgressBar` with the flag: `SemanticColorUtils.getProgressBarTrackColor()` (same attr) ✅                              |
| **Stop indicator**     | `--clank/sys/color/primary`, `4 × 4px`, radius `26px` (circle)                 | Not implemented                                                                                                                                                                                                                              |
| **Track thickness**    | `4px`, corner radius `2px`                                                     | `MaterialProgressBar`: layout height (`4dp` in `passwords_progress_dialog.xml`, `device_lock_view.xml`), square corners. Toolbar: `@dimen/toolbar_progress_bar_height` `2dp`, or `toolbar_progress_bar_increased_height` `4dp` with the flag |
| **Active thickness**   | `4px` stroke, round caps                                                       | Same as track height (fills the full view height)                                                                                                                                                                                            |
| **Component height**   | `12px` frame (4px bar centered)                                                | View height equals bar height (no vertical padding)                                                                                                                                                                                          |
| **Gap**                | `6px` `track-and-stop` padding minus the `2px` round cap ≈ `4px` visible       | Toolbar (flag on): gap = `mProgressBarHeight` (`4dp`), filled with `setProgressGapBackgroundColor(toolbar color)`. `MaterialProgressBar`: none                                                                                               |
| **Secondary progress** | *(Not modeled)*                                                                | `@color/progress_bar_secondary` → `@color/baseline_neutral_40` (static, not theme-aware)                                                                                                                                                     |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. No MDC `LinearProgressIndicator`

- Chromium doesn't use the MDC `LinearProgressIndicator`, which supports gap,
  stop indicator and rounded ends natively. Instead it has two custom classes:
  `MaterialProgressBar` (2016, `View` + `Canvas.drawRect`) and
  `ClipDrawableProgressBar` / `ToolbarProgressBar` (page-load, composited). A
  handful of layouts also use the platform
  `?android:attr/progressBarStyleHorizontal`.

### 2. M3 visuals gated on a disabled flag

- Rounded ends, the `4dp` height, the `secondaryContainer` track color via
  `getProgressBarTrackColor()`, and the gap only appear in `ToolbarProgressBar`
  when `ChromeFeatureList.sAndroidProgressBarVisualUpdate` is on. It's
  `FEATURE_DISABLED_BY_DEFAULT`
  (`chrome/browser/flags/android/chrome_feature_list.cc`). By default the
  toolbar bar is `2dp`, square, with no gap.

### 3. Gap is painted, not transparent

- `ToolbarProgressBar` paints the gap with the toolbar color
  (`setProgressGapBackgroundColor`) instead of leaving it transparent. This
  works on a solid toolbar but would break over non-uniform backgrounds.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Stop indicator missing (verified)

Figma's trailing `4px` `primary` dot isn't drawn by either class.

### 2. `MaterialProgressBar` is flat M2 style (verified)

No gap, no rounded ends, no stop. Only the colors match Figma.

### 3. Toolbar height

Figma: `4px`. `ToolbarProgressBar` default: `2dp` (`4dp` only with the flag).

### 4. Possible bug: secondary progress drawn on top

`MaterialProgressBar.drawDeterminateBar()` draws the secondary progress
**after** the primary progress. When `secondaryProgress > progress` (the usual
buffer case), the secondary color covers the primary bar. Nothing in Chromium
calls `setSecondaryProgress` on this class today, so it's latent.

### 5. Static secondary color

`@color/progress_bar_secondary` is `baseline_neutral_40` and doesn't follow
dynamic color or night mode.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use the linear indicator when progress is measurable (downloads, exports,
  multi-step flows), or for page-load progress. Use the
  [Spinner](./Spinner_Spec_Clank.md) for unknown waits in small areas.
- Use `MaterialProgressBar` for in-content progress, at `4dp` height, full
  width. Don't style it with ad-hoc colors. The defaults map to `primary` /
  `secondaryContainer`.
- Switch to indeterminate only when progress can't be measured. Switch back to
  determinate as soon as it can.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- `MaterialProgressBar` is a plain `View`. It doesn't expose
  `AccessibilityNodeInfo.RangeInfo` or a progress role. Give it a
  `contentDescription`, or announce progress milestones from the hosting view
  (e.g. via the dialog title / `announceForAccessibility`).
- The bar isn't focusable or interactive. It doesn't need a touch target.
- RTL is handled by both classes (the bar fills from the end).

### 3. Icon Usage Guidelines

- Not applicable.
