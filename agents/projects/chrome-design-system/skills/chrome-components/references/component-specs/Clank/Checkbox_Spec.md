# Checkbox Spec (Clank)

This document maps the specifications of the **Checkbox** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Checkbox** marks a selection for a specific item or question, and more
than one can be selected at once (per the Figma description). Figma models only
the 24px glyph, with `Type` On / Off × Enabled / Disabled. In code, Chrome uses
the Material 3 `CheckBox` (`MaterialCheckBox`), styled app-wide by
`Widget.BrowserUI.CheckBox`, and usually wraps it in the Chromium row widget
`CheckBoxWithDescription`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                         | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Checkbox`                                                                                                                                              | `CheckBox` (`MaterialCheckBox`)<br>`CheckBoxWithDescription`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Source Files**   | [Figma Link: `Checkbox` (`7068:51761`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7068-51761&m=dev) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/CheckBoxWithDescription.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/CheckBoxWithDescription.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)<br>[components/browser_ui/styles/android/java/res/color/selection_control_button_tint_list.xml](//src/components/browser_ui/styles/android/java/res/color/selection_control_button_tint_list.xml) |

> Source page:
> [`Components/Selection-controls` (`43256:439`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=43256-439)
> — documentation canvas with light and dark showcases.
>
> `CheckBox` / `MaterialCheckBox` come from the Android framework and Material
> Components (`com.google.android.material:material` 1.14.0), not Chromium. The
> app theme
> ([themes.xml](//src/components/browser_ui/theme/android/templates/res/values/themes.xml))
> sets `checkboxStyle` → `@style/Widget.BrowserUI.CheckBox` (parent
> `Widget.Material3.CompoundButton.CheckBox`).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Checkbox`: `Type` (On | On Disabled | Off | Off Disabled).

| Feature / Variant               | Figma Component                     | Clank (Android)                                                               |
| :------------------------------ | :---------------------------------- | :---------------------------------------------------------------------------- |
| **Checked**                     | `Type=On`                           | `android:checked="true"` / `setChecked(true)`                                 |
| **Unchecked**                   | `Type=Off`                          | `android:checked="false"`                                                     |
| **Checked, disabled**           | `Type=On Disabled`                  | `android:enabled="false"` + checked                                           |
| **Unchecked, disabled**         | `Type=Off Disabled`                 | `android:enabled="false"` + unchecked                                         |
| **With label + description**    | *(Not modeled — glyph only)*        | `CheckBoxWithDescription` (`setPrimaryText`, `setDescriptionText`)            |
| **Indeterminate**               | *(Not modeled)*                     | `MaterialCheckBox#setCheckedState(MaterialCheckBox.STATE_INDETERMINATE)`      |
| **Error**                       | *(Not modeled)*                     | `MaterialCheckBox#setErrorShown(true)`                                        |
| **Hovered / Focused / Pressed** | *(Not modeled)*                     | M3 ripple state layer around the button drawable                              |
| **Dark theme**                  | "Dark" showcase (token mode switch) | Night-mode resolution of `?attr/colorPrimary` / `?attr/colorOnSurfaceVariant` |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute            | Figma Design Token                                                                                     | Clank (Android)                                                                                                                                          |
| :-------------------------- | :----------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Checked container**       | `--clank/sys/color/primary`<br>(light `#0b57d0` / dark `#a8c7fa`)                                      | `buttonTint` → `@color/selection_control_button_tint_list` (`state_checked`) → `@macro/default_control_color_active` → `?attr/colorPrimary`              |
| **Unchecked outline**       | `--clank/sys/color/onSurface, os`<br>(light `#1f1f1f` / dark `#e3e3e3`)                                | `@macro/default_control_color_normal` → `?attr/colorOnSurfaceVariant` (baseline light `#444746` / dark `#c4c7c5`)                                        |
| **Disabled (on & off)**     | `--clank/sys/state_layer/onSurface_38%`<br>(light `rgba(31,31,31,.38)` / dark `rgba(227,227,227,.38)`) | `@macro/default_control_color_normal` @ `@dimen/default_disabled_alpha` (`0.38`), i.e. `colorOnSurfaceVariant` @ 38%                                     |
| **Checkmark**               | Knocked out of the filled `check_box` glyph (shows the surface behind)                                 | M3 `buttonIconTint` default `m3_checkbox_button_icon_tint`: `?attr/colorOnPrimary`; disabled `?attr/colorSurface` *(verified, MDC 1.14.0)*               |
| **Glyph / drawable**        | Google Symbols `check_box` (FILL 1) / `check_box_outline_blank` (FILL 0)                               | M3 animated vector button drawable (`Widget.Material3.CompoundButton.CheckBox`)                                                                          |
| **Glyph size**              | `24px`                                                                                                 | `24dp` drawable (18dp box inside)                                                                                                                        |
| **Min size / touch target** | `24px` (not modeled)                                                                                   | `android:minWidth` / `minHeight` = `0dp` (overrides M3's 48dp); `CheckBoxWithDescription` row `minHeight="@dimen/min_touch_target_size"` (`48dp`)        |
| **Label typography**        | *(Not modeled)*                                                                                        | `@style/TextAppearance.TextMedium.Primary` (widget); `TextLarge.Secondary` primary text + `TextSmall.Secondary` description in `CheckBoxWithDescription` |
| **Label spacing**           | *(Not modeled)*                                                                                        | `@style/DefaultCheckboxStyle` → `paddingStart` `16dp`                                                                                                    |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Unchecked and disabled color (`onSurface` vs. `onSurfaceVariant`)

- **Figma**: Off uses `onSurface` (`#1f1f1f`); disabled uses `onSurface` @ 38%.
- **Clank**: Chromium overrides `buttonTint` with
  `selection_control_button_tint_list`, which resolves unchecked and disabled to
  `colorOnSurfaceVariant` (`#444746`; disabled at 38%). Note that stock M3 also
  uses `colorOnSurfaceVariant` for the unchecked outline, so code follows M3 and
  Figma differs from both.

### 2. Glyph-only Figma component

- Labels, descriptions, row padding and touch targets live in
  `CheckBoxWithDescription` / `DefaultCheckboxStyle`, which have no Figma
  counterpart on this page.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing interactive states in Figma

There are no Hovered / Focused / Pressed variants, but code renders M3 ripple
state layers and keyboard focus feedback.

### 2. Code-only states

Indeterminate and error states (M3) aren't modeled in Figma.

### 3. Glyph vs. drawable

Figma uses Google Symbols font glyphs; code uses M3 animated vector drawables
(with a check/uncheck animation). Stroke and corner proportions may differ
slightly.

### 4. Touch target

Because `Widget.BrowserUI.CheckBox` sets `minWidth` / `minHeight` to `0dp`, a
standalone checkbox can be smaller than `48dp`; the parent row has to provide
the touch target.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use a checkbox for independent, multi-select options or for
  consent/confirmation. Use a `Switch` for settings that take effect
  immediately, and a radio button for mutually exclusive choices.
- Prefer `CheckBoxWithDescription` over a raw `CheckBox` so label typography and
  touch target are consistent.
- **Labeling**: Sentence case; describe the option or the state being selected.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: The whole row should be clickable and at least `48dp` tall
  (`@dimen/min_touch_target_size`).
- **TalkBack**: Make sure the label is associated with the checkbox so it
  announces label + checked state together. Only set `contentDescription` when
  there's no visible label.
- **Keyboard / D-pad**: Focus feedback comes from the M3 ripple state layer.

### 3. Icon Usage Guidelines

- Don't replace the checkbox glyph with custom icons; use the themed M3
  drawable.
