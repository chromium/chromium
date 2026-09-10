# Component Spec: RadioButtons

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **RadioButtons** (Radio Button)
component across Figma, C++ Views (Desktop), WebUI (Desktop), and Clank
(Android).

______________________________________________________________________

## Overview

The **RadioButtons** component represents a circular selection item used in a
mutual exclusion group (Radio Group). It features an outer circle outline that
reveals a nested solid spot when active.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                                              | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                      |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `RadioButtons`                                                                                                                      | `views::RadioButton`                                                                     | `<cr-radio-button>`                                                                                                                          | `RadioButtonWithDescription` / `RichRadioButton` / `@style/Widget.BrowserUI.RadioButton`                                                                                                                                                                                                                                                                                                             |
| **Source Files**   | [Figma Link: `280:26336`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26336) | [ui/views/controls/button/radio_button.h](//src/ui/views/controls/button/radio_button.h) | [ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts](//src/ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescription.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescription.java)<br>[components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component  | C++ Views (Desktop)             | WebUI (Web Frontend)        | Clank (Android)                                 |
| :---------------- | :--------------- | :------------------------------ | :-------------------------- | :---------------------------------------------- |
| **Selected**      | `selected=true`  | Inner spot active inside circle | Attribute: `checked` active | `android:checked="true"` / `setChecked(true)`   |
| **Unselected**    | `selected=false` | Circular boundary frame only    | Unchecked default state     | `android:checked="false"` / `setChecked(false)` |

______________________________________________________________________

## 3. Component States

| State                | Figma Component  | C++ Views (Desktop)                   | WebUI (Web Frontend)                       | Clank (Android)                                                        |
| :------------------- | :--------------- | :------------------------------------ | :----------------------------------------- | :--------------------------------------------------------------------- |
| **Default (Normal)** | `state=Default`  | `Button::ButtonState::STATE_NORMAL`   | Idle radio group select style              | `android:enabled="true"`                                               |
| **Hovered**          | `state=Hovered`  | `Button::ButtonState::STATE_HOVERED`  | `:hover:not([disabled])` pseudo-class      | Hover layer overlay (`@dimen/default_hovered_alpha`)                   |
| **Pressed**          | `state=Pressed`  | `Button::ButtonState::STATE_PRESSED`  | `:active` pseudo-class with ripple overlay | `@color/control_highlight_color` / Ripple                              |
| **Disabled**         | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-radio-button disabled>`    | Attribute: `android:enabled="false"` (`@dimen/default_disabled_alpha`) |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute              | Figma Design Token                                                               | C++ Views (Desktop)                       | WebUI (Web Frontend)                    | Clank (Android)                                                                 |
| :---------------------------- | :------------------------------------------------------------------------------- | :---------------------------------------- | :-------------------------------------- | :------------------------------------------------------------------------------ |
| **Selected Active Ring/Spot** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)                            | `ui::kColorRadioButtonActiveBackground`   | `--cr-radio-button-checked-color`       | `@color/selection_control_button_tint_list` / `?attr/colorPrimary`              |
| **Unselected Outer Frame**    | `--desktop/sys/outline-colors/outline`<br>(`#747775`)                            | `ui::kColorRadioButtonBorder`             | `--cr-radio-button-unchecked-color`     | `@macro/hairline_stroke_color` / `?attr/colorOutline`                           |
| **Disabled Spot/Frame Color** | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorRadioButtonDisabledBackground` | `--cr-radio-button-disabled-color`      | `@color/default_icon_color_disabled` (`@dimen/default_disabled_alpha` = `0.38`) |
| **Corner Radius**             | `--desktop/corner-radius/fully-rounded`<br>(`999px`)                             | Circular clipping matches button radius   | `border-radius: 50%;` *(Circular mask)* | Native circular vector drawable                                                 |
| **Dimension (Diameter)**      | `20px`                                                                           | Bounded by standard radio size guidelines | `height: 20px; width: 20px;`            | 20dp circle with min 48dp touch target (`@dimen/min_touch_target_size`)         |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Complex Multi-Variant Groups in Clank

- **Desktop**: `<cr-radio-group>` and `views::RadioGroup` assume uniform radio
  buttons with standard text labels.
- **Clank (Android)**: Clank provides specialized rich radio implementations:
  - [`RadioButtonWithDescriptionLayout`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithDescriptionLayout.java):
    Manages single-selection across complex rows.
  - [`RadioButtonWithEditText`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RadioButtonWithEditText.java):
    Integrates inline text input when a "Custom" radio option is selected.
  - [`RichRadioButton`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/RichRadioButton.java):
    Provides card-style bounded radio selections with lead icons.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Group-Wide Focus Operations

- **Figma**: Treats each radio component as a standalone toggle.
- **Code**: Group management is automated. Both `views::RadioButton`,
  `<cr-radio-button>`, and `RadioButtonWithDescriptionLayout` group controls so
  that navigation jumps between group members intuitively.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Exclusive Choices**: Use only for mutually exclusive selection groups (2 or
  more items).
- **Implicit Save**: Selecting an option should instantly register.
- **Touch Friendly**: In Clank, ensure the entire container row responds to
  clicks, not just the 20dp radio circle.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Accessibility Labels**: Anchor companion text labels using
  `aria-labelledby`, `SetName()`, or `android:contentDescription`.
- **Keyboard Operation**: Navigate using Arrow Keys within the group; select
  with **Space**.
