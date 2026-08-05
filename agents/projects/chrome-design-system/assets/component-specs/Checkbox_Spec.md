# Component Spec: Checkbox

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Checkbox** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **Checkbox** component is a dual-state toggle control representing standard binary select conditions (True/False). It displays a square outline that fills with checkmark vectors when active, supporting hover masks, pressed states, and disabled effects.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Checkbox` | `views::Checkbox` | `<cr-checkbox>` |
| **Source Files** | [Figma Link: `280:22475`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-22475&m=dev) | [ui/views/controls/button/checkbox.h](//src/ui/views/controls/button/checkbox.h) | [ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts](//src/ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected** | `selected=true` | Checked box displays a checkmark | Attribute: `checked` is active |
| **Unselected** | `selected=false` | Box displays an empty square frame | Default unchecked state |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | `Button::ButtonState::STATE_NORMAL` | Default idle checklist style |
| **Hovered** | `state=Hovered` | `Button::ButtonState::STATE_HOVERED` | `:hover:not([disabled])` pseudo-class |
| **Pressed** | `state=Pressed` | `Button::ButtonState::STATE_PRESSED` | `:active` pseudo-class with ripple |
| **Disabled** | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-checkbox disabled>` |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected Box Background** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`) | `ui::kColorCheckboxActiveBackground` | `--cr-checkbox-checked-box-color` |
| **Checkmark Vector Color** | `white` | `ui::kColorCheckboxCheckMark` | `--cr-checkbox-checked-checkmark-color` |
| **Unselected Frame Outline** | `--desktop/sys/outline-colors/outline`<br>(`#747775`) | `ui::kColorCheckboxBorder` | `--cr-checkbox-unchecked-box-color` |
| **Disabled Container BG** | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorCheckboxDisabledBackground` | `--cr-checkbox-disabled-box-color` |
| **Corner Radius** | `--desktop/corner-radius/2`<br>(`2px`) | Matches default checkbox corner metrics | `border-radius: 2px;` |
| **Outer Dimension** | `16px` | Bounded by standard check metrics | `height: 16px; width: 16px;` |

---

## 5. Architectural & Implementation Gaps

### 1. Hardcoded Percentages vs. Vector Outlines
*   **Figma**: The checkmark vector is positioned using absolute percentages inside its 16px box.
*   **Code**: Both WebUI and Views draw checkmarks using dynamic vector paths (either inline SVGs in WebUI or path coordinates inside C++'s `Canvas::DrawImageInt`) to ensure razor-sharp graphics under variable high-DPI platform screen scalings.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Label Positioning
*   **Figma**: The check component only contains the 16px square checkbox node.
*   **Code**: Both `<cr-checkbox>` and `views::Checkbox` are typically instantiated with integrated companion text labels positioned on the right side. In C++, this text label is managed natively inside the checkbox's own layout layer, whereas Figma designs label nodes separately.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Standard Selection**: Use for independent multi-select toggle choices.
*   **Unambiguous States**: Always ensure checked vs unchecked is highly visible.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Keyboard Operation**: Highlighted via focus outline; toggled using **Space**.
*   **Accessibility Labels**: Feed explicit labels via `aria-label` or `SetName()`.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Button (Focus, click handler)
              └── views::LabelButton (Integrated text label/images)
                     └── views::Checkbox (Tick selection toggle state)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── CrLitElement (Lit reactive UI component)
              └── CrCheckboxElement (Reusable cr-checkbox element)
    ```
