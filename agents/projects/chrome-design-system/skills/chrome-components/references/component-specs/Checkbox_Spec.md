# Component Spec: Checkbox

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Checkbox** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Checkbox** component is a dual-state toggle control representing standard
binary select conditions (True/False). It displays a square outline that fills
with checkmark vectors when active, supporting hover masks, pressed states, and
disabled effects.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                              | WebUI (Desktop)                                                                                                              | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Checkbox`                                                                                                                          | `views::Checkbox`                                                                | `<cr-checkbox>`                                                                                                              | `CheckBoxWithDescription` / `@style/Widget.BrowserUI.CheckBox`                                                                                                                                                                                                                                                                                                                                 |
| **Source Files**   | [Figma Link: `280:22475`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-22475) | [ui/views/controls/button/checkbox.h](//src/ui/views/controls/button/checkbox.h) | [ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts](//src/ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/CheckBoxWithDescription.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/CheckBoxWithDescription.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component  | C++ Views (Desktop)                | WebUI (Web Frontend)           | Clank (Android)                                 |
| :---------------- | :--------------- | :--------------------------------- | :----------------------------- | :---------------------------------------------- |
| **Selected**      | `selected=true`  | Checked box displays a checkmark   | Attribute: `checked` is active | `android:checked="true"` / `setChecked(true)`   |
| **Unselected**    | `selected=false` | Box displays an empty square frame | Default unchecked state        | `android:checked="false"` / `setChecked(false)` |

______________________________________________________________________

## 3. Component States

| State                | Figma Component  | C++ Views (Desktop)                   | WebUI (Web Frontend)                  | Clank (Android)                                                        |
| :------------------- | :--------------- | :------------------------------------ | :------------------------------------ | :--------------------------------------------------------------------- |
| **Default (Normal)** | `state=Default`  | `Button::ButtonState::STATE_NORMAL`   | Default idle checklist style          | `android:enabled="true"`                                               |
| **Hovered**          | `state=Hovered`  | `Button::ButtonState::STATE_HOVERED`  | `:hover:not([disabled])` pseudo-class | Hover layer overlay (`@dimen/default_hovered_alpha`)                   |
| **Pressed**          | `state=Pressed`  | `Button::ButtonState::STATE_PRESSED`  | `:active` pseudo-class with ripple    | `@color/control_highlight_color` / Ripple                              |
| **Disabled**         | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-checkbox disabled>`   | Attribute: `android:enabled="false"` (`@dimen/default_disabled_alpha`) |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute             | Figma Design Token                                                               | C++ Views (Desktop)                     | WebUI (Web Frontend)                    | Clank (Android)                                                                 |
| :--------------------------- | :------------------------------------------------------------------------------- | :-------------------------------------- | :-------------------------------------- | :------------------------------------------------------------------------------ |
| **Selected Box Background**  | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)                            | `ui::kColorCheckboxActiveBackground`    | `--cr-checkbox-checked-box-color`       | `@color/selection_control_button_tint_list` / `?attr/colorPrimary`              |
| **Checkmark Vector Color**   | `white`                                                                          | `ui::kColorCheckboxCheckMark`           | `--cr-checkbox-checked-checkmark-color` | `@android:color/white`                                                          |
| **Unselected Frame Outline** | `--desktop/sys/outline-colors/outline`<br>(`#747775`)                            | `ui::kColorCheckboxBorder`              | `--cr-checkbox-unchecked-box-color`     | `@macro/hairline_stroke_color` / `?attr/colorOutline`                           |
| **Disabled Container BG**    | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorCheckboxDisabledBackground`  | `--cr-checkbox-disabled-box-color`      | `@color/default_icon_color_disabled` (`@dimen/default_disabled_alpha` = `0.38`) |
| **Typography (Label)**       | `--desktop/font/body` / `--desktop/font_size/body-three`                         | Native Typography Provider              | Default font inheritance                | `@style/TextAppearance.TextMedium.Primary`                                      |
| **Corner Radius**            | `--desktop/corner-radius/2`<br>(`2px`)                                           | Matches default checkbox corner metrics | `border-radius: 2px;`                   | Standard Material 3 CheckBox corner radius (`2dp`)                              |
| **Outer Dimension**          | `16px`                                                                           | Bounded by standard check metrics       | `height: 16px; width: 16px;`            | 18dp box with min 48dp touch target (`@dimen/min_touch_target_size`)            |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Touch Target & Integrated Description

- **Desktop**: `<cr-checkbox>` and `views::Checkbox` are compact and designed
  for fine cursor interaction.
- **Clank (Android)**:
  [`CheckBoxWithDescription`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/CheckBoxWithDescription.java)
  expands the touch target to a minimum of 48dp and integrates support for
  multi-line primary title + secondary summary descriptions out-of-the-box.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Label Positioning

- **Figma**: The check component only contains the 16px square checkbox node.
- **Code**: `<cr-checkbox>`, `views::Checkbox`, and `CheckBoxWithDescription`
  are all instantiated with integrated companion text labels positioned on the
  right side.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Standard Selection**: Use for independent multi-select toggle choices.
- **Unambiguous States**: Always ensure checked vs unchecked is highly visible.
- **Clank Accessibility**: Ensure the entire row containing the checkbox and
  description is clickable to maximize the target area.
