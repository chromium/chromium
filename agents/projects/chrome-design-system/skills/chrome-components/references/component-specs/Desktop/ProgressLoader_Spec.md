# Progress Loader Spec (Desktop)

This document maps the specifications of the **Progress Loader** (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

The **Progress Loader** communicates that work is in progress on Chrome Desktop.
It has three variants: a `200px × 4px` linear **Progress Indicator** bar, a
`28px` circular **Spinner** (throbber), and a **Spinner Button**, which is a
filled primary pill button showing a `16px` spinner in place of its label. None
of them are interactive except the Spinner Button. C++ Views implements these as
`views::ProgressBar`, `views::Throbber` / `views::SmoothedThrobber` and
`views::MdTextButtonWithSpinner`, and WebUI uses `<cr-progress>` and the shared
`.spinner` CSS class.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | C++ Views (Desktop)                                                                                                                                                                                                                                                        | WebUI (Desktop)                                                                                                                                                                                                                                          |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Progress Loader` (3 variants: `Progress Indicator`, `Spinner`, `Spinner Button`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | `views::ProgressBar`<br>`views::Throbber` / `views::SmoothedThrobber`<br>`views::MdTextButtonWithSpinner`                                                                                                                                                                  | `<cr-progress>`<br>`.spinner` (shared style module, not a custom element)                                                                                                                                                                                |
| **Source Files**   | [Figma Link: `Progress Loader` (`6191:3526`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=6191-3526)<br>[`Variant=Progress Indicator` (`6191:3525`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=6191-3525)<br>[`Variant=Spinner` (`6191:3521`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=6191-3521)<br>[`Variant=Spinner Button` (`14060:1473`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=14060-1473) | [ui/views/controls/progress_bar.h](//src/ui/views/controls/progress_bar.h)<br>[ui/views/controls/throbber.h](//src/ui/views/controls/throbber.h)<br>[ui/views/controls/button/md_text_button_with_spinner.h](//src/ui/views/controls/button/md_text_button_with_spinner.h) | [ui/webui/resources/cr_elements/cr_progress/cr_progress.ts](//src/ui/webui/resources/cr_elements/cr_progress/cr_progress.ts)<br>[ui/webui/resources/cr_elements/cr_spinner_style_lit.css](//src/ui/webui/resources/cr_elements/cr_spinner_style_lit.css) |

> [!NOTE] The Figma node `6191:3526` is reported by the Figma API as a `FRAME`
> that holds three `Variant=…` component symbols. It is the `Progress Loader`
> variant set in practice. The Spinner Button symbol also has hidden `Label` and
> `Trailing Icon` layers.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Progress Loader`: `Variant` (Progress Indicator | Spinner | Spinner Button).
  There are no `State` or `Size` axes, and no determinate/indeterminate axis.

| Feature / Variant                       | Figma Component                                        | C++ Views (Desktop)                                                                                           | WebUI (Desktop)                                                                                          |
| :-------------------------------------- | :----------------------------------------------------- | :------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------- |
| **Linear progress (determinate)**       | `Variant=Progress Indicator` (shown at 70% fill)       | `views::ProgressBar::SetValue(0.0–1.0)`                                                                       | `<cr-progress value="70">` (`min`/`max`/`step`, defaults 0/100/1)                                        |
| **Linear progress (indeterminate)**     | *(Not modeled in Figma)*                               | `ProgressBar::SetValue(<0 or >1)` runs the indeterminate animation                                            | `<cr-progress indeterminate>`                                                                            |
| **Linear progress (paused / disabled)** | *(Not modeled in Figma)*                               | `ProgressBar::SetPaused(true)` → `ui::kColorProgressBarPaused`                                                | `<cr-progress disabled>` → `--cr-progress-disabled-active-color` (default `--google-grey-500`)           |
| **Circular spinner**                    | `Variant=Spinner` (`28px`)                             | `views::Throbber(28)` + `Start()` / `Stop()`. Use `views::SmoothedThrobber` for start/stop delays.            | `<div class="spinner">` (with `cr-spinner-style-lit`), size from `--cr-spinner-size: 28px`               |
| **Spinner checkmark (done)**            | *(Not modeled in Figma)*                               | `Throbber::SetChecked(true)`                                                                                  | —                                                                                                        |
| **Spinner button**                      | `Variant=Spinner Button` (spinner only, label hidden)  | `views::MdTextButtonWithSpinner` + `SetStyle(ui::ButtonStyle::kProminent)` + `SetSpinnerVisible(true)`        | *No equivalent.* Callers have to put a `.spinner` inside `<cr-button class="action-button">` themselves. |
| **Spinner button with label**           | Hidden `Label` layer (`x=41`)                          | Supported: label stays visible next to the spinner, with `kSpinnerLabelSpacing = 8`                           | —                                                                                                        |
| **Spinner button states**               | *(Not modeled. Only an empty `States` overlay frame.)* | Hover, pressed, focus and disabled come from `MdTextButton`. The spinner switches to the disabled text color. | —                                                                                                        |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

### Progress Indicator

| Design Attribute            | Figma Design Token                                 | C++ Views (Desktop)                                                      | WebUI (Desktop)                                                   |
| :-------------------------- | :------------------------------------------------- | :----------------------------------------------------------------------- | :---------------------------------------------------------------- |
| **Active (fill) color**     | `--desktop/sys/primary-colors/primary` (`#0b57d0`) | `ui::kColorProgressBar` → `ui::kColorSysPrimary` ✅                      | `--cr-progress-active-color`, default **`--google-green-500`** ❌ |
| **Track (container) color** | *None (transparent; no track layer)*               | `ui::kColorProgressBarBackground` → `ui::kColorSysNeutralOutline` ❌     | `--cr-progress-container-color`, default `--google-grey-300` ❌   |
| **Height**                  | `4px`                                              | `preferred_height_ = 5` ❌ (`SetPreferredHeight(4)` needed)              | `--cr-progress-height`, default `4px` ✅                          |
| **Width**                   | `200px` (example)                                  | Fills layout width (preferred width `1`)                                 | `:host { width: 200px; }` ✅                                      |
| **Corner radius**           | `0` (square ends)                                  | `preferred_corner_radii_ = RoundedCornersF(3)` ❌ (capped to bar height) | `0` ✅                                                            |
| **Paused color**            | *(Not modeled)*                                    | `ui::kColorProgressBarPaused` → `ui::kColorSysStateDisabled`             | `--google-grey-500` (no dark-mode variant)                        |

### Spinner

| Design Attribute | Figma Design Token                                                                                                      | C++ Views (Desktop)                                                             | WebUI (Desktop)                                                                                           |
| :--------------- | :---------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------ | :-------------------------------------------------------------------------------------------------------- |
| **Color**        | `#0B57D0` fill (looks like `--desktop/sys/primary-colors/primary`, but the token binding on the vector isn't confirmed) | `ui::kColorThrobber` → `ui::kColorSysPrimary` ✅. Override with `SetColorId()`. | `--cr-spinner-color`, default **`--google-blue-500`** (`#4285f4`), or `--google-blue-300` in dark mode ❌ |
| **Diameter**     | `28px`                                                                                                                  | `Throbber(int diameter)`. Default **`16`**, so callers must pass `28`.          | `--cr-spinner-size: 28px` ✅                                                                              |
| **Stroke width** | `3px`                                                                                                                   | `(28 + 8) / 12 = 3px` (auto-computed in `gfx::PaintThrobberSpinning`) ✅        | The mask is `throbber_small.svg` (16px viewBox, `stroke-width: 2.25`) scaled to 28px ≈ **`3.94px`** ❌    |
| **Arc shape**    | Static 270° arc, flat (butt) ends                                                                                       | Animated sweep up to 270° (`kMaxArcSize`), **round caps**                       | Animated 270° dash, **round caps**                                                                        |
| **Track**        | None                                                                                                                    | None ✅                                                                         | None ✅                                                                                                   |

### Spinner Button

| Design Attribute         | Figma Design Token                                    | C++ Views (Desktop)                                                                                                                    | WebUI (Desktop)                                                        |
| :----------------------- | :---------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------- |
| **Container background** | `--desktop/sys/primary-colors/primary` (`#0b57d0`)    | `ui::kColorButtonBackgroundProminent` → `ui::kColorSysPrimary` ✅                                                                      | — (`.action-button` → `--cr-button-background-color` if built by hand) |
| **Spinner color**        | `--desktop/sys/primary-colors/on-primary` (`#ffffff`) | `TypographyProvider::GetColorId(…, STYLE_DIALOG_BUTTON_DEFAULT)` → `ui::kColorButtonForegroundProminent` → `ui::kColorSysOnPrimary` ✅ | —                                                                      |
| **Spinner diameter**     | `16px`                                                | `kSpinnerDiameter = 20` ❌                                                                                                             | —                                                                      |
| **Spinner stroke width** | ≈ `2px`                                               | `3 − (28 − 20) / 16 = 2.5px` ❌                                                                                                        | —                                                                      |
| **Spinner–label gap**    | `--desktop/spacing/8` (`8px`)                         | `kSpinnerLabelSpacing = 8` ✅                                                                                                          | —                                                                      |
| **Horizontal padding**   | `--desktop/spacing/32` (`32px`)                       | `MdTextButton` default `DISTANCE_BUTTON_HORIZONTAL_PADDING` (`16px`) ❌ unless `SetCustomPadding()` is used                            | —                                                                      |
| **Vertical padding**     | `--desktop/spacing/10` (`10px`)                       | `(control height − label height) / 2` (≈ `8px` with a label)                                                                           | —                                                                      |
| **Height**               | `36px`                                                | `LayoutProvider::GetControlHeightForFont()` (`36px` for the default button font) ✅                                                    | —                                                                      |
| **Corner radius**        | `--desktop/corner-radius/fully-rounded` (`999px`)     | `MdTextButton` pill radius (`ShapeContextTokens::kButtonRadius`) ✅ (not re-verified)                                                  | —                                                                      |
| **Width**                | `80px` (16 + 32 × 2)                                  | Label width + `20` + `8` + insets. The label stays visible, and the width keeps space for an empty label (see Gaps).                   | —                                                                      |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

1. **No WebUI Spinner Button.** `<cr-button>` has no spinner or loading slot or
   attribute. Every WebUI surface builds its own (`.spinner` +
   `--cr-spinner-color`). That makes on-primary coloring, the 16px size and the
   8px gap inconsistent across surfaces.
2. **WebUI spinner is a mask over a fixed 16px SVG.** `.spinner` scales
   `//resources/images/throbber_small.svg` (stroke `2.25` at 16px). Because the
   stroke scales with the size, a 28px spinner draws a ≈3.9px stroke instead of
   Figma's 3px. Views computes the stroke from the diameter, so its 28px spinner
   matches Figma.
3. **WebUI spinner color is not a semantic token.** It falls back to
   `--google-blue-500` / `--google-blue-300` (legacy Google palette), not
   `--color-sys-primary` / `--cr-fallback-color-primary`. In the GM3 light
   theme, `#4285f4` is visibly lighter than `#0b57d0`.
4. **`<cr-progress>` defaults predate GM3.** It defaults to green active
   (`--google-green-500`) and grey track (`--google-grey-300`) colors, with no
   dark-mode handling. Consumers (downloads, PDF, glic, webui_gallery) all
   override `--cr-progress-active-color` by hand, each with a different value.
5. **Views `ProgressBar` defaults differ from Figma.** It defaults to a height
   of `5px`, a corner radius of `3px` and a `kColorSysNeutralOutline` track.
   Figma shows `4px`, square ends and no track.
6. **Figma covers less than the code.** Figma doesn't model indeterminate,
   paused/disabled, the spinner checkmark state, Spinner Button
   hover/pressed/focus/disabled states, or spinner sizes other than 28px and
   16px.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **Spinner Button size**: Views uses a `20px` spinner (≈2.5px stroke). Figma
  uses `16px` (≈2px stroke).
- **Spinner Button padding**: Figma uses `32px` horizontal / `10px` vertical
  padding for the spinner-only button, so its width (`80px`) matches the
  width-preserving pattern. Views keeps the default `MdTextButton` insets
  (`16px`) and does not hide the label when the spinner is shown.
- **Spinner-only button centering (possible bug)**: `CalculateProposedLayout()`
  only drops the 8px gap when the label's laid-out width is `0`. But
  `CalculatePreferredSize()` decides from `GetText().empty()`. If the text is
  non-empty but the label gets squeezed to zero width, preferred size and layout
  disagree. This is minor and needs verification.
- **`MdTextButtonWithSpinner` destructor (possible bug)**: the destructor calls
  `SetSpinnerVisible(false)`, which calls `PreferredSizeChanged()` on a view
  that is being destroyed. This is probably harmless but unusual.
- **Arc caps**: Figma draws flat-ended arcs. Both code paths use round caps.
- **Spinner checkmark**: `Throbber::SetChecked(true)` paints a checkmark that
  has no Figma counterpart.
- **`<cr-progress>` disabled color** is hardcoded `--google-grey-500`, with no
  dark-mode or semantic token.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use the **Progress Indicator** when progress can be measured (downloads, file
  uploads). Use the indeterminate mode only when you can't estimate the
  duration.
- Use the **Spinner** for short, indeterminate waits inside a surface (loading
  list content, fetching results). In Views, prefer `views::SmoothedThrobber` so
  very short operations don't flash a spinner.
- Use the **Spinner Button** when a primary action starts an async operation.
  Keep the button's width the same so the layout doesn't jump. Disable repeat
  activation while the spinner is showing.
- In WebUI, set `--cr-progress-active-color` (and the container color) and
  `--cr-spinner-color` to semantic tokens (for example `--color-sys-primary` /
  `--cr-fallback-color-primary`) until the defaults are updated.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: `<cr-progress>` sets `role="progressbar"`, `aria-valuemin`,
  `aria-valuemax` and `aria-valuenow` (removed when `indeterminate`), plus
  `aria-disabled`. Give it an accessible name with `aria-label` or
  `aria-labelledby`. `.spinner` is a plain `<div>` with no role, so add
  `role="progressbar"` or `aria-busy` on the region and a label, or announce the
  loading state with `<cr-a11y-announcer>`.
- **C++ Views**: `views::ProgressBar` exposes progress values and fires
  `kValueChanged` accessibility events when the percentage changes.
  `views::Throbber` is purely visual, so announce state changes separately. For
  `MdTextButtonWithSpinner`, keep an accessible name even when the visible label
  is hidden.
- Spinners and indeterminate bars are animated. Make sure nothing depends only
  on the animation to convey meaning.

### 3. Icon Usage Guidelines

- The Spinner Button in Figma includes a hidden `Trailing Icon` (20px) slot.
  Don't show a trailing icon while the spinner is visible.
  `MdTextButtonWithSpinner` doesn't lay out a spinner and an image together.
