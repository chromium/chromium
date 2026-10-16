# Switch Spec (Clank)

This document maps the specifications of the **Switch** (Clank) component across
Figma and code.

______________________________________________________________________

## Overview

The **Switch** is the preferred control for adjusting settings: it toggles a
binary option (On/Off) that takes effect immediately (per the Figma
description). Figma models `Selected` True / False × Enabled, Hovered, Focused,
Pressed and Disabled states. In code, Chrome uses the Material 3
`MaterialSwitch` scaled to 80% via `Widget.BrowserUI.Switch`, wrapped by
`MaterialSwitchWithText` and the settings `SwitchPreference`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                         | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Switch`                                                                                                                                                | `MaterialSwitch`<br>`MaterialSwitchWithText`<br>`MaterialSwitchWithTitleAndSummary`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Source Files**   | [Figma Link: `Switch` (`32019:16780`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32019-16780&m=dev) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithText.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithText.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithTitleAndSummary.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithTitleAndSummary.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)<br>[components/browser_ui/widget/android/java/res/layout/preference_switch.xml](//src/components/browser_ui/widget/android/java/res/layout/preference_switch.xml)<br>[components/browser_ui/widget/android/java/res/values/dimens.xml](//src/components/browser_ui/widget/android/java/res/values/dimens.xml) |

> Source page:
> [`Components/Selection-controls` (`43256:439`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=43256-439)
> — documentation canvas with light and dark showcases.
>
> `MaterialSwitch` comes from Material Components
> (`com.google.android.material:material` **1.14.0**, see
> `third_party/android_deps/committed/libs/com_google_android_material_material/README.chromium`).
> The app theme
> ([themes.xml](//src/components/browser_ui/theme/android/templates/res/values/themes.xml))
> sets `materialSwitchStyle` → `@style/Widget.BrowserUI.Switch` (parent
> `Widget.Material3.CompoundButton.MaterialSwitch`). Chromium has **no**
> `thumbTint` / `trackTint` / `trackDecorationTint` overrides, so every color
> below is an M3 default. These were checked against MDC 1.14.0 source:
> `materialswitch/res/values/tokens.xml` and the
> `mtrl_switch_{thumb,track,track_decoration}_tint.xml` color lists.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Switch`: `Selected` (True | False) × `State` (Enabled | Hovered | Focused |
  Pressed | Disabled).

| Feature / Variant | Figma Component                     | Clank (Android)                                                                                                                 |
| :---------------- | :---------------------------------- | :------------------------------------------------------------------------------------------------------------------------------ |
| **Selected**      | `Selected=True`                     | `setChecked(true)`                                                                                                              |
| **Unselected**    | `Selected=False`                    | `setChecked(false)`                                                                                                             |
| **Enabled**       | `State=Enabled`                     | Default                                                                                                                         |
| **Hovered**       | `State=Hovered`                     | `state_hovered`: track tint only (thumb tint has **no** hover entry); state layer only where a ripple background exists         |
| **Focused**       | `State=Focused`                     | `state_focused`: track tint only (thumb tint has **no** focus entry); M3 focus ring (`focusRingsInset -4dp`)                    |
| **Pressed**       | `State=Pressed`                     | `state_pressed`: thumb grows to the pressed shape and switches to the pressed color                                             |
| **Disabled**      | `State=Disabled`                    | `android:enabled="false"`                                                                                                       |
| **With label**    | *(Not modeled — track only)*        | `MaterialSwitchWithText`; `MaterialSwitchWithTitleAndSummary`; settings `@style/SwitchPreference` → `@layout/preference_switch` |
| **Thumb icon**    | *(Not modeled)*                     | M3 `app:thumbIcon` (`16dp`, unused by default)                                                                                  |
| **Dark theme**    | "Dark" showcase (token mode switch) | Night-mode resolution of the M3 color roles                                                                                     |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                           | Figma Design Token                                                                                                        | Clank (Android) — M3 defaults, MDC 1.14.0                                                                                                                                              |
| :----------------------------------------- | :------------------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Track – selected**                       | `--clank/sys/color/primary` (light `#0b57d0` / dark `#a8c7fa`)                                                            | `m3_comp_switch_selected_track_color` → `?attr/colorPrimary`                                                                                                                           |
| **Track – unselected**                     | `--clank/sys/color/scontainerhighest, schh` (light `#dde3ea` / dark `#333537`)                                            | `m3_comp_switch_unselected_track_color` → `?attr/colorSurfaceContainerHighest`                                                                                                         |
| **Track outline – unselected**             | `--clank/sys/color/outline` (light `#747775` / dark `#8e918f`), `2px`                                                     | `m3_comp_switch_unselected_track_outline_color` → `?attr/colorOutline` (transparent when checked)                                                                                      |
| **Thumb – selected**                       | `#ffffff` (light)                                                                                                         | `m3_comp_switch_selected_handle_color` → `?attr/colorOnPrimary`                                                                                                                        |
| **Thumb – selected, hovered / focused**    | `#d3e3fd` (`primaryContainer`)                                                                                            | No hover/focus entry in `mtrl_switch_thumb_tint`, so it stays `?attr/colorOnPrimary` (tokens define `colorPrimaryContainer` but don't use it)                                          |
| **Thumb – selected, pressed**              | `#d3e3fd` (`primaryContainer`)                                                                                            | `m3_comp_switch_selected_pressed_handle_color` → `?attr/colorPrimaryContainer`                                                                                                         |
| **Thumb – unselected**                     | `#747775` (`outline`)                                                                                                     | `m3_comp_switch_unselected_handle_color` → `?attr/colorOutline`                                                                                                                        |
| **Thumb – unselected, hovered / focused**  | `#444746` (`onSurfaceVariant`)                                                                                            | No hover/focus entry, so it stays `?attr/colorOutline`                                                                                                                                 |
| **Thumb – unselected, pressed**            | `#444746` (`onSurfaceVariant`)                                                                                            | `m3_comp_switch_unselected_pressed_handle_color` → `?attr/colorOnSurfaceVariant`                                                                                                       |
| **State layer – selected**                 | `primary` @ 8% (hover) / 10% (focus) / 10% (pressed), `32px` circle                                                       | Tokens: `colorPrimary` @ `m3_sys_state_{hover,focus,pressed}_state_layer_opacity`; Chromium `preference_switch.xml` sets `android:background="@null"` (no ripple on the switch itself) |
| **State layer – unselected**               | `onSurface` (`#1f1f1f`) @ 8% / 10% / 10%                                                                                  | Tokens: `colorOnSurface` @ the same opacities (same `@null` background caveat)                                                                                                         |
| **Disabled track – selected**              | `--clank/sys/state_layer/onSurface_12%`                                                                                   | `?attr/colorOnSurface` @ `0.12`                                                                                                                                                        |
| **Disabled track – unselected**            | `--clank/sys/state_layer/scontainerhighest_10%`                                                                           | `?attr/colorSurfaceContainerHighest` @ `0.12`                                                                                                                                          |
| **Disabled outline – unselected**          | `onSurface_12%`, `2px`                                                                                                    | `?attr/colorOnSurface` @ `0.12`                                                                                                                                                        |
| **Disabled thumb – selected**              | `#ffffff`                                                                                                                 | `?attr/colorSurface` @ `1.0`                                                                                                                                                           |
| **Disabled thumb – unselected**            | `onSurface` @ 38%                                                                                                         | `?attr/colorOnSurface` @ `0.38`                                                                                                                                                        |
| **Track size**                             | `42 × 26px`                                                                                                               | `52 × 32dp` (`m3_comp_switch_track_*`) × `@fraction/material_switch_scale_fraction` (`0.8`) = `41.6 × 25.6dp`                                                                          |
| **Thumb diameter (unsel / sel / pressed)** | `14` / `20` / `22px`                                                                                                      | `16` / `24` / `28dp` (`mtrl_switch_thumb_path_*`) × `0.8` = `12.8` / `19.2` / `22.4dp`                                                                                                 |
| **Track padding**                          | `--clank/shape/spacing/3` (`3px`) sel, `--clank/shape/spacing/7` (`7px`) unsel, `--clank/shape/spacing/2` (`2px`) pressed | Implied by the thumb geometry in a `32dp` thumb viewport (not configurable)                                                                                                            |
| **Corner radius**                          | `--clank/shape/corner_radius/80` (`80px`)                                                                                 | Fully rounded track path (`mtrl_switch_track_path`)                                                                                                                                    |
| **Min height / touch target**              | *(Not modeled)*                                                                                                           | `@dimen/switch_pre_scale_min_height` (`60dp`, about `48dp` after scale); `MaterialSwitchWithText` `@dimen/switch_with_text_min_height` (`48dp`)                                        |
| **Label typography**                       | *(Not modeled)*                                                                                                           | `MaterialSwitchWithText`: `@style/TextAppearance.TextLarge.Primary`                                                                                                                    |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Scaled M3 widget (crbug/1248590)

- `Widget.BrowserUI.Switch` applies `scaleX/scaleY = 0.8` to the stock
  `MaterialSwitch`. Figma rounds to whole pixels (`42 × 26`, thumb `14/20/22`);
  code renders fractional sizes (`41.6 × 25.6`, `12.8/19.2/22.4`), and the
  unselected thumb is visibly smaller in code (`12.8` vs. `14`). Scaling is a
  draw transform, so layout bounds stay at the unscaled size and `minHeight` is
  `60dp` to land around `48dp`.

### 2. No Chromium color layer

- All colors come from M3 defaults. To match Figma's hover/focus thumb colors or
  the `10%` disabled track, Chromium would need to add its own tint lists.

### 3. Switch is not the interaction target in settings

- `preference_switch.xml` makes the switch `focusable="false"`,
  `clickable="false"`, `background="@null"`. The row handles clicks, focus and
  ripple, so Figma's 32px circular state layer isn't rendered in settings rows.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Hovered / Focused thumb color (verified)

Figma changes the thumb to `primaryContainer` (selected) / `onSurfaceVariant`
(unselected) on hover and focus. MDC 1.14.0's `mtrl_switch_thumb_tint.xml` only
has `state_pressed` entries, so the thumb color doesn't change on hover/focus in
code.

### 2. Disabled unselected track opacity

Figma `10%` vs. M3 `12%`.

### 3. Disabled selected thumb

Figma uses literal white; code uses `?attr/colorSurface`, which matches in light
mode but is dark (`#131314` baseline) in dark mode. The Figma dark showcase
wasn't inspected in detail.

### 4. Hovered / Focused thumb size

In Figma the thumb stays the same size (`20` / `14px`) for Hovered and Focused.
This matches code, where only Pressed changes the thumb shape.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use a switch for a single binary setting that applies immediately. Don't use
  it for actions that need confirmation; use a checkbox in forms that are
  submitted.
- Prefer `MaterialSwitchWithText` or `SwitchPreference` (settings) over a raw
  `MaterialSwitch`.
- **Labeling**: Describe the feature in its "on" state, in Sentence case (e.g.
  "Show suggestions").

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Make the whole row clickable and at least `48dp` tall
  (`@dimen/switch_with_text_min_height`).
- **TalkBack**: `MaterialSwitchWithText` makes the `LinearLayout` focusable and
  implements `Checkable`, so TalkBack announces label + on/off state once. The
  inner switch isn't focusable.
- **Keyboard / D-pad**: Focus lands on the row. The M3 focus ring
  (`focusRingsInset -4dp`) applies only when the switch itself is focusable.

### 3. Icon Usage Guidelines

- Chrome doesn't use the M3 `thumbIcon` slot by default. If you add one, use a
  `16dp` icon (`mtrl_switch_thumb_icon_size`) tinted by
  `mtrl_switch_thumb_icon_tint`.
