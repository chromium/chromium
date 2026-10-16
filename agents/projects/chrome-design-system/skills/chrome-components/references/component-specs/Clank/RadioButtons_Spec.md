# RadioButtons Spec (Clank)

This document maps the specifications of the **RadioButtons** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

A **Radio button** marks a selection where only one item in a set can be
selected (per the Figma description). Figma models only the 24px glyph, with
`Type` On / Off × Enabled / Disabled. In code, Chrome uses the Material 3
`RadioButton` (`MaterialRadioButton`), styled app-wide by
`Widget.BrowserUI.RadioButton`, and usually wraps it in
`RadioButtonWithDescription` rows grouped by `RadioButtonWithDescriptionLayout`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                             | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Radio-button`                                                                                                                                              | `RadioButton` (`MaterialRadioButton`)<br>`RadioButtonWithDescription`<br>`RadioButtonWithDescriptionLayout`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Source Files**   | [Figma Link: `Radio-button` (`7068:51814`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7068-51814&m=dev) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescription.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescription.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescriptionLayout.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescriptionLayout.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)<br>[components/browser_ui/styles/android/java/res/color/selection_control_button_tint_list.xml](//src/components/browser_ui/styles/android/java/res/color/selection_control_button_tint_list.xml) |

> Source page:
> [`Components/Selection-controls` (`43256:439`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=43256-439)
> — documentation canvas with light and dark showcases.
>
> `RadioButton` / `MaterialRadioButton` come from the Android framework and
> Material Components (1.14.0). The app theme
> ([themes.xml](//src/components/browser_ui/theme/android/templates/res/values/themes.xml))
> sets `radioButtonStyle` → `@style/Widget.BrowserUI.RadioButton` (parent
> `Widget.Material3.CompoundButton.RadioButton`). Related Chromium variants:
> `RadioButtonWithEditText`, `RadioButtonWithDescriptionAndAuxButton`,
> `RichRadioButton`.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Radio-button`: `Type` (On | On Disabled | Off | Off Disabled).

| Feature / Variant               | Figma Component                     | Clank (Android)                                                               |
| :------------------------------ | :---------------------------------- | :---------------------------------------------------------------------------- |
| **Selected**                    | `Type=On`                           | `android:checked="true"` / `setChecked(true)`                                 |
| **Unselected**                  | `Type=Off`                          | `android:checked="false"`                                                     |
| **Selected, disabled**          | `Type=On Disabled`                  | `android:enabled="false"` + checked                                           |
| **Unselected, disabled**        | `Type=Off Disabled`                 | `android:enabled="false"` + unchecked                                         |
| **With label + description**    | *(Not modeled — glyph only)*        | `RadioButtonWithDescription` (`setPrimaryText`, `setDescriptionText`)         |
| **Group (mutual exclusion)**    | *(Not modeled)*                     | `RadioButtonWithDescriptionLayout` (or framework `RadioGroup`)                |
| **With edit text / aux button** | *(Not modeled)*                     | `RadioButtonWithEditText`, `RadioButtonWithDescriptionAndAuxButton`           |
| **Hovered / Focused / Pressed** | *(Not modeled)*                     | M3 ripple state layer (`m3_radiobutton_ripple_tint`)                          |
| **Dark theme**                  | "Dark" showcase (token mode switch) | Night-mode resolution of `?attr/colorPrimary` / `?attr/colorOnSurfaceVariant` |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute            | Figma Design Token                                                                                     | Clank (Android)                                                                                                                                       |
| :-------------------------- | :----------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Selected color**          | `--clank/sys/color/primary`<br>(light `#0b57d0` / dark `#a8c7fa`)                                      | `buttonTint` → `@color/selection_control_button_tint_list` (`state_checked`) → `@macro/default_control_color_active` → `?attr/colorPrimary`           |
| **Unselected color**        | `--clank/sys/color/onSurface, os`<br>(light `#1f1f1f` / dark `#e3e3e3`)                                | `@macro/default_control_color_normal` → `?attr/colorOnSurfaceVariant` (baseline light `#444746` / dark `#c4c7c5`)                                     |
| **Disabled (on & off)**     | `--clank/sys/state_layer/onSurface_38%`<br>(light `rgba(31,31,31,.38)` / dark `rgba(227,227,227,.38)`) | `@macro/default_control_color_normal` @ `@dimen/default_disabled_alpha` (`0.38`), i.e. `colorOnSurfaceVariant` @ 38%                                  |
| **State layer (reference)** | *(Not modeled)*                                                                                        | M3 tokens (MDC 1.14.0): hover/focus `colorPrimary` (selected) / `colorOnSurface` (unselected) at the M3 system state opacities                        |
| **Glyph / drawable**        | Google Symbols `radio_button_checked` / `radio_button_unchecked` (FILL 0)                              | M3 animated button drawable (`Widget.Material3.CompoundButton.RadioButton`)                                                                           |
| **Glyph size**              | `24px`                                                                                                 | `24dp` drawable (20dp ring inside)                                                                                                                    |
| **Min size / touch target** | `24px` (not modeled)                                                                                   | `android:minWidth` / `minHeight` = `0dp` (overrides M3's 48dp); the row widget provides the touch target                                              |
| **Label typography**        | *(Not modeled)*                                                                                        | `@style/TextAppearance.TextMedium.Primary` (widget); `TextLarge.Primary` primary + `TextMedium.Secondary` description in `RadioButtonWithDescription` |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Unselected and disabled color (`onSurface` vs. `onSurfaceVariant`)

- **Figma**: Off uses `onSurface` (`#1f1f1f`); disabled uses `onSurface` @ 38%.
- **Clank**: `selection_control_button_tint_list` uses `colorOnSurfaceVariant`
  for unselected (matching the M3 token
  `m3_comp_radio_button_unselected_icon_color`) and `colorOnSurfaceVariant` @
  38% for disabled. M3's own disabled token is `colorOnSurface` @ 38%, so Figma
  matches M3 on disabled but Chromium's override doesn't.

### 2. Glyph-only Figma component

- Grouping, labels, descriptions and the rich variants exist only in Chromium
  widgets and have no Figma counterpart on this page.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing interactive states in Figma

There are no Hovered / Focused / Pressed variants; code renders the M3 ripple
(`m3_radiobutton_ripple_tint`).

### 2. Glyph vs. drawable

Figma uses Google Symbols glyphs; code uses M3 animated drawables (with a
selection animation). Ring weight and dot size may differ slightly.

### 3. Touch target

`minWidth` / `minHeight` are `0dp`, so the touch target depends on the parent
row.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use radio buttons for 2+ mutually exclusive options that are all visible at
  once. Always group them (`RadioButtonWithDescriptionLayout`) and preselect a
  sensible default when appropriate.
- Use a checkbox when multiple selections are allowed, and a switch for a single
  on/off setting.
- **Labeling**: Sentence case, concise, parallel phrasing across options.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Make the whole row clickable and at least `48dp` tall
  (`@dimen/min_touch_target_size`).
- **TalkBack**: Announce label + selected state + position in the group; the
  group container should expose the collection semantics.
- **Keyboard / D-pad**: Focus moves between options; focus feedback comes from
  the ripple state layer.

### 3. Icon Usage Guidelines

- Don't replace the radio glyph with custom icons. If an option needs an icon,
  use a row variant (e.g. `RichRadioButton`) that places the icon beside the
  label.
