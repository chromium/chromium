# Component Spec: RadioButtons

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **RadioButtons** (Radio Button) component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **RadioButtons** component represents a circular selection item used in a mutual exclusion group (Radio Group). It features a outer circle outline that reveals a nested solid spot when active.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `RadioButtons` | `views::RadioButton` | `<cr-radio-button>` |
| **Source Files** | [Figma Link: `280:26336`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26336&m=dev) | [ui/views/controls/button/radio_button.h](//src/ui/views/controls/button/radio_button.h) | [ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts](//src/ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected** | `selected=true` | Inner spot active inside circle | Attribute: `checked` active |
| **Unselected** | `selected=false` | Circular boundary frame only | Unchecked default state |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | `Button::ButtonState::STATE_NORMAL` | Idle radio group select style |
| **Hovered** | `state=Hovered` | `Button::ButtonState::STATE_HOVERED` | `:hover:not([disabled])` pseudo-class |
| **Pressed** | `state=Pressed` | `Button::ButtonState::STATE_PRESSED` | `:active` pseudo-class with ripple overlay |
| **Disabled** | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-radio-button disabled>` |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected Active Ring/Spot** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`) | `ui::kColorRadioButtonActiveBackground` | `--cr-radio-button-checked-color` |
| **Unselected Outer Frame** | `--desktop/sys/outline-colors/outline`<br>(`#747775`) | `ui::kColorRadioButtonBorder` | `--cr-radio-button-unchecked-color` |
| **Disabled Spot/Frame Color**| `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorRadioButtonDisabledBackground` | `--cr-radio-button-disabled-color` |
| **Corner Radius** | `--desktop/corner-radius/fully-rounded`<br>(`999px`) | Circular clipping matches button radius | `border-radius: 50%;` *(Circular mask)* |
| **Dimension (Diameter)** | `20px` | Bounded by standard radio size guidelines | `height: 20px; width: 20px;` |

---

## 5. Architectural & Implementation Gaps

### 1. Circular Masking Implementations
*   **Figma**: Uses `--desktop/corner-radius/fully-rounded` (`999px`) to turn a square frame into a circle.
*   **WebUI**: Enforces standard `border-radius: 50%` in its CSS definitions.
*   **C++ Views**: Manages circular borders and focus indicators natively through custom vector rendering parameters inside its drawing engine.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Group-Wide Focus Operations
*   **Figma**: Treats each radio component as a standalone toggle.
*   **Code**: Group management is automated. Both `views::RadioButton` and `<cr-radio-button>` are grouped under layout controllers. Pressing Tab jumps to the *active selected* option in the group, and keyboard arrows are used to toggle selection within the group, rather than individually tabbing every element.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Exclusive Choices**: Use only for mutually exclusive selection groups (2 or more items).
*   **Implicit Save**: Selecting an option should instantly register.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Accessibility Labels**: Anchor companion text labels using `aria-labelledby` or `SetName()`.
*   **Keyboard Operation**: Navigate using Arrow Keys within the group; select with **Space**.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Button (Focus, click handlers)
              └── views::LabelButton (Integrated text label support)
                     └── views::Checkbox (Tick selection toggle state)
                            └── views::RadioButton (Mutual exclusion toggles)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── CrLitElement (Lit reactive UI component)
              └── CrRadioButtonElement (Reusable cr-radio-button element)
    ```
