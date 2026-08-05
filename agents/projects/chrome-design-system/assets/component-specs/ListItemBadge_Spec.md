# Component Spec: ListItemBadge

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **ListItemBadge** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The ListItemBadge component acts as a supplementary metadata pill, typically docked within lists or rows. It displays text, icons, and occasionally crossed-out original values (e.g., for "Updated Status").

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `ListItemBadge` | Custom / `views::Badge` | Custom CSS / `cr-badge` |
| **Source Files** | [Figma Link: `280:27248`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-27248) | [`ui/views/controls/badge.h`](//src/ui/views/controls/badge.h) | N/A |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ Classes | WebUI Element/Class |
| :--- | :--- | :--- |
| **Default** | Generic View | Custom HTML/CSS |
| **Updated Status** | Custom layout | Custom HTML/CSS |

---

## 3. Component States

This component is strictly informational and does not feature interactive states (Hovered, Pressed) natively.

| Interactive State | Figma | C++ | WebUI |
| :--- | :--- | :--- | :--- |
| **Default** | N/A | Normal painting | Normal layout |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Default Background** | Neutral Container | Background | CSS classes |
| **Updated Background** | Tertiary Container | N/A | CSS classes |
| **Corner Radius** | Fully Rounded (999px) | Canvas rounding | `border-radius: 999px` |
| **Typography** | Body Five (11px) | N/A | Font sizes |

---

## 5. Architectural & Implementation Gaps

* **No direct UI primitive**: WebUI and C++ Views do not offer a generalized `ListItemBadge` with native crossed-out status layouts. Consumers must build this using flexbox (`<div class="badge">`) in WebUI or a composite `views::View` with multiple `views::Label`s in C++.
* **views::Badge**: The standard C++ `views::Badge` (`ui/views/controls/badge.h`) is heavily restricted to displaying text on a blue background, which does not cover the visual diversity requested in Figma (like "Updated Status" with tertiary green colors and icons).

---

## 6. Styling, Variants, Features and States Mismatches

* **Strikethrough styling**: The "Updated Status" variant requires explicit CSS (`text-decoration: line-through`) or `views::Label` text styles to render correctly, which must be hand-assembled by developers.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Context**: Use within complex list items to highlight changes or small tags.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**: N/A
*   **WebUI (Web Frontend)**: N/A