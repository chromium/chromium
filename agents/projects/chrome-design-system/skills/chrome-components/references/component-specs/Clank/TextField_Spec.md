# Text Field Spec (Clank)

This document maps the specifications of the **Text field** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

**Text field** is the Material 3 text input with a floating label, optional
leading/trailing icons and supporting text. Figma models `Type` Default
(outlined) and Filled × four states × three content configurations. In code it
maps to the Material Components `TextInputLayout` with
`Widget.Material3.TextInputLayout.OutlinedBox` (used in Settings editors and
bookmark editors). The Filled type has no usage in Chromium today.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                             | Clank (Android)                                                                                                                                                                                                                          |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Text field`                                                                                                                                                | `TextInputLayout`<br>`TextInputEditText`                                                                                                                                                                                                 |
| **Source Files**   | [Figma Link: `Text field` (`66478:13661`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=66478-13661&m=dev) | [components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)<br>[chrome/android/java/res/values/styles.xml](//src/chrome/android/java/res/values/styles.xml) |

> Source page:
> [`Input & Text Fields` (`46765:112`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=46765-112)
> — documentation frame
> [`Text Fields` (`66658:44337`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=66658-44337)
> with light and dark showcases. The Figma component description links to the
> GDP / GM3 Design System text field. The same page also has the legacy
> underline [`Input Fields`](./InputFields_Spec_Clank.md) component, which has
> its own spec.
>
> `TextInputLayout` / `TextInputEditText` come from Material Components
> (`com.google.android.material:material` 1.14.0), not Chromium. How it's
> applied in Chromium:
>
> - `Widget.Chromium.Settings.TextInputLayout.OutlinedBox` (parent
>   `Widget.Material3.TextInputLayout.OutlinedBox`) is applied through
>   `ThemeOverlay.Chromium.Settings.InputFields` (`textInputStyle`) in
>   `EditorViewBase` and `AutofillLocalCardEditor`.
> - `Widget.Material3.TextInputLayout.OutlinedBox` is set directly in
>   `bookmark_popup.xml` and `bookmark_edit_desktop.xml`.
> - `Widget.MaterialComponents.TextInputLayout.OutlinedBox` (M2) is used in
>   `autofill_save_iban_bottom_sheet.xml`.
> - `Widget.Material3.TextInputLayout.OutlinedBox.ExposedDropdownMenu` is used
>   in `autofill_billing_address_dropdown.xml`.
>
> MDC tokens below were checked against MDC `tokens_textfield.xml` and
> `textfield/res/color/*` on GitHub (token v34.0.0). They weren't checked
> against the exact 1.14.0 AAR, which isn't checked in.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Text field`: `Type` (Default | Filled) × `State` (Enabled | Focused | Error |
  Disabled) × `Configuration` (Input Text | Label Text | Placeholder), plus
  booleans `Leading Icon`, `Trailing Icon`, `Supporting Text`, `Label Text`,
  `Input Text`, `Placeholder Text` (default on), and `Character Count` (default
  off).

| Feature / Variant                 | Figma Component                                   | Clank (Android)                                                                                                   |
| :-------------------------------- | :------------------------------------------------ | :---------------------------------------------------------------------------------------------------------------- |
| **Outlined**                      | `Type=Default`                                    | `@style/Widget.Material3.TextInputLayout.OutlinedBox` (or `Widget.Chromium.Settings.TextInputLayout.OutlinedBox`) |
| **Filled**                        | `Type=Filled`                                     | `@style/Widget.Material3.TextInputLayout.FilledBox` (*no usage in Chromium*)                                      |
| **Enabled**                       | `State=Enabled`                                   | Default state                                                                                                     |
| **Focused**                       | `State=Focused`                                   | `state_focused`                                                                                                   |
| **Error**                         | `State=Error`                                     | `TextInputLayout#setError(...)` (`app:errorEnabled`)                                                              |
| **Disabled**                      | `State=Disabled`                                  | `android:enabled="false"`                                                                                         |
| **Filled input + floating label** | `Configuration=Input Text`                        | Text present → hint collapsed into the outline notch / top of the container                                       |
| **Empty, label only**             | `Configuration=Label Text`                        | Empty and unfocused → hint expanded inside the field                                                              |
| **Placeholder**                   | `Configuration=Placeholder`                       | `app:placeholderText` (shown when focused/empty with the label collapsed)                                         |
| **Leading icon**                  | `Leading Icon=true` (`search`)                    | `app:startIconDrawable`                                                                                           |
| **Trailing icon**                 | `Trailing Icon=true` (`cancel`; `error` in Error) | `app:endIconMode="clear_text"` / `custom`; the error icon replaces the end icon (`errorIconDrawable`)             |
| **Supporting text**               | `Supporting Text=true`                            | `app:helperText` (error state: `setError` text)                                                                   |
| **Character count**               | `Character Count=true`                            | `app:counterEnabled` + `app:counterMaxLength`                                                                     |
| **Hovered / Pressed**             | *(Not modeled)*                                   | M3 hover outline (`colorOnSurface`) via `m3_textfield_stroke_color`                                               |
| **Dark theme**                    | "Dark" showcase (token mode switch)               | Night-mode resolution of the same theme attributes                                                                |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                       | Figma Design Token                                                                                         | Clank (Android)                                                                                                                                       |
| :------------------------------------- | :--------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Outline (enabled)**                  | `--clank/sys/color/outline` (`#747775`), `1px`                                                             | `boxStrokeColor` → `m3_textfield_stroke_color` → `?attr/colorOutline` (`baseline_neutral_variant_50` `#747775`; night `_60`), `1dp`                   |
| **Outline (focused)**                  | `--clank/sys/color/primary` (`#0b57d0`), **`3px`**                                                         | `m3_comp_outlined_text_field_focus_outline_color` → `?attr/colorPrimary`; width **`2dp`** (MDC overrides the 3dp token)                               |
| **Outline (error)**                    | `--clank/sys/color/error` (`#b3261e`); `1px`, `3px` when focused (Input Text)                              | `boxStrokeErrorColor` → `?attr/colorError` (`baseline_error_40` `#B3261E` / night `baseline_error_80`); focused width `2dp`                           |
| **Outline (disabled)**                 | `--clank/sys/state_layer/onSurface_12%`                                                                    | `m3_textfield_stroke_color` disabled → `?attr/colorOnSurface` @ **`0.38`**                                                                            |
| **Filled container**                   | `--clank/sys/color/sContainerHighest, schh` (`#dde3ea`)                                                    | `boxBackgroundColor` → `m3_textfield_filled_background_color` → `?attr/colorSurfaceContainerHighest` (`#DDE3EA` / night `#333537`)                    |
| **Filled active indicator**            | Enabled `onSurfaceVariant` `1px`; Focused `primary` `3px`; Error `error`; Disabled `onSurface_38%`         | `m3_textfield_stroke_color`: enabled **`colorOutline`**; focused `colorPrimary` `2dp`; error `colorError`; disabled `colorOnSurface` @ 38%            |
| **Label (enabled)**                    | `onSurfaceVariant` (`#444746`)                                                                             | `hintTextColor` / `textColorHint` → `m3_textfield_label_color` → `?attr/colorOnSurfaceVariant`                                                        |
| **Label (focused)**                    | `primary`                                                                                                  | `m3_textfield_label_color` focused → `?attr/colorPrimary`                                                                                             |
| **Label (error)**                      | `error`                                                                                                    | Error color applied to the collapsed hint while an error is shown                                                                                     |
| **Label / input / support (disabled)** | `onSurface_38%`                                                                                            | `colorOnSurface` @ `0.38` (label, input and helper color state lists)                                                                                 |
| **Input text**                         | `--clank/sys/color/onSurface, os` (`#1f1f1f`)                                                              | `m3_comp_outlined_text_field_input_text_color` → `?attr/colorOnSurface`                                                                               |
| **Placeholder**                        | `onSurfaceVariant`                                                                                         | `placeholderTextColor` → `m3_textfield_indicator_text_color` → `?attr/colorOnSurfaceVariant`                                                          |
| **Supporting text**                    | `--md-sys-color--on-surface-variant` (`#444746`); error `--md-sys-color--error`                            | `helperTextTextColor` → `?attr/colorOnSurfaceVariant`; `errorTextColor` → `?attr/colorError`                                                          |
| **Leading / trailing icon**            | `onSurfaceVariant`, `24px` glyph in a `48px` target                                                        | `startIconTint` / `endIconTint` → `m3_textfield_indicator_text_color` (`colorOnSurfaceVariant`)                                                       |
| **Error icon**                         | `error` glyph, `error` color                                                                               | `errorIconTint` → `?attr/colorError`                                                                                                                  |
| **Caret**                              | `primary` (focused) / `error` (error)                                                                      | `cursorColor` `@null` → `colorControlActivated` (`@macro/default_control_color_active` → `colorPrimary`); `cursorErrorColor` → `colorError`           |
| **Container height**                   | `56px`                                                                                                     | M3 `TextInputLayout` min height `56dp` *(MDC default, not verified in-tree)*                                                                          |
| **Corner radius**                      | `--clank/shape/corner_radius/4` (`4px`); Filled: top corners only                                          | `shapeAppearance` → `?attr/shapeAppearanceCornerExtraSmall` (`4dp`); Filled adds `ShapeAppearanceOverlay.Material3.Corner.Top`                        |
| **Horizontal padding**                 | `16px` (no icon) / `12px` + `4px` with icons (`--clank/shape/spacing/*`)                                   | MDC defaults (`16dp` text start, `12dp` icon start) *(not verified)*                                                                                  |
| **Supporting text spacing**            | `16px` horizontal, `4px` top                                                                               | MDC indicator area defaults *(not verified)*                                                                                                          |
| **Label typography**                   | `textSmall`: Google Sans Text 12/16, tracking 0 (outline label uses `--static/body-small`, tracking 0.1px) | `hintTextAppearance` → `?attr/textAppearanceBodySmall` (not overridden by the Chrome theme → MDC `TextAppearance.Material3.BodySmall`)                |
| **Input typography**                   | `textLarge`: Google Sans Text 16/24, tracking 0                                                            | `?attr/textAppearanceBodyLarge` (MDC default). Chromium's `Widget.BrowserUI.TextInputEditText` (`TextLarge.Primary`) only applies to the legacy style |
| **Supporting typography**              | `--static/body-small/*`: Google Sans Text 12/16, tracking 0.1px                                            | `helperTextTextAppearance` / `errorTextAppearance` → `?attr/textAppearanceBodySmall`                                                                  |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Focus stroke width (3px vs. 2dp)

- **Figma**: Focused outline (outlined) and focused active indicator (filled)
  are `3px`.
- **Clank**: MDC sets `m3_comp_outlined_text_field_focus_outline_width` to `2dp`
  ("The resource value 3dp has been overridden by the config"). Chromium doesn't
  override `boxStrokeWidthFocused`.

### 2. Disabled outline opacity

- **Figma**: Outlined disabled border is `onSurface` @ 12%.
- **Clank**: `m3_textfield_stroke_color` uses `colorOnSurface` @ 38% for
  disabled, so the code border is darker.

### 3. Filled enabled indicator color

- **Figma**: `onSurfaceVariant` (`#444746`).
- **Clank**: Filled reuses `m3_textfield_stroke_color`, so the indicator is
  `colorOutline` (`#747775`). This only matters if FilledBox is adopted.

### 4. Typography source

- Chrome's theme doesn't override `textAppearanceBodySmall` /
  `textAppearanceBodyLarge`, so the M3 field uses MDC's typescale (letter
  spacing and font family come from MDC) rather than Clank's
  `TextAppearance.TextSmall` / `TextLarge` with Google Sans Text and 0 tracking.
  *Exact MDC typescale values and font resolution weren't verified.*
- `ThemeOverlay.Chromium.Settings.TextInput` sets `hintTextAppearance` /
  `errorTextAppearance` as theme items. The M3 parent style defines both
  attributes, which take precedence over theme fallbacks, so these overrides are
  probably no-ops. *Not verified at runtime.*

### 5. Mixed Figma token families

- Supporting text and the outlined label notch use GM3 tokens
  (`--md-sys-color--*`, `--static/body-small/*`, tracking `0.1px`). Other layers
  use `--clank/*` tokens. The Figma component should use `--clank/*` tokens
  throughout.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Filled type unused in code

There's no `Widget.Material3.TextInputLayout.FilledBox` usage in Chromium. All
M3 fields are outlined.

### 2. Inconsistent styles across the app

The default app `textInputStyle` is still the legacy underline
`Widget.BrowserUI.TextInputLayout`
([`Input Fields`](./InputFields_Spec_Clank.md)). This component only applies
where features opt in (Settings editors, bookmark editors), and one surface
(IBAN save sheet) uses the M2 `MaterialComponents` outlined style.

### 3. Missing interactive states in Figma

No Hovered or Pressed variants are modeled. MDC renders a hover outline in
`colorOnSurface`.

### 4. Error focused vs. unfocused

In Figma, `State=Error` + `Configuration=Input Text` shows a 3px (focused)
outline with a cursor, while other Error configurations show 1px. The Error
state mixes focus and error rather than modeling them separately.

### 5. Not verified

- Internal paddings, icon spacing and the 56dp minimum height (MDC defaults).
- Disabled filled container color (MDC uses `colorOnSurfaceInverse` @
  `material_emphasis_disabled`; Figma's disabled filled container wasn't
  inspected in detail).

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use the outlined type for forms in Settings (addresses, cards, IBANs,
  bookmarks). Apply it through `ThemeOverlay.Chromium.Settings.InputFields`
  rather than per-view styles so every field on a screen matches.
- Don't mix outlined, filled and legacy underline fields on the same screen.
- Use supporting text for format hints and replace it with an error message on
  validation failure. Keep labels short and in sentence case.
- Only use a placeholder together with a label. A placeholder isn't a substitute
  for a label.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Put the label in `TextInputLayout`'s hint so TalkBack reads label, value, and
  helper or error text together.
- Use `setError(...)` (not a separate red `TextView`) so errors are announced.
  Enable `errorEnabled` up front to avoid layout jumps.
- Trailing icons (clear, password toggle) must have content descriptions. The
  `endIconMode` built-ins provide them. Icon targets are `48dp`.
- Set the right `inputType` and `autofillHints`.

### 3. Icon Usage Guidelines

- Leading icon: an optional 24dp glyph that describes the field (for example
  `search`).
- Trailing icon: an action (`clear_text`, `password_toggle`, `dropdown_menu`).
  In the error state, MDC's error icon replaces it.
