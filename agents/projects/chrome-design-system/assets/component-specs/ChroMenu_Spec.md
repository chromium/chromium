# Component Spec: ChroMenu / Menu

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **ChroMenu (Menu)** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The ChroMenu component (and its children, MenuListItems) provides a standardized dropdown or context menu interface. It supports various item types including actionable links, toggleable checkboxes/radios, titles, submenus, and visual dividers.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Key UIs / ChroMenu` | `views::MenuItemView` | `<cr-action-menu>` |
| **Source Files** | [Figma Link: `20250:2915`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20250-2915) | [`ui/views/controls/menu/menu_item_view.h`](//src/ui/views/controls/menu/menu_item_view.h) | [`ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts`](//src/ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ `MenuItemView` Method / Enum | WebUI `<cr-action-menu>` Property |
| :--- | :--- | :--- |
| **Normal Item** | `MenuItemView::Type::kNormal` | `<button class="dropdown-item">` |
| **Divider / Separator** | `MenuItemView::Type::kSeparator` | `<hr>` element inside slot |
| **Title / Subheader** | `MenuItemView::Type::kTitle` | Custom un-clickable item |
| **Checkbox / Radio** | `MenuItemView::Type::kCheckbox` / `kRadio` | Custom markup in slot |
| **Submenu** | `MenuItemView::Type::kSubMenu` | Nested menus or programmatic opening |

---

## 3. Component States

| Interactive State | Figma | C++ `MenuItemView` | WebUI `.dropdown-item` |
| :--- | :--- | :--- | :--- |
| **Default** | `state="Default"` | Standard painting | Standard layout |
| **Hovered / Focused** | `state="Hovered"` | `SetHotTracked()` | `:focus` / `:focus-visible` (changes background) |
| **Pressed / Active** | N/A | Mouse release / AcceleratorPressed | `:active` |
| **Disabled** | N/A | `SetEnabled(false)` | `[disabled]` attribute (opacity change) |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Menu Background** | Surface Color | `ui::kColorMenuBackground` | `--cr-menu-background-color` |
| **Menu Shadow** | Elevation 2 | `views::BubbleBorder` | `--cr-menu-shadow` (Elevation 2) |
| **Item Padding** | Vertical / Horizontal | Handled by LayoutManager | `padding: 8px 24px` |
| **Item Text Color** | On-Surface | `ui::kColorMenuItemForeground` | `--cr-primary-text-color` |
| **Item Hover Color** | Hover Overlay | `ui::kColorMenuItemBackgroundSelected` | `--cr-menu-background-focus-color` |
| **Item Disabled** | Opacity | Disabled Text Color | `--cr-action-menu-disabled-item-opacity: 0.65` |

---

## 5. Architectural & Implementation Gaps

* **Component Decomposition**: C++ constructs menus programmatically via a `MenuDelegate` and adds `MenuItemView` children sequentially. WebUI `<cr-action-menu>` wraps a native `<dialog>` element and projects consumer-provided children (typically `.dropdown-item` buttons) into a `<slot>`.
* **Submenu Architecture**: In C++, submenus are deeply integrated; a `MenuItemView` can own a `SubmenuView`. In WebUI, nested menus require custom controller logic to instantiate and position a second `<cr-action-menu>` over the parent.
* **Layout Mechanics**: C++ relies on a highly specialized `views::DelegatingLayoutManager` within `MenuItemView` to align icons, text, accelerators, and submenu arrows in rigid columns. WebUI relies entirely on the host page providing flexbox layouts within the slotted `.dropdown-item`.

---

## 6. Styling, Variants, Features and States Mismatches

* **Focus vs Hover**: C++ merges "hover" and "keyboard focus" into a single state known as "hot-tracked". WebUI `<cr-action-menu>` items map standard mouse hover directly to keyboard `:focus` dynamically via Javascript (`FocusOutlineManager`) to simulate arrow key navigation, applying `background-color: var(--cr-menu-background-focus-color)`.
* **Disabled Styling**: WebUI relies on CSS opacity (`0.65`) for disabled items (along with optional color shifts). C++ explicitly paints disabled text using `ui::kColorMenuItemForegroundDisabled` without modifying overall opacity.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Concise Labels**: Keep menu item text as brief as possible.
- **Logical Grouping**: Use dividers (`kSeparator` / `<hr>`) to group related commands logically.
- **Destructive Actions**: Position destructive actions at the bottom of the menu and separate them with a divider if possible.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI a11y rules**: `<cr-action-menu>` utilizes the native `<dialog>` element which captures focus and handles backdrop modality. Arrow key navigation must be manually wired to shift focus between `.dropdown-item`s.
- **C++ Views native focus**: Managed through complex `MenuController` logic that captures event dispatch to route Up/Down keys, Escape, and Enter without taking actual OS window focus away from the anchor element.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    `views::View` → `views::MenuItemView`
*   **WebUI (Web Frontend)**:
    `HTMLElement` → `LitElement` → `CrActionMenuElement` (`<cr-action-menu>`)
