# Component Spec: Switch

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Switch** (Slide Toggle / Toggle
Button) component across Figma, C++ Views (Desktop), WebUI (Desktop), and Clank
(Android).

______________________________________________________________________

## Overview

The **Switch** component represents a sliding, standard physical switch used for
simple on/off or enabled/disabled toggle configurations. It consists of an
elongated pill-shaped background track and a nested solid circular handle that
slides horizontally.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                        | WebUI (Desktop)                                                                                                      | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                              |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Switch`                                                                                                                            | `views::ToggleButton`                                                                      | `<cr-toggle>`                                                                                                        | `MaterialSwitchWithText` / `@style/Widget.BrowserUI.Switch`                                                                                                                                                                                                                                                                                                                                  |
| **Source Files**   | [Figma Link: `280:26373`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26373) | [ui/views/controls/button/toggle_button.h](//src/ui/views/controls/button/toggle_button.h) | [ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithText.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithText.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant    | Figma Component  | C++ Views (Desktop)                           | WebUI (Web Frontend)           | Clank (Android)                                 |
| :------------------- | :--------------- | :-------------------------------------------- | :----------------------------- | :---------------------------------------------- |
| **Selected (On)**    | `selected=true`  | Handle shifts right; track turns primary blue | Attribute: `checked` is active | `android:checked="true"` / `setChecked(true)`   |
| **Unselected (Off)** | `selected=false` | Handle shifts left; track turns grey          | Default unchecked/off state    | `android:checked="false"` / `setChecked(false)` |

______________________________________________________________________

## 3. Component States

| State                | Figma Component  | C++ Views (Desktop)                   | WebUI (Web Frontend)              | Clank (Android)                                                        |
| :------------------- | :--------------- | :------------------------------------ | :-------------------------------- | :--------------------------------------------------------------------- |
| **Default (Normal)** | `state=Default`  | `Button::ButtonState::STATE_NORMAL`   | Standard slide toggler appearance | `android:enabled="true"`                                               |
| **Disabled**         | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-toggle disabled>` | Attribute: `android:enabled="false"` (`@dimen/default_disabled_alpha`) |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute            | Figma Design Token                                                               | C++ Views (Desktop)                                     | WebUI (Web Frontend)                       | Clank (Android)                                                                  |
| :-------------------------- | :------------------------------------------------------------------------------- | :------------------------------------------------------ | :----------------------------------------- | :------------------------------------------------------------------------------- |
| **Selected Track Color**    | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)                            | `ui::kColorToggleButtonTrackOn`                         | `--cr-toggle-checked-bar-color`            | `@macro/default_control_color_active` / `?attr/colorPrimary`                     |
| **Selected Handle Color**   | `--desktop/sys/primary-colors/on-primary`<br>(`white`)                           | `ui::kColorToggleButtonThumbOn`                         | `--cr-toggle-checked-button-color`         | `@macro/default_icon_color_on_accent1` / `?attr/colorOnPrimary`                  |
| **Unselected Track Color**  | `--desktop/sys/surface-colors/surface-variant`<br>(`#e1e3e1`)                    | `ui::kColorToggleButtonTrackOff`                        | `--cr-toggle-unchecked-bar-color`          | `?attr/colorSurfaceContainerHighest`                                             |
| **Unselected Handle Color** | `--desktop/sys/outline-colors/outline`<br>(`#747775`)                            | `ui::kColorToggleButtonThumbOff`                        | `--cr-toggle-unchecked-button-color`       | `@macro/hairline_stroke_color` / `?attr/colorOutline`                            |
| **Disabled Track Color**    | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorToggleButtonTrackOnDisabled` / `OffDisabled` | `--cr-toggle-disabled-bar-color`           | `@dimen/default_disabled_alpha` overlay                                          |
| **Disabled Handle Color**   | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`)           | `ui::kColorToggleButtonThumbOnDisabled` / `OffDisabled` | `--cr-toggle-disabled-button-color`        | `@dimen/default_disabled_alpha` overlay                                          |
| **Corner Radius (Fully)**   | `--desktop/corner-radius/fully-rounded`<br>(`999px`)                             | Oval geometry matches bounded track borders             | `border-radius: 999px;` on track and thumb | Full pill radius defined by `Widget.Material3.CompoundButton.MaterialSwitch`     |
| **Track Dimensions**        | `26px` Width, `16px` Height                                                      | Configured inside slide layout metrics                  | `width: 26px; height: 16px;`               | Scaled via `@fraction/material_switch_scale_fraction` with min 48dp touch target |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Slide Animation Physics

- **Figma**: Transitions are static or utilize simple prototyping animations.
- **C++ Views**: Incorporates active spring/slide animations (`SlideAnimation`)
  inside `toggle_button.cc`.
- **WebUI**: Drives sliding thumb animations using CSS transitions
  (`transition: transform 150ms;`).
- **Clank (Android)**: Leverages Android's Material 3 `MaterialSwitch` native
  thumb track animators and state layers.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Drag and Swipe Handlers

- **Figma**: Strictly click-based.
- **Code**: WebUI, C++ Views, and Clank (`MaterialSwitchWithText`) all support
  drag/swipe touch gestures in addition to tap events.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Instant Result**: Slide switches indicate instant changes and should not
  require clicking a separate "Save" button.
- **Unambiguous Actions**: Avoid using switches where multi-choice or radio
  buttons fit better.
- **Clank Title & Summary**: Use
  [`MaterialSwitchWithTitleAndSummary`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/MaterialSwitchWithTitleAndSummary.java)
  when pairing the switch with explanatory setting text.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Aria Bindings / TalkBack**: Ensure a11y roles map to `role="switch"` or
  `android.widget.Switch`.
- **Keyboard Operation**: Focused switches are highlighted; toggle state using
  **Space** or **Enter**.
