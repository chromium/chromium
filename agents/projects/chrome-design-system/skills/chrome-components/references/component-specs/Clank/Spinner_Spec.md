# Spinner Spec (Clank)

This document maps the specifications of the **Spinner** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Spinner** is the circular progress indicator: it shows that something is
loading or that a process has an unknown wait time (per the Figma description).
Figma models one Material 3 style ring: a `primary` active arc and a
`secondaryContainer` track, with a gap between them. In code, the default is
`LoadingView` (a platform `ProgressBar` with Material show/hide timing). A few
features use the MDC `CircularProgressIndicator` directly.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                      | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Spinner`                                                                                                                                            | `LoadingView`<br>`CircularProgressIndicator` (MDC)                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Source Files**   | [Figma Link: `Spinner` (`38237:313`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=38237-313&m=dev) | [ui/android/java/src/org/chromium/ui/widget/LoadingView.java](//src/ui/android/java/src/org/chromium/ui/widget/LoadingView.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml) (`ProgressBarStyle`)<br>[components/browser_ui/theme/android/templates/res/values/themes.xml](//src/components/browser_ui/theme/android/templates/res/values/themes.xml) (`android:progressBarStyle`) |

> Source page:
> [`↳ Spinner + Progress Indicator` (`38310:633`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=38310-633),
> frame `Components/Spinner` (`43256:16230`). The page shows one light and one
> dark instance. The linear indicator on the same page has its own spec:
> [ProgressIndicator_Spec_Clank.md](./ProgressIndicator_Spec_Clank.md).
>
> `Spinner` is a single `COMPONENT` (not a set). Its only child,
> `circular-determinate progress indicator` (`54070:15518`), is a nested
> instance from a library that the Figma MCP can't resolve, so it was inspected
> through the exported SVG only.
>
> `CircularProgressIndicator` comes from Material Components
> (`com.google.android.material:material` 1.14.0). Its source isn't in the
> Chromium checkout, so its M3 Expressive default values below were **not
> verified**.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Spinner`: no variant properties. The Figma description says to "Hide / Show
  each progress bar segment to customize". The arc length is set by hand, and
  the component is named "circular-determinate".

| Feature / Variant           | Figma Component                           | Clank (Android)                                                                                                                                  |
| :-------------------------- | :---------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------- |
| **Indeterminate (default)** | Not modeled as an animation (static arc)  | `LoadingView` (indeterminate `ProgressBar`, `@style/ProgressBarStyle`); `CircularProgressIndicator` with `android:indeterminate="true"`          |
| **Determinate**             | Arc length set by hiding/showing segments | `CircularProgressIndicator.setProgressCompat()`. `LoadingView` is indeterminate only                                                             |
| **Track visible**           | Always shown (`secondaryContainer` ring)  | `LoadingView`: **no track**. `CircularProgressIndicator`: `app:indeterminateTrackVisible="true"` (e.g. facilitated payments, BNPL touch-to-fill) |
| **Show / hide timing**      | *(Not modeled)*                           | `LoadingView.showLoadingUi()` / `hideLoadingUi()`: 500ms show delay, shown for at least 500ms, alpha fade on hide                                |
| **Size**                    | One size: `36px` container, `28px` ring   | `LoadingView` default `ProgressBar` (`48dp`, framework); MDC usages set `app:indicatorSize` per feature (`24` / `32` / `80dp`)                   |
| **Dark theme**              | "Dark Theme" showcase (token mode switch) | Night-mode resolution of `?attr/colorPrimary` / `?attr/colorSecondaryContainer`                                                                  |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute             | Figma Design Token                                                           | Clank (Android)                                                                                                                                                                                                               |
| :--------------------------- | :--------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Active indicator color**   | `primary` (light `#0b57d0` / dark `#a8c7fa`, from the SVG export)            | `LoadingView`: `android:indeterminateTint` = `@macro/default_icon_color_accent1` → `?attr/colorPrimary` ✅. MDC usages: `app:indicatorColor="@macro/default_icon_color_accent1"`                                              |
| **Track color**              | `secondaryContainer` (light `#c2e7ff` / dark `#004a77`, from the SVG export) | `LoadingView`: none (no track). MDC: M3 default track color (**not verified**, expected `?attr/colorSecondaryContainer`)                                                                                                      |
| **Container size**           | `36 × 36px`                                                                  | `LoadingView`: framework `Widget.Material.ProgressBar` (`48dp`, not verified in-tree). MDC: `app:indicatorSize` per usage                                                                                                     |
| **Container padding**        | `--clank/shape/spacing/4` (`4px`)                                            | `LoadingView`: built into the framework drawable. MDC: `app:indicatorInset` (M3 default, not verified)                                                                                                                        |
| **Ring diameter**            | `28px`                                                                       | MDC per usage: `@dimen/bnpl_progress_bar_size` (`24dp`), `@dimen/message_progress_indicator_size` (`32dp`), `@dimen/tab_grid_thumbnail_fetch_spinner_size` (`32dp`), `@dimen/facilitated_payments_progress_bar_size` (`80dp`) |
| **Stroke / track thickness** | About `2.8px` (outer radius `14`, inner `11.2`, measured from the SVG)       | MDC per usage: `2.5dp` (BNPL, tab grid), `3dp` (messages), `4dp` (facilitated payments). `LoadingView`: framework stroke (not verified)                                                                                       |
| **Track ↔ indicator gap**    | Visible gap at both ends of the active arc (in the SVG geometry, no token)   | MDC M3 Expressive `indicatorTrackGapSize` (not verified). `LoadingView`: N/A                                                                                                                                                  |
| **Stroke cap**               | Rounded                                                                      | MDC: `app:trackCornerRadius` (M3 default, not verified). Framework spinner: square/round per platform drawable                                                                                                                |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Two implementations

- **`LoadingView`** is the shared Chromium widget (14 layouts). It extends the
  platform `ProgressBar`, styled by the app theme's `android:progressBarStyle` →
  `@style/ProgressBarStyle` (parent `Widget.AppCompat.ProgressBar`). This draws
  the older Material spinner: a single `colorPrimary` arc with **no track and no
  gap**, at the framework default size.
- **MDC `CircularProgressIndicator`** is used directly in a few features
  (messages, tab grid, vertical tabs, autofill / payments). Each sets its own
  size and thickness. Only some set `app:indeterminateTrackVisible="true"`. This
  is the only path that can render the Figma track and gap.

### 2. No shared size or style tokens

- Figma has one size (`28px` ring, `2.8px` stroke). Code has no shared `@dimen`
  or `@style` for it, so each MDC usage hardcodes a size and thickness, and none
  match `28 / 2.8`.
- No theme-level `circularProgressIndicatorStyle` override exists, so MDC usages
  without explicit attributes fall back to the M3 Expressive defaults.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Track missing on `LoadingView` (verified)

Figma always shows a `secondaryContainer` track. `LoadingView` only sets
`indeterminateTint`, so no track is drawn.

### 2. Thickness differs per usage

Figma: about `2.8px`. Code: `2.5dp`, `3dp` or `4dp` depending on the feature.

### 3. Size

Figma: `28px` ring in a `36px` box. `LoadingView`: framework `48dp`
(`progressBarStyleSmall` / `Large` are used in a few layouts). MDC: `24`, `32`
or `80dp`.

### 4. Static determinate arc vs. animated indeterminate spinner

Figma only shows a static arc (named "circular-determinate"). It doesn't specify
indeterminate motion, timing, or show/hide delay. Code's `LoadingView` follows
Material's 500ms delay and 500ms minimum display time.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use a spinner when the wait time is unknown, or the area is too small for a
  linear indicator. For page-level or measurable progress, use the
  [Progress Indicator](./ProgressIndicator_Spec_Clank.md).
- Prefer `LoadingView` with `showLoadingUi()` / `hideLoadingUi()` so short loads
  don't flash the spinner.
- To match Figma (track and gap), use `CircularProgressIndicator` with
  `app:indeterminateTrackVisible="true"` and
  `app:indicatorColor="@macro/default_icon_color_accent1"`.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Spinners aren't focusable or interactive. Announce loading state through a
  `contentDescription` on the spinner, or an accessibility announcement on the
  container (e.g. "Loading").
- Hide the spinner (`GONE`) when it isn't loading, so TalkBack doesn't announce
  it.
- Respect the system's reduced-animation settings. The MDC indicator and
  framework `ProgressBar` handle animator scale.

### 3. Icon Usage Guidelines

- Not applicable. Don't put icons inside the spinner. If a spinner replaces a
  button's icon or label (e.g. `SpinnerButtonWrapper`), keep the button's size
  fixed.
