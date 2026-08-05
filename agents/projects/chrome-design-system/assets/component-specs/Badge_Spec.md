# Component Spec: Badge

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Badge** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The Badge component is a highly compact, informational text label (commonly rendering text like "NEW"). It is placed alongside other components to highlight updates or new features.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Badge` | `views::Badge` | Custom CSS / `new-badge` |
| **Source Files** | [Figma Link: `280:26503`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26503) | [`ui/views/controls/badge.h`](//src/ui/views/controls/badge.h) | `ui/webui/resources/cr_components/help_bubble/new_badge.ts` |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ `views::Badge` Method | WebUI Element/Class |
| :--- | :--- | :--- |
| **Default (Tonal Container)** | Not natively supported | `.new-badge` or custom |
| **Cocoa Menu (Primary)** | `views::Badge` standard behavior | `<new-badge>` / Custom |

---

## 3. Component States

This component is non-interactive.

| Interactive State | Figma | C++ `views::Badge` | WebUI |
| :--- | :--- | :--- | :--- |
| **Default** | N/A | Normal painting | Normal layout |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Typography** | Special Label (9px, Bold) | Custom font list | `font-weight: bold; font-size: 9px` |
| **Padding** | `4px` | Native layout | `padding: 4px` |
| **Corner Radius** | `4px` | Native drawing | `border-radius: 4px` |
| **Default Color** | Tonal Container | N/A | CSS specific |
| **Cocoa Color** | Primary | Hardcoded blue | CSS specific |

---

## 5. Architectural & Implementation Gaps

* **Theme Toggles**: The `views::Badge` in C++ hardcodes its rendering to a blue background. Custom colors (like the Tonal Container variant seen in Figma) require overriding or creating a new View.
* **WebUI `<new-badge>`**: Some WebUI systems use specialized components like `<new-badge>` (from user education frameworks) instead of a universal CSS primitive.

---

## 6. Styling, Variants, Features and States Mismatches

* **Typography size**: A 9px font size is extremely small and may be scaled up automatically by OS-level text scaling settings, deviating from pure Figma mockups.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Brevity**: Badges should contain 1-3 characters maximum (e.g., "NEW", "PRO").

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    `views::View` → `views::Badge`
*   **WebUI (Web Frontend)**: N/A