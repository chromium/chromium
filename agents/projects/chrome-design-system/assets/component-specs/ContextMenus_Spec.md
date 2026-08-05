# Component Spec: ContextMenus

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **ContextMenus** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **ContextMenus** component is a floating, dropdown list containing context-dependent action items. It supports custom list item rows, menu dividers, key action shortcuts, toggle checkbox markers, and disabled option styles.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `ContextMenus` | `views::MenuItemView` | `<cr-action-menu>` |
| **Source Files** | [Figma Link: `323:14690`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-14690&m=dev) | [ui/views/controls/menu/menu_item_view.h](//src/ui/views/controls/menu/menu_item_view.h) | [ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts](//src/ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Context Menu** | `Context Menu` | Menu controller instantiation | `<cr-action-menu>` container |
| **Menu Item** | `Context Menu Row` | `views::MenuItemView` child row | `<button class="menu-item">` |
| **Menu Divider** | `Divider` | `views::MenuItemView::Type::kSeparator` | `<hr class="divider">` separator |
| **Keyboard Shortcut**| `Ctrl+R` or `Ctrl+W` label | `MenuItemView::SetSecondaryTitle()` | Text element nested inside LHS/RHS rows |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | Base menu item style | Default row style |
| **Hovered** | `state=Hovered` | Target row changes background color | `.menu-item:hover` styles |
| **Pressed** | `state=Pressed` | Handled via item trigger select | `.menu-item:active` actions |
| **Disabled** | `State=Disabled` / Grayed | `views::MenuItemView::SetEnabled(false)` | Attribute: `[disabled]` on button row |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Menu Container BG** | `--desktop/sys/surface-colors/surface`<br>(`white`) | `ui::kColorMenuBackground` | `--cr-menu-background-color` |
| **Menu Text Color** | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`) | `ui::kColorMenuItemForeground` | `--cr-menu-item-text-color` |
| **Disabled Text Color** | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`) | `ui::kColorMenuItemForegroundDisabled` | `--cr-disabled-text-color` |
| **Corner Radius** | `--desktop/corner-radius/12`<br>(`12px`) | Resolved by native menu styles | `border-radius: 8px;` or `12px;` |
| **Elevation Shadow** | `--desktop/elevation/3`<br>(Dual drop shadows) | Handled dynamically by native menus | Handled via custom shadow elevations |
| **Inner Side Padding** | `--desktop/spacing/16`<br>(`16px` padding) | Matches list padding metric parameters | `padding-inline-start: 16px;` |

---

## 5. Architectural & Implementation Gaps

### 1. Submenu Arrow and Expanding Layers
*   **Figma**: Built as simple, static side icons.
*   **C++ Views**: Submenu items dynamically trigger secondary popup menu instances (`views::MenuRunner`) positioned to cascade correctly beside the parent item.
*   **WebUI**: Managed via nested elements or dynamic template insertions that render conditionally.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Mac-Specific Context Menus (OS Native vs Custom)
*   **Figma**: Separately designs a Mac Context Menu with specific transparency effects.
*   **C++ Views**: On macOS, Chrome often delegates menu rendering entirely to native Cocoa context menus to preserve platform styling, entirely bypassing custom paint layers.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Option Limits**: Keep lists concise to avoid scroll bars.
*   **Visual Separators**: Use Dividers strategically to group related commands (e.g. edit actions separately from file actions).

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::MenuItemView (Handles items, separators, and submenu indicators)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── LitElement / CrLitElement (Web UI host)
              └── CrActionMenuElement (Reusable cr-action-menu component)
    ```
