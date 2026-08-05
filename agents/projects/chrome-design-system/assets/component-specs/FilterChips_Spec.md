# Component Spec: FilterChips

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **FilterChips** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The FilterChips component allows users to filter content or selections interactively. It acts as a toggle button displaying an optional icon and text. In the selected state, it typically displays a checkmark or highlighted background.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `FilterChips` | `views::MdTextButton` / Generic | `<cr-chip>` |
| **Source Files** | [Figma Link: `280:27253`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-27253) | N/A | [`ui/webui/resources/cr_elements/cr_chip/cr_chip.ts`](//src/ui/webui/resources/cr_elements/cr_chip/cr_chip.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ Classes | WebUI Element/Class |
| :--- | :--- | :--- |
| **Default (Unselected)** | Generic Button | `<cr-chip>` |
| **Selected** | Generic Button (Checked) | `<cr-chip selected>` |
| **Icon Slot** | Handled natively by button | Handled via inner HTML / icons |

---

## 3. Component States

| Interactive State | Figma | C++ | WebUI |
| :--- | :--- | :--- | :--- |
| **Default** | `state="Default"` | Standard painting | Standard layout |
| **Hovered** | `state="Hovered"` | `ButtonState::STATE_HOVERED` | `cr-ripple` or CSS pseudo-class |
| **Disabled** | `state="Disabled"` | `ButtonState::STATE_DISABLED` | `[disabled]` attribute |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Unselected Background** | Transparent | N/A | Component specific |
| **Unselected Outline** | Tonal Outline | N/A | Component specific |
| **Selected Background** | Tonal Container | N/A | Component specific |
| **Corner Radius** | `8px` | N/A | `border-radius: 8px` |

---

## 5. Architectural & Implementation Gaps

* **Component Availability**: C++ Views does not have a dedicated `FilterChip` UI primitive in `ui/views/controls`. Native implementation requires manually styling an `MdTextButton` or creating a custom view.
* **WebUI Integration**: `<cr-chip>` integrates a standard `<button>` internally with the `CrRippleMixin` for standard Material touch/click feedback.

---

## 6. Styling, Variants, Features and States Mismatches

* **Icon Swapping**: Figma defines swapping out icons for `check` when selected. The WebUI `<cr-chip>` does not enforce the icon swap natively; consumers must handle DOM updates to swap the icon based on the `selected` attribute.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Toggle Logic**: Filter chips should be used for boolean filters or multi-select arrays, not single-action navigation.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI a11y rules**: Relies on standard button focus and `aria-label`/`role` projection.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**: N/A
*   **WebUI (Web Frontend)**:
    `HTMLElement` → `LitElement` → `CrChipElementBase` (with `CrRippleMixin`) → `CrChipElement` (`<cr-chip>`)