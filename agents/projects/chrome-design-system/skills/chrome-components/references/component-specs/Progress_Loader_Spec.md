# Component Spec: Progress Loader

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Progress Loader** component across
Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The Progress Loader component provides visual feedback for operations taking an
indeterminate or determinate amount of time. It comes in three main variants: a
linear Progress Indicator (for determinate and indeterminate tracking), a
standalone Spinner (Throbber) for localized loading, and a Spinner Button which
embeds a loading spinner directly within an actionable button.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                                                                                                                                                                                                              | WebUI (Desktop)                                                                                                                                                                                                                                              | Clank (Android)                                                                                                                    |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Progress Loader`                                                                                                                   | `ProgressBar` / `Throbber` / `MdTextButtonWithSpinner`                                                                                                                                                                                                                           | `<cr-progress>` / `.spinner`                                                                                                                                                                                                                                 | `LoadingView` / `LinearProgressIndicator` / `CircularProgressIndicator`                                                            |
| **Source Files**   | [Figma Link: `6191:3526`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=6191-3526) | [`ui/views/controls/progress_bar.h`](//src/ui/views/controls/progress_bar.h)<br>[`ui/views/controls/throbber.h`](//src/ui/views/controls/throbber.h)<br>[`ui/views/controls/button/md_text_button_with_spinner.h`](//src/ui/views/controls/button/md_text_button_with_spinner.h) | [`ui/webui/resources/cr_elements/cr_progress/cr_progress.ts`](//src/ui/webui/resources/cr_elements/cr_progress/cr_progress.ts)<br>[`ui/webui/resources/cr_elements/cr_spinner_style_lit.css`](//src/ui/webui/resources/cr_elements/cr_spinner_style_lit.css) | [`ui/android/java/src/org/chromium/ui/widget/LoadingView.java`](//src/ui/android/java/src/org/chromium/ui/widget/LoadingView.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant          | C++ `views` Classes                           | WebUI Element/Class                                       | Clank (Android) Implementation              |
| :--------------------- | :-------------------------------------------- | :-------------------------------------------------------- | :------------------------------------------ |
| **Progress Indicator** | `views::ProgressBar`                          | `<cr-progress>`                                           | `LinearProgressIndicator` / `ProgressBar`   |
| **Spinner**            | `views::Throbber` / `views::SmoothedThrobber` | `<div class="spinner">` (uses `cr_spinner_style_lit.css`) | `LoadingView` / `CircularProgressIndicator` |
| **Spinner Button**     | `views::MdTextButtonWithSpinner`              | `<cr-button>` composed with `<div class="spinner">`       | `ButtonCompat` showing `ProgressBar`        |

______________________________________________________________________

## 3. Component States

| Interactive State            | Figma                                 | C++                                            | WebUI                                         | Clank (Android)                                        |
| :--------------------------- | :------------------------------------ | :--------------------------------------------- | :-------------------------------------------- | :----------------------------------------------------- |
| **Determinate (Progress)**   | Not explicitly shown, defaults to 30% | `ProgressBar::SetValue(double)`                | `<cr-progress value="X" min="Y" max="Z">`     | `setProgress(int)` / `setProgressCompat(int, boolean)` |
| **Indeterminate (Progress)** | N/A                                   | Calculated internally when value < 0           | `<cr-progress indeterminate>`                 | `setIndeterminate(true)`                               |
| **Running (Spinner)**        | Animated image                        | `Throbber::Start()`                            | CSS animation automatically active            | `LoadingView.showLoadingUI()`                          |
| **Paused/Stopped**           | N/A                                   | `ProgressBar::SetPaused()`, `Throbber::Stop()` | Removed from DOM, or `<cr-progress disabled>` | `LoadingView.hideLoadingUI()`                          |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type                 | Figma Property  | C++ Views                                        | WebUI Variable / CSS            | Clank (Android) Equivalent                                   |
| :------------------------- | :-------------- | :----------------------------------------------- | :------------------------------ | :----------------------------------------------------------- |
| **Progress Active Color**  | Primary         | `ProgressBar::SetForegroundColorId()`            | `--cr-progress-active-color`    | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| **Progress Track Color**   | Tonal Container | `ProgressBar::SetBackgroundColorId()`            | `--cr-progress-container-color` | `?attr/colorSurfaceContainerHighest`                         |
| **Spinner Color**          | Primary         | `Throbber::SetColorId()`                         | `--cr-spinner-color`            | `?attr/colorPrimary`                                         |
| **Progress Height**        | `4px`           | `ProgressBar::preferred_height_ = 5`             | `--cr-progress-height: 4px`     | `@dimen/toolbar_progress_bar_height` (`4dp`)                 |
| **Spinner Size (Default)** | `28px`          | `Throbber::kDefaultDiameter = 16`                | `--cr-spinner-size: 28px`       | `@dimen/loading_view_spinner_size` (`32dp`)                  |
| **Spinner Size (Button)**  | `16px`          | `MdTextButtonWithSpinner::kSpinnerDiameter = 20` | Native `.spinner` resized       | `20dp`                                                       |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Smoothed Loading Implementations**: Both C++ (`views::SmoothedThrobber`) and
  Clank
  ([`LoadingView`](//src/ui/android/java/src/org/chromium/ui/widget/LoadingView.java))
  incorporate 500ms debounce/delay algorithms to prevent UI flashing for rapid
  operations. WebUI relies on client JavaScript implementation.
- **Component Composition vs Dedicated Classes**: Figma provides "Spinner
  Button" as a built-in variant. In C++, this is handled via
  `views::MdTextButtonWithSpinner`. In Clank and WebUI, loading spinners are
  composed directly into parent button containers.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Default Sizing Gaps**: WebUI specifies a default `28px` spinner, C++
  `Throbber` defaults to `16px`, and Clank's `LoadingView` defaults to `32dp`.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Determinate vs Indeterminate**: Use `ProgressBar` /
  `LinearProgressIndicator` when percentage completion is known. Use `Throbber`
  or `LoadingView` when duration is uncertain.
- **Throbber Contexts**: Place a spinner in form submission views to communicate
  progress.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: `<cr-progress>` natively injects `role="progressbar"`.
- **C++ Views**: `ProgressBar` uses `MaybeNotifyAccessibilityValueChanged()`.
- **Clank (Android)**: `LoadingView` triggers TalkBack accessibility
  announcements upon show/hide.
