# Component Spec: Progress Loader

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Progress Loader** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The Progress Loader component provides visual feedback for operations taking an indeterminate or determinate amount of time. It comes in three main variants: a linear Progress Indicator (for determinate and indeterminate tracking), a standalone Spinner (Throbber) for localized loading, and a Spinner Button which embeds a loading spinner directly within an actionable button.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Progress Loader` | `ProgressBar` / `Throbber` / `MdTextButtonWithSpinner` | `<cr-progress>` / `.spinner` |
| **Source Files** | [Figma Link: `6191:3526`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=6191-3526) | [`ui/views/controls/progress_bar.h`](//src/ui/views/controls/progress_bar.h)<br>[`ui/views/controls/throbber.h`](//src/ui/views/controls/throbber.h)<br>[`ui/views/controls/button/md_text_button_with_spinner.h`](//src/ui/views/controls/button/md_text_button_with_spinner.h) | [`ui/webui/resources/cr_elements/cr_progress/cr_progress.ts`](//src/ui/webui/resources/cr_elements/cr_progress/cr_progress.ts)<br>[`ui/webui/resources/cr_elements/cr_spinner_style_lit.css`](//src/ui/webui/resources/cr_elements/cr_spinner_style_lit.css) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ `views` Classes | WebUI Element/Class |
| :--- | :--- | :--- |
| **Progress Indicator** | `views::ProgressBar` | `<cr-progress>` |
| **Spinner** | `views::Throbber` / `views::SmoothedThrobber` | `<div class="spinner">` (uses `cr_spinner_style_lit.css`) |
| **Spinner Button** | `views::MdTextButtonWithSpinner` | `<cr-button>` composed with `<div class="spinner">` |

---

## 3. Component States

| Interactive State | Figma | C++ | WebUI |
| :--- | :--- | :--- | :--- |
| **Determinate (Progress)** | Not explicitly shown, defaults to 30% | `ProgressBar::SetValue(double)` | `<cr-progress value="X" min="Y" max="Z">` |
| **Indeterminate (Progress)** | N/A | Calculated internally when value < 0 | `<cr-progress indeterminate>` |
| **Running (Spinner)** | Animated image | `Throbber::Start()` | CSS animation (mask or keyframes) automatically active |
| **Paused/Stopped** | N/A | `ProgressBar::SetPaused()`, `Throbber::Stop()` | Removed from DOM, or `<cr-progress disabled>` |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Progress Active Color** | Primary | `ProgressBar::SetForegroundColorId()` | `--cr-progress-active-color` |
| **Progress Track Color** | Tonal Container | `ProgressBar::SetBackgroundColorId()` | `--cr-progress-container-color` |
| **Spinner Color** | Primary | `Throbber::SetColorId()` | `--cr-spinner-color` |
| **Progress Height** | `4px` | `ProgressBar::preferred_height_ = 5` | `--cr-progress-height: 4px` |
| **Spinner Size (Default)** | `28px` | `Throbber::kDefaultDiameter = 16` | `--cr-spinner-size: 28px` |
| **Spinner Size (Button)** | `16px` | `MdTextButtonWithSpinner::kSpinnerDiameter = 20` | Native `.spinner` resized or native button layout |

---

## 5. Architectural & Implementation Gaps

* **Component Composition vs Dedicated Classes**: Figma provides "Spinner Button" as a built-in variant of the `Progress Loader`. In C++, this is handled via a dedicated wrapper class, `views::MdTextButtonWithSpinner`, which manages laying out the Throbber inline with the text. In WebUI, there is no pre-packaged spinner button; clients are expected to project a `<div class="spinner">` natively into a standard `<cr-button>`.
* **Spinner Asset Handling**: The WebUI `.spinner` class uses a CSS `mask-image` over `throbber_small.svg` to allow native CSS background-color tinting (e.g., `--cr-spinner-color`). The C++ `Throbber` draws frames sequentially and calculates custom vector painting.
* **Indeterminate Bar Animation**: The WebUI `<cr-progress indeterminate>` drives its loading animation entirely via native CSS keyframes (`indeterminate-bar`, `indeterminate-splitter`), while the C++ `ProgressBar` uses a scheduled `gfx::LinearAnimation` delegating to `OnPaintIndeterminate()`.

---

## 6. Styling, Variants, Features and States Mismatches

* **Default Sizing Gaps**: The sizing between platforms is slightly out of sync natively. WebUI specifies a default `28px` spinner while C++ `Throbber` defaults to `16px`. Furthermore, the C++ `ProgressBar` uses a hardcoded default `preferred_height_ = 5`, while WebUI aligns closer to Figma with a `4px` default. Developers must explicitly set dimensions based on layout parameters.
* **Smoothed Throbber Logic**: C++ provides a specialized `views::SmoothedThrobber` that intercepts and delays the start and stop of a throbber animation to prevent "flash" loading screens on fast operations. Neither Figma nor WebUI have a native equivalent for this smoothed loading logic without custom JavaScript wrapping.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Determinate vs Indeterminate**: Use `ProgressBar` when the system knows the percentage completion. Use `Throbber` or an indeterminate `ProgressBar` when the load time or status is unknown.
- **Throbber Contexts**: Place a spinner button in form-submitting scenarios (e.g., "Save", "Submit") to disable the action and immediately communicate that a backend operation is in progress.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI a11y rules**: `<cr-progress>` natively injects `role="progressbar"`, `aria-valuemin`, `aria-valuemax`, and `aria-valuenow`. When `indeterminate` is set, `aria-valuenow` is appropriately removed.
- **C++ Views native focus**: `ProgressBar` is generally not focusable and relies on `MaybeNotifyAccessibilityValueChanged()` to trigger announcements for screen readers when progress percentages advance.

### 3. Icon Usage Guidelines
- The Spinner masks should strictly use `throbber_small.svg` variants or programmatically generated curves to ensure high visual fidelity instead of relying on legacy PNG/GIF loops.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    - `views::View` → `views::ProgressBar`
    - `views::View` → `views::Throbber` → `views::SmoothedThrobber`
    - `views::View` → `views::Button` → `views::LabelButton` → `views::MdTextButton` → `views::MdTextButtonWithSpinner`
*   **WebUI (Web Frontend)**:
    - `HTMLElement` → `LitElement` → `CrProgressElement` (`<cr-progress>`)
    - *Spinner*: Standard `<div>` utilizing `.spinner` CSS class rules.