# Component Spec: Text Fields

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Text Fields** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Text Fields** component is a single-line string input box designed to
capture user inputs in native desktop interfaces (C++ Views), web frontends
(WebUI), and mobile interfaces (Clank Android). It supports rounded containers,
focus ring outlines or animated bottom underline highlights, helper labels,
slot-based inline leading/trailing icons, disabled container styles, and
validation error treatments.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                  | Clank (Android)                                                                                                                                  |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Text Fields (Views)` / `Text Fields (WebUI)`                                                                                                                                                                                                                                                  | `views::Textfield`                                                                       | `<cr-input>`                                                                                                     | `EditTextWithLeading` / `TextInputLayout`                                                                                                        |
| **Source Files**   | [Figma Link (Views): `30230:10704`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=30230-10704)<br>[Figma Link (WebUI): `280:26417`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26417) | [ui/views/controls/textfield/textfield.h](//src/ui/views/controls/textfield/textfield.h) | [ui/webui/resources/cr_elements/cr_input/cr_input.ts](//src/ui/webui/resources/cr_elements/cr_input/cr_input.ts) | [ui/android/java/src/org/chromium/ui/widget/EditTextWithLeading.java](//src/ui/android/java/src/org/chromium/ui/widget/EditTextWithLeading.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant             | Figma Component                     | C++ Views (Desktop)                                     | WebUI (Desktop)                       | Clank (Android)                                                  |
| :---------------------------- | :---------------------------------- | :------------------------------------------------------ | :------------------------------------ | :--------------------------------------------------------------- |
| **Standard / Outlined Input** | Standard empty text field container | Native `views::Textfield` with `FocusableBorder`        | `<cr-input class="stroked">`          | `@style/Widget.BrowserUI.TextInputLayout` (Outlined)             |
| **Filled (Underlined) Input** | Filled/underlined text field        | N/A *(Standard Views is outlined)*                      | Standard `<cr-input>` default styling | `com.google.android.material.textfield.TextInputLayout` (Filled) |
| **Inline Prefix Icon**        | LHS Icon slot                       | Composed as separate icon view or `Textfield` icon slot | `<div slot="inline-prefix">`          | `app:startIconDrawable` / `EditTextWithLeading`                  |
| **Inline Suffix Icon**        | RHS Icon slot                       | Composed inside container or trailing button            | `<div slot="inline-suffix">`          | `app:endIconMode` / `EditTextWithLeading`                        |

______________________________________________________________________

## 3. Component States

| State                  | Figma Component     | C++ Views (Desktop)                                | WebUI (Desktop)                                                                | Clank (Android)                                             |
| :--------------------- | :------------------ | :------------------------------------------------- | :----------------------------------------------------------------------------- | :---------------------------------------------------------- |
| **Default (Normal)**   | `State=Default`     | `ui::kColorTextfieldOutline` border                | Idle state, background `--cr-input-background-color` with 1px border underline | Outline / fill with default hairline stroke                 |
| **Hovered**            | *(Inferred/Common)* | Shows `ui::kColorTextfieldHover` ink drop overlay  | `#hover-layer` overlay styled via `--cr-input-hover-background-color`          | State layer overlay (`@dimen/default_hovered_alpha`)        |
| **Selected (Focused)** | `State=Selected`    | Triggers `views::FocusRing` drawing                | Triggers `[focused_]` transition (2px solid highlight underline)               | 2dp active stroke (`?attr/colorPrimary`)                    |
| **Error (Invalid)**    | `State=Error`       | `views::FocusRing::Get(this)->SetInvalid(true)`    | Attribute: `<cr-input invalid>` (colors label, underline, and caret red)       | `app:errorEnabled="true"` with error tint                   |
| **Disabled**           | `State=Disabled`    | `SetEnabled(false)` (disabled background & border) | Attribute: `<cr-input disabled>` (`[disabled]` reflected)                      | `android:enabled="false"` (`@dimen/default_disabled_alpha`) |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                         | C++ Views (Desktop)                                   | WebUI (Desktop)                            | Clank (Android)                                              |
| :------------------------------- | :--------------------------------------------------------- | :---------------------------------------------------- | :----------------------------------------- | :----------------------------------------------------------- |
| **Default Outline / Underline**  | `--desktop/sys/outline-colors/neutral-outline` (`#c7c7c7`) | `ui::kColorTextfieldOutline`                          | `--cr-input-border-bottom` (`1px solid`)   | `@macro/hairline_stroke_color` / `?attr/colorOutline`        |
| **Focus Ring / Underline Color** | `--desktop/sys/state-colors/state-focus-ring` (`#0b57d0`)  | `ui::kColorFocusableBorderFocused`                    | `--cr-input-focus-color` (`2px solid`)     | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| **Error Color**                  | `--desktop/sys/error-colors/error` (`#b3261e`)             | `ui::kColorTextfieldOutlineInvalid`                   | `--cr-input-error-color`                   | `?attr/colorError`                                           |
| **Container Background**         | `--desktop/sys/surface-colors/surface-variant` (`#e1e3e1`) | `ui::kColorTextfieldBackground`                       | `--cr-input-background-color`              | `?attr/colorSurfaceContainer`                                |
| **Disabled Background**          | `--desktop/sys/state-colors/state-disabled-container`      | `ui::kColorTextfieldBackgroundDisabled`               | `--color-textfield-background-disabled`    | `@dimen/default_disabled_alpha` overlay                      |
| **Base On-Surface Text**         | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)      | `ui::kColorTextfieldForeground`                       | `--cr-input-color`                         | `@macro/default_text_color` / `?attr/colorOnSurface`         |
| **Disabled Text Color**          | `--desktop/sys/state-colors/state-disabled`                | `ui::kColorTextfieldForegroundDisabled`               | `--color-textfield-foreground-disabled`    | `@macro/default_text_color_secondary`                        |
| **Label Color (Normal)**         | `--desktop/sys/surface-colors/on-surface-subtle`           | N/A *(Separate Label view)*                           | `--cr-input-label-color`                   | `@macro/default_text_color_secondary`                        |
| **Corner Radius**                | `--desktop/corner-radius/8` (`8px`)                        | `ShapeSysTokens::kSmall` (`8px`)                      | `--cr-input-border-radius` (`8px 8px 0 0`) | `@dimen/default_rounded_corner_radius` (`8dp`)               |
| **Inner Padding (Horizontal)**   | `--desktop/spacing/10` (`10px`)                            | `DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING` (`10px`) | `--cr-input-padding-start`/`end` (`10px`)  | `12dp` - `16dp`                                              |
| **Inner Padding (Vertical)**     | `--desktop/spacing/9` (`9px`)                              | `DISTANCE_CONTROL_VERTICAL_TEXT_PADDING` (`10px`)     | `--cr-input-padding-top`/`bottom` (`10px`) | `12dp` - `16dp`                                              |
| **Font Size (Value)**            | `--desktop/font_size/body-four` (`12px`)                   | `ui::kLabelFontSizeDelta` (`12px`)                    | `--cr-input-font-size` (`12px`)            | `@style/TextAppearance.TextMedium.Primary` (`14sp`-`16sp`)   |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Outlined Views vs. Underlined WebUI & Clank Material

- **C++ Views**: Standard text fields render as an all-around outlined box
  (`views::FocusableBorder`).
- **WebUI**: Standard `<cr-input>` renders as a filled container with a flat
  bottom underline (`border-bottom`) that animates on focus, though
  `<cr-input class="stroked">` supports outlined boxes.
- **Clank (Android)**: Material Design Components (`TextInputLayout`) support
  both outlined boxes (`@style/Widget.BrowserUI.TextInputLayout`) and filled
  containers.

### 2. Hardcoded Padding vs. LayoutProvider

In Views, padding is resolved via `LayoutProvider`. On WebUI, padding is
tokenized via CSS variables. On Android, `TextInputLayout` /
`EditTextWithLeading` resolves padding via theme dimension attributes
(`@dimen/*`).

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Floating Labels vs Fixed Labels

- **WebUI**: Supports static label blocks positioned above or within input rows
  (`label` attribute).
- **Clank (Android)**: `TextInputLayout` supports animated floating hint labels
  on text input focus.
- **Views**: Requires a companion `views::Label` or accessible name.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Label Visibility**: Always set the `label` attribute on `<cr-input>` or
  `android:hint` on Android to describe expected input.
- **Validation**: Use `auto-validate` or Android `TextWatcher` to trigger error
  feedback automatically.
- **Accessible Naming**: Views text fields should have an accessible name
  assigned using `GetViewAccessibility().SetName()` or an associated
  `views::Label`.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: Automatically links labels and inputs via `aria-labelledby`.
- **C++ Views**: Focus ring outsets disabled by default
  (`SetOutsetFocusRingDisabled(true)`).
- **Clank (Android)**: Ensure minimum touch target height meets `48dp`
  (`@dimen/min_touch_target_size`).
