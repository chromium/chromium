# Component Spec: Comboboxes

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Comboboxes** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

Comboboxes allow users to select an option from a dropdown menu. They come in two primary flavors: a standard dropdown (select) and an editable combobox allowing users to type a custom value or filter choices.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Comboboxes` | `views::Combobox` | `<select class="md-select">` |
| **Source Files** | [Figma Link: `280:26332`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26332) | [`ui/views/controls/combobox/combobox.h`](//src/ui/views/controls/combobox/combobox.h) | `ui/webui/resources/cr_elements/md_select_lit.css` |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ Classes | WebUI Element/Class |
| :--- | :--- | :--- |
| **Default** | `views::Combobox` | `<select class="md-select">` |
| **Editable Combobox** | `views::EditableCombobox` | Custom input/dropdown pairs |

---

## 3. Component States

| Interactive State | Figma | C++ `views::Combobox` | WebUI `<select>` |
| :--- | :--- | :--- | :--- |
| **Default** | `state="Default"` | Normal drawing | Normal layout |
| **Hovered** | `state="Hovered"` | InkDrop / Hover state | `:hover` |
| **Pressed** | `state="Pressed"` | `IsMenuRunning()` | Active / Dropdown open |
| **Disabled** | `state="Disabled"` | `SetEnabled(false)` | `[disabled]` attribute |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Corner Radius** | `8px` | Layout Provider | `border-radius: 8px` / `4px` |
| **Padding** | `10px` | `GetInsets()` | `padding` based on `md-select` |
| **Border Color** | Neutral Outline | `ui::kColorComboboxBackground` | `border-color` |
| **Disabled Background** | State Disabled Container | Inherits from system | `--md-select-disabled-background` |

---

## 5. Architectural & Implementation Gaps

* **C++ Menu Integration**: `views::Combobox` ties directly into `ui::ComboboxModel` and spins up a native menu (via `views::MenuRunner`). `views::EditableCombobox` is heavily specialized for text input combined with suggestions.
* **WebUI**: A native `<select>` element styled with `.md-select` is most common, avoiding complex DOM/JS dropdowns unless rich contents (like icons per item) are needed, in which case `<cr-action-menu>` or custom components are used.

---

## 6. Styling, Variants, Features and States Mismatches

* **Arrow Customization**: The dropdown arrow in WebUI is often customized using background SVG masks (e.g., `images/select.png`), while C++ draws it via vector icons.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **When to use**: Use when there are more than 4-5 options. For fewer options, consider radio buttons or segmented controls.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **Keyboard**: Standard up/down arrow navigation. C++ manages prefix selection delegates.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    `views::View` → `views::Combobox`
*   **WebUI (Web Frontend)**:
    Native `HTMLSelectElement` styled by `md_select_lit.css`