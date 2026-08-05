# Component Spec: Switch

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Switch** (Slide Toggle / Toggle Button) component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **Switch** component represents a sliding, standard physical switch used for simple on/off or enabled/disabled toggle configurations. It consists of an elongated pill-shaped background track and a nested solid circular handle that slides horizontally.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Switch` | `views::ToggleButton` | `<cr-toggle>` |
| **Source Files** | [Figma Link: `280:26373`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26373&m=dev) | [ui/views/controls/button/toggle_button.h](//src/ui/views/controls/button/toggle_button.h) | [ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected (On)** | `selected=true` | Handle shifts right; track turns primary blue | Attribute: `checked` is active |
| **Unselected (Off)** | `selected=false` | Handle shifts left; track turns grey | Default unchecked/off state |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | `Button::ButtonState::STATE_NORMAL` | Standard slide toggler appearance |
| **Disabled** | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-toggle disabled>` |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Selected Track Color** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`) | `ui::kColorToggleButtonTrackOn` | `--cr-toggle-checked-bar-color` |
| **Selected Handle Color** | `--desktop/sys/primary-colors/on-primary`<br>(`white`) | `ui::kColorToggleButtonThumbOn` | `--cr-toggle-checked-button-color` |
| **Unselected Track Color** | `--desktop/sys/surface-colors/surface-variant`<br>(`#e1e3e1`) | `ui::kColorToggleButtonTrackOff` | `--cr-toggle-unchecked-bar-color` |
| **Unselected Handle Color**| `--desktop/sys/outline-colors/outline`<br>(`#747775`) | `ui::kColorToggleButtonThumbOff` | `--cr-toggle-unchecked-button-color` |
| **Disabled Track Color** | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorToggleButtonTrackOnDisabled` / `OffDisabled` | `--cr-toggle-disabled-bar-color` |
| **Disabled Handle Color** | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`) | `ui::kColorToggleButtonThumbOnDisabled` / `OffDisabled` | `--cr-toggle-disabled-button-color` |
| **Corner Radius (Fully)** | `--desktop/corner-radius/fully-rounded`<br>(`999px`) | Oval geometry matches bounded track borders | `border-radius: 999px;` on track and thumb |
| **Track Dimensions** | `26px` Width, `16px` Height | Configured inside slide layout metrics | `width: 26px; height: 16px;` |

---

## 5. Architectural & Implementation Gaps

### 1. Slide Animation Physics
*   **Figma**: Transitions are static or utilize simple prototyping animations.
*   **C++ Views**: Incorporates active spring/slide animations (`SlideAnimation`) inside `toggle_button.cc` to smoothly animate the thumb handle across the track upon toggles.
*   **WebUI**: Drives sliding thumb animations using standard CSS transitions (`transition: transform 150ms;`) defined inside `cr_toggle.css`.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Drag and Swipe Handlers
*   **Figma**: Strictly click-based.
*   **Code**: Both WebUI `<cr-toggle>` (especially on touch-enabled platforms) and Views `views::ToggleButton` support swipe/drag input gestures in addition to standard clicks, allowing users to physically drag the slider handles across the track boundaries.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Instant Result**: Slide switches indicate instant changes (such as toggling dark mode or turning off Wi-Fi) and should not require clicking a separate "Save" or "Apply" button.
*   **Unambiguous Actions**: Avoid using switches where multi-choice or radio buttons fit better.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Aria Bindings**: Ensure a11y roles map to `role="switch"` and track state via `aria-checked`.
*   **Keyboard Operation**: Focused switches are highlighted; toggle state using **Space** or **Enter**.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Button (Focus, click handlers)
              └── views::ToggleButton (Handles track bounds, thumb sliders, and slide animations)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── CrLitElement (Lit reactive UI component)
              └── CrToggleElement (Reusable cr-toggle element)
    ```
