# Component Spec: ChroMenu / Menu

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **ChroMenu (Menu)** component across
Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The ChroMenu component (and its children, MenuListItems) provides a standardized
dropdown or context menu interface. It supports various item types including
actionable links, toggleable checkboxes/radios, titles, submenus, and visual
dividers.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                       | C++ Views (Desktop)                                                                        | WebUI (Desktop)                                                                                                                            | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                  |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Key UIs / ChroMenu`                                                                                                                  | `views::MenuItemView`                                                                      | `<cr-action-menu>`                                                                                                                         | `AppMenu` / `ListMenu` / `AnchoredPopupWindow`                                                                                                                                                                                                                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `20250:2915`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20250-2915) | [`ui/views/controls/menu/menu_item_view.h`](//src/ui/views/controls/menu/menu_item_view.h) | [`ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts`](//src/ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts) | [`chrome/android/java/src/org/chromium/chrome/browser/app/appmenu/AppMenu.java`](//src/chrome/android/java/src/org/chromium/chrome/browser/app/appmenu/AppMenu.java)<br>[`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/listmenu/ListMenu.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/listmenu/ListMenu.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant           | C++ `MenuItemView` Enum                    | WebUI `<cr-action-menu>` Property | Clank (Android) Implementation               |
| :---------------------- | :----------------------------------------- | :-------------------------------- | :------------------------------------------- |
| **Normal Item**         | `MenuItemView::Type::kNormal`              | `<button class="dropdown-item">`  | `ListMenuItemProperties` / Standard menu row |
| **Divider / Separator** | `MenuItemView::Type::kSeparator`           | `<hr>` element inside slot        | `ListMenuItemProperties.DIVIDER`             |
| **Title / Subheader**   | `MenuItemView::Type::kTitle`               | Custom un-clickable item          | `ListMenuItemProperties.SUBHEADER`           |
| **Checkbox / Radio**    | `MenuItemView::Type::kCheckbox` / `kRadio` | Custom markup in slot             | Checkable menu item                          |
| **Submenu**             | `MenuItemView::Type::kSubMenu`             | Programmatic menu trigger         | Nested `ListMenu`                            |

______________________________________________________________________

## 3. Component States

| Interactive State     | Figma             | C++ `MenuItemView`                 | WebUI `.dropdown-item`      | Clank (Android)                  |
| :-------------------- | :---------------- | :--------------------------------- | :-------------------------- | :------------------------------- |
| **Default**           | `state="Default"` | Standard painting                  | Standard layout             | Normal row                       |
| **Hovered / Focused** | `state="Hovered"` | `SetHotTracked()`                  | `:focus` / `:focus-visible` | Hover overlay                    |
| **Pressed / Active**  | N/A               | Mouse release / AcceleratorPressed | `:active`                   | `?attr/selectableItemBackground` |
| **Disabled**          | N/A               | `SetEnabled(false)`                | `[disabled]` attribute      | `android:enabled="false"`        |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type           | Figma Property        | C++ Views                              | WebUI Variable / CSS                           | Clank (Android) Equivalent                           |
| :------------------- | :-------------------- | :------------------------------------- | :--------------------------------------------- | :--------------------------------------------------- |
| **Menu Background**  | Surface Color         | `ui::kColorMenuBackground`             | `--cr-menu-background-color`                   | `@macro/menu_bg_color` / `?attr/colorSurfaceBright`  |
| **Menu Shadow**      | Elevation 2           | `views::BubbleBorder`                  | `--cr-menu-shadow` (Elevation 2)               | `@dimen/menu_elevation`                              |
| **Item Padding**     | Vertical / Horizontal | Handled by LayoutManager               | `padding: 8px 24px`                            | `paddingStart="16dp"`, `paddingEnd="16dp"`           |
| **Item Text Color**  | On-Surface            | `ui::kColorMenuItemForeground`         | `--cr-primary-text-color`                      | `@macro/default_text_color` / `?attr/colorOnSurface` |
| **Item Hover Color** | Hover Overlay         | `ui::kColorMenuItemBackgroundSelected` | `--cr-menu-background-focus-color`             | State layer overlay                                  |
| **Item Disabled**    | Opacity               | Disabled Text Color                    | `--cr-action-menu-disabled-item-opacity: 0.65` | `@color/default_text_color_disabled_list`            |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Component Decomposition**: C++ constructs menus programmatically via
  `MenuDelegate`. WebUI `<cr-action-menu>` wraps `<dialog>` with slotted items.
  Clank uses `AppMenuCoordinator` with `PropertyModel` list adapters.
- **Layout Mechanics**: Views uses `views::DelegatingLayoutManager`. WebUI uses
  flexbox. Clank uses `ListView` / `RecyclerView` within an
  `AnchoredPopupWindow`.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Focus vs Hover**: C++ merges hover/focus into "hot-tracked". WebUI simulates
  focus outlines. Clank supports Android state layer animations and ripple
  feedback.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Concise Labels**: Keep menu item text as brief as possible.
- **Logical Grouping**: Use dividers to group related commands logically.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Capture focus inside dialog backdrop.
- **C++ Views**: Managed via `MenuController`.
- **Clank (Android)**: Announce item positions to TalkBack and support back
  button dismissals.
