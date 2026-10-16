# Button Spec (Clank)

This document maps the specifications of the **Button** (Clank) component across
Figma and code.

______________________________________________________________________

## Overview

The **Button** component is the core interactive control in Chrome for Android
(Clank) for triggering actions in dialogs, promos, sheets, and settings. Figma
models a pill-shaped `Button` (`Filled` and `Text` types across Enabled, Hover,
Focused, Pressed, Disabled, and TalkBack-focused states) plus a full-width
`Button-tonal` that can be stacked (Top / Middle / Bottom segments). In code,
both map onto `ButtonCompat`: the pill variants map to its XML styles, and the
tonal stack maps to the `FilledButton.Tonal.ThemeOverlay.*` overlays used by
`ModalDialogView`'s button group.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                    | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Button`<br>`Button-tonal`                                                                                                                                                                                                                                                                                         | `ButtonCompat`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Source Files**   | [Figma Link: `Button` (`7015:48330`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7015-48330&m=dev)<br>[Figma Link: `Button-tonal` (`40533:404`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=40533-404&m=dev) | [ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java](//src/ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java)<br>[ui/android/java/res/values/styles.xml](//src/ui/android/java/res/values/styles.xml)<br>[components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java)<br>[components/browser_ui/modaldialog/android/java/res/values/styles.xml](//src/components/browser_ui/modaldialog/android/java/res/values/styles.xml) |

> Source page:
> [`↳ Buttons` (`6355:203172`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6355-203172)
> — documentation canvas containing light/dark showcases of both components.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Button`: `Type` (Filled | Text) × `State` (Enabled | Hover | Focused |
  Pressed | Disabled | Focused talkback).
- `Button-tonal`: boolean properties `topButton` | `middleButton` |
  `bottomButton` (position within a vertical stack; `middleButton=true` alone is
  the default/standalone button).

| Feature / Variant              | Figma Component                                              | Clank (Android)                                                                                                                                                            |
| :----------------------------- | :----------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Filled (Primary) Style**     | `Button` · `Type=Filled`                                     | `style="@style/FilledButton"` *(default `ButtonCompat` inflation style)*                                                                                                   |
| **Text Style**                 | `Button` · `Type=Text`                                       | `style="@style/TextButton"`                                                                                                                                                |
| **Tonal (single, standalone)** | `Button-tonal` · `middleButton=true` only (default)          | `@style/FilledTonalButton` (pill, generic)<br>or `R.style.FilledButton_Tonal_ThemeOverlay_SingleButton` (rounded-rect, modal dialog button group)                          |
| **Tonal Stack – Top**          | `Button-tonal` · `topButton=true`                            | `R.style.FilledButton_Tonal_ThemeOverlay_TopButton` (first of N>1 in `ModalDialogView#setupButtonGroup`)                                                                   |
| **Tonal Stack – Middle**       | `Button-tonal` · `middleButton=true`                         | `R.style.FilledButton_Tonal_ThemeOverlay_MiddleButton`                                                                                                                     |
| **Tonal Stack – Bottom**       | `Button-tonal` · `bottomButton=true`                         | `R.style.FilledButton_Tonal_ThemeOverlay_BottomButton` (last of N>1)                                                                                                       |
| **Outlined Style**             | *(Not modeled in Figma)*                                     | `style="@style/OutlinedButton"`                                                                                                                                            |
| **Enabled State**              | `State=Enabled`                                              | `android:enabled="true"` *(default)*                                                                                                                                       |
| **Hovered State**              | `State=Hover`                                                | `android:state_hovered="true"` in `rippleColor` selector                                                                                                                   |
| **Focused State**              | `State=Focused`                                              | `android:state_focused="true"` in `rippleColor` selector                                                                                                                   |
| **Pressed State**              | `State=Pressed`                                              | `android:state_pressed="true"` → `RippleDrawable` built by `RippleBackgroundHelper`                                                                                        |
| **Disabled State**             | `State=Disabled`                                             | `android:enabled="false"` (`state_enabled="false"` selectors in bg/text `ColorStateList`s)                                                                                 |
| **Focused TalkBack State**     | `State=Focused talkback`                                     | OS-rendered accessibility focus indicator (no Chromium code)                                                                                                               |
| **Tonal interactive states**   | *(Not modeled — `Button-tonal` only has `Variant=Default`)*  | Inherits `rippleColor=?attr/globalTextButtonRippleColor` (`FilledTonalButton` parent is `TextButton`; tonal overlays parent `FilledButton` → `filled_button_ripple_color`) |
| **Dark theme**                 | Separate "DARK theme" showcase instances (token mode switch) | Night-mode resolution of `?attr/colorPrimary`, `?attr/colorPrimaryContainer`, etc.                                                                                         |
| **Leading / Trailing Icon**    | *(Not modeled in Figma components)*                          | `AppCompatButton` compound drawables: `app:drawableStartCompat`, `app:drawableEndCompat`, `android:drawablePadding`                                                        |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

### Button (`Filled` / `Text`)

| Design Attribute                      | Figma Design Token                                                                                     | Clank (Android)                                                                                                                                       |
| :------------------------------------ | :----------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Filled Container Background**       | `--clank/sys/color/primary`<br>(light `#0b57d0` / dark `#a8c7fa`)                                      | `?attr/globalFilledButtonBgColor` → `@color/filled_button_bg_dynamic_list` (`?attr/colorPrimary`)                                                     |
| **Filled Label Foreground**           | `--clank/sys/color/onPrimary, op`<br>(light `#ffffff` / dark `#062e6f`)                                | `?attr/globalFilledButtonTextColor` → `@color/default_text_color_on_accent1_list` (`?attr/colorOnPrimary`)                                            |
| **Filled Hover State Layer**          | `--clank/sys/state_layer/surface_8%`<br>(light `rgba(255,255,255,.08)` / dark `rgba(6,46,111,.08)`)    | `@color/filled_button_ripple_color` → `@color/white_alpha_8` (`state_hovered`)                                                                        |
| **Filled Focused State Layer**        | `--clank/sys/state_layer/surface_10%`<br>(light `rgba(255,255,255,.10)` / dark `rgba(6,46,111,.10)`)   | `@color/filled_button_ripple_color` → `@color/white_alpha_24` (`state_focused`)                                                                       |
| **Filled Pressed State Layer**        | `--clank/sys/state_layer/surface_8%`                                                                   | `@color/filled_button_ripple_color` → `@color/white_alpha_32` (`state_pressed`; alpha doubled by `RippleBackgroundHelper`)                            |
| **Filled Disabled Container**         | `--clank/sys/state_layer/onSurface_10%`<br>(light `rgba(31,31,31,.10)` / dark `rgba(227,227,227,.10)`) | `filled_button_bg_dynamic_list` → `?attr/colorOnSurface` @ `@dimen/filled_button_bg_disabled_alpha` (`0.12`)                                          |
| **Text Container Background**         | Transparent                                                                                            | `buttonColor="@android:color/transparent"` (`@style/TextButton`)                                                                                      |
| **Text Label Foreground**             | `--clank/sys/color/primary`<br>(light `#0b57d0` / dark `#a8c7fa`)                                      | `?attr/globalTextButtonTextColor` → `@color/default_text_color_accent1_tint_list`                                                                     |
| **Text Hover State Layer**            | `--clank/sys/state_layer/primary_8%`                                                                   | `?attr/globalTextButtonRippleColor` → `@color/text_button_ripple_color_list` (`@macro/default_control_color_active` @ `default_hovered_alpha` `0.04`) |
| **Text Focused State Layer**          | `--clank/sys/state_layer/primary_10%`                                                                  | `text_button_ripple_color_list` @ `default_focused_alpha` (`0.06`)                                                                                    |
| **Text Pressed State Layer**          | `--clank/sys/state_layer/primary_8%`                                                                   | `text_button_ripple_color_list` @ `default_pressed_alpha` (`0.06`)                                                                                    |
| **Disabled Label Foreground**         | `--clank/sys/state_layer/onSurface_38%`<br>(light `rgba(31,31,31,.38)` / dark `rgba(227,227,227,.38)`) | `?attr/colorOnSurface` @ `@dimen/default_disabled_alpha` (`0.38`)                                                                                     |
| **TalkBack Focus Border**             | `--clank/sys/static/talkbackColor` (`#00ac19`, `4px` solid)                                            | OS-rendered (not in Chromium resources)                                                                                                               |
| **Typography (composite)**            | `buttonText`                                                                                           | `@style/TextAppearance.Button.Text.Filled` (Filled) / `@style/TextAppearance.Button.Text.Blue` (Text)                                                 |
| **Font Family / Weight**              | `--clank/font/font_family/default` (`Google Sans`) / `--clank/font/font_weight/medium` (`500`)         | `sans-serif-medium` (`TextAppearance.MediumStyle`; Google Sans downstream)                                                                            |
| **Font Size / Line Height**           | `--clank/font/font_size/buttonText` (`14px`) / `--clank/font/line_height/buttonText` (`20px`)          | `@dimen/text_size_medium` (`14sp`) / `@dimen/text_size_medium_leading` (`20sp`)                                                                       |
| **Letter Spacing**                    | `--clank/font/letter_spacing/buttonText` (`0px`)                                                       | Default (`0`)                                                                                                                                         |
| **Touch Target Height**               | `48px` (`40px` visual + `4px` top/bottom)                                                              | `android:minHeight="@dimen/min_touch_target_size"` (`48dp`)                                                                                           |
| **Outer Vertical Inset**              | `--clank/shape/spacing/4` (`4px`)                                                                      | `verticalInset="@dimen/button_bg_vertical_inset"` (`4dp`)                                                                                             |
| **Minimum Width**                     | Hugs content                                                                                           | `android:minWidth="@dimen/button_min_width"` (`88dp`)                                                                                                 |
| **Inner Horizontal Padding (Filled)** | `--clank/shape/spacing/24` (`24px`)                                                                    | `24dp` (`@style/FilledButton`)                                                                                                                        |
| **Inner Horizontal Padding (Text)**   | `--clank/shape/spacing/24` (`24px`)                                                                    | `8dp` (`@style/TextButton`)                                                                                                                           |
| **Inner Vertical Padding**            | `--clank/shape/spacing/8` (`8px`)                                                                      | `5dp` (`@style/ButtonCompatBase`)                                                                                                                     |
| **Corner Radius**                     | `--clank/shape/corner_radius/100` (`100px`)                                                            | `@dimen/button_compat_corner_radius` (`500dp`)                                                                                                        |

### Button-tonal (single & stack)

| Design Attribute                  | Figma Design Token                                                                | Clank (Android)                                                                                                                                                                       |
| :-------------------------------- | :-------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Container Background**          | `--clank/sys/color/primaryContainer, pc`<br>(light `#d3e3fd` / dark `#0842a0`)    | `FilledButton.Tonal.ButtonStyle` → `buttonColor="?attr/colorPrimaryContainer"`<br>`@style/FilledTonalButton` → `?attr/globalFilledTonalButtonBgColor` (`?attr/colorPrimaryContainer`) |
| **Label Foreground**              | `--clank/sys/color/onPrimaryContainer, opc`<br>(light `#0842a0` / dark `#d3e3fd`) | `buttonTextColor="@macro/default_text_color_on_accent1_container"`<br>`@style/FilledTonalButton` → `@color/default_text_color_on_accent1_container_list`                              |
| **Typography**                    | `buttonText` (Google Sans Medium 14/20)                                           | `@style/TextAppearance.Button.Text.Blue` (overlay) / `.Filled` (`FilledTonalButton`), color overridden by `buttonTextColor`                                                           |
| **Segment Height**                | `56px` (visual)                                                                   | `18dp` padding × 2 + `20sp` line = `56dp` view; visual bg = `52dp` after `2dp` inset                                                                                                  |
| **Horizontal / Vertical Padding** | `--clank/shape/spacing/24` (`24px`) / `--clank/shape/spacing/10` (`10px`)         | `@dimen/modal_dialog_button_group_padding` (`18dp`) on all sides (set in `setupButtonGroup`)                                                                                          |
| **Gap Between Segments**          | `--clank/shape/spacing/4` (`4px`)                                                 | `verticalInset="@dimen/button_group_button_inset"` (`2dp`) top + bottom → `4dp` visual gap                                                                                            |
| **Outer Corner Radius**           | `--clank/shape/corner_radius/12` (`12px`)                                         | `@dimen/button_group_large_radius` (`16dp`) via `rippleCornerRadius{TopStart,TopEnd,BottomStart,BottomEnd}`                                                                           |
| **Inner Corner Radius**           | `--clank/shape/corner_radius/4` (`4px`)                                           | `@dimen/button_group_small_radius` (`8dp`)                                                                                                                                            |
| **Width**                         | Fixed `332px` (fills container)                                                   | `match_parent` within `@id/button_group` (`24dp` side padding in `modal_dialog_view.xml`)                                                                                             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Tonal stack corner radii (`12/4px` vs. `16/8dp`)

- **Figma**: `Button-tonal` uses `corner_radius/12` on outer corners and
  `corner_radius/4` on inner corners.
- **Clank**: `button_group_large_radius` = **`16dp`**,
  `button_group_small_radius` = **`8dp`** in
  [modaldialog dimens.xml](//src/components/browser_ui/modaldialog/android/java/res/values/dimens.xml).
  One side needs to be aligned.

### 2. Tonal stack padding & height

- **Figma**: `24px` horizontal / `10px` vertical padding, fixed `56px` segments.
- **Clank**: `setupButtonGroup` sets a uniform **`18dp`** padding on all sides,
  so segment height is content-driven (`56dp` view → `52dp` visible bg after the
  `2dp` inset) and grows with font scale.

### 3. Tonal stack is not a reusable widget

- The Top/Middle/Bottom logic lives only in `ModalDialogView#setupButtonGroup`
  (picks overlay by index/count) and the overlays live in the **modaldialog**
  module. There is no general-purpose `ButtonGroup` view; other surfaces must
  copy the overlay styles or depend on modaldialog resources.

### 4. Two tonal implementations in code

- `@style/FilledTonalButton` (`ui/android`) is a **pill** using theme attrs
  `?attr/globalFilledTonalButton*`, whereas the Figma `Button-tonal` is a
  **rounded-rect full-width** button that matches the modal-dialog overlays
  (which hard-code `?attr/colorPrimaryContainer` and
  `@macro/default_text_color_on_accent1_container` instead of the `global*`
  attrs). Figma has no pill-tonal variant.

### 5. State-layer / disabled alpha discrepancies (`Button`)

- **Filled**: Figma `8% / 10% / 8%` (hover / focus / pressed) vs. code
  `white_alpha_8 / 24 / 32`. In dark theme Figma's `surface_*%` layer resolves
  to `onPrimary` (`rgba(6,46,111,…)`), whereas code always uses **white**.
- **Text**: Figma `8% / 10% / 8%` vs. code `4% / 6% / 6%`.
- **Disabled container**: Figma `10%` vs. code `0.12`.
- `RippleBackgroundHelper` doubles the pressed alpha because `RippleDrawable`
  halves it.

### 6. Pill radius (`100px` vs. `500dp`) & dynamic color

- Code uses `500dp` so wrapped labels at XL font scale stay pill-shaped.
- Figma binds static baseline hex values; code resolves through theme attrs and
  Material 3 dynamic color (`?attr/colorPrimary`,
  `?attr/colorPrimaryContainer`).

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Text button horizontal padding (`24px` vs. `8dp`)

`@style/TextButton` uses `8dp` start/end padding (intentional, aligns with body
text in dialogs); Figma uses `24px` for both types.

### 2. Missing tonal states in Figma

`Button-tonal` only has `Variant=Default`; Hover / Focused / Pressed / Disabled
/ TalkBack states are undefined. In code, tonal overlays (parent `FilledButton`)
inherit the **white** `filled_button_ripple_color`, which is low-contrast on a
light `primaryContainer` background; `@style/FilledTonalButton` (parent
`TextButton`) inherits the primary-tinted text ripple.

### 3. Code variants missing in Figma

`@style/OutlinedButton` (`1dp` `?attr/globalOutlinedButtonBorderColor` border,
optional dashed via `app:buttonBorderStyle`) and pill-shaped
`@style/FilledTonalButton` are not modeled.

### 4. TalkBack focus

Figma models a `4px` `#00ac19` border; Clank relies on the OS accessibility
focus overlay.

### 5. Minimum width (`88dp`)

`ButtonCompatBase` enforces `minWidth=88dp`; short labels (e.g. "OK") render
wider than Figma's hug-content frames unless overridden.

### 6. Tonal stack text appearance

Overlay uses `TextAppearance.Button.Text.Blue` (color overridden by
`buttonTextColor`), while `FilledTonalButton` uses `.Filled`. Size/weight match
`buttonText`, but the inconsistent base style is a maintenance risk.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Variant selection**:
  - `Type=Filled` / `@style/FilledButton`: the single primary, high-emphasis
    action on a surface.
  - `Type=Text` / `@style/TextButton`: secondary or dismissive actions
    ("Cancel", "No thanks").
  - `Button-tonal`: equally-weighted actions and/or when **2+ actions** are
    offered at the same time (per Figma guidance). Stack them vertically with
    Top / Middle / Bottom segments (single button → fully rounded).
- **Action pairing**: Pair Filled + Text via
  [`DualControlLayout`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DualControlLayout.java)
  or `ModalDialogView` (primary at trailing edge, auto-stacks on overflow). For
  tonal stacks in dialogs, use `ModalDialogProperties` button specs so
  `ModalDialogView#setupButtonGroup` applies the right segment style.
- **Labeling**: Concise, active verbs in Sentence case ("Save password", "Turn
  on sync"); see go/write-buttons. Do not force `singleLine` if it would clip
  localized text at large font scales.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Visible height `40dp` with a `48dp` touch target
  (`@dimen/min_touch_target_size`) provided out-of-the-box by `verticalInset`.
  Account for this invisible area when speccing mocks. Tonal segments are `56dp`
  tall, comfortably exceeding `48dp`.
- **TalkBack**: Provide `android:contentDescription` when visible text is
  ambiguous (`setupButtonGroup` forwards `spec.getContentDescription()`).
- **Keyboard / D-pad**: `ButtonCompatBase` sets `focusable` / `clickable`; focus
  feedback comes from the `state_focused` ripple layer.
- **Security**: Dialog buttons apply `setFilterTouchForSecurityIfNecessary` to
  prevent tap-jacking.

### 3. Icon Usage Guidelines

- Figma `Button` / `Button-tonal` are text-only. `ButtonCompat` supports
  compound drawables (`app:drawableStartCompat`, `android:drawablePadding`,
  `app:drawableTint`) when an icon reinforces meaning (modal dialogs use
  `@dimen/modal_dialog_button_with_icon_start_padding` = `12dp` and
  `@dimen/modal_dialog_button_with_icon_text_padding` = `8dp`).
- For icon-only actions use
  [`ChromeImageButton`](//src/ui/android/java/src/org/chromium/ui/widget/ChromeImageButton.java).
