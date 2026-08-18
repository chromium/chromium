# Component Spec: ContextMenus

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **ContextMenus** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **ContextMenus** component is a floating, dropdown list containing
context-dependent action items. It supports custom list item rows, menu
dividers, key action shortcuts, toggle checkbox markers, and disabled option
styles.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                                          | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                            |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `ContextMenus`                                                                                                                      | `views::MenuItemView`                                                                    | `<cr-action-menu>`                                                                                                                       | `ListMenu` / `AnchoredPopupWindow` / `AppMenu`                                                                                                                                                                                                                                                                                                                                             |
| **Source Files**   | [Figma Link: `323:14690`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-14690) | [ui/views/controls/menu/menu_item_view.h](//src/ui/views/controls/menu/menu_item_view.h) | [ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts](//src/ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/listmenu/ListMenu.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/listmenu/ListMenu.java)<br>[ui/android/java/src/org/chromium/ui/widget/AnchoredPopupWindow.java](//src/ui/android/java/src/org/chromium/ui/widget/AnchoredPopupWindow.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant     | Figma Component            | C++ Views (Desktop)                     | WebUI (Web Frontend)             | Clank (Android)                             |
| :-------------------- | :------------------------- | :-------------------------------------- | :------------------------------- | :------------------------------------------ |
| **Context Menu**      | `Context Menu`             | Menu controller instantiation           | `<cr-action-menu>` container     | `ListMenuButton` / `AnchoredPopupWindow`    |
| **Menu Item**         | `Context Menu Row`         | `views::MenuItemView` child row         | `<button class="menu-item">`     | `ListMenuItemProperties` row in `ModelList` |
| **Menu Divider**      | `Divider`                  | `views::MenuItemView::Type::kSeparator` | `<hr class="divider">` separator | `ListMenuItemProperties.DIVIDER` row        |
| **Keyboard Shortcut** | `Ctrl+R` or `Ctrl+W` label | `MenuItemView::SetSecondaryTitle()`     | Text element nested inside rows  | N/A (mobile touchscreen context)            |

______________________________________________________________________

## 3. Component States

| State                | Figma Component           | C++ Views (Desktop)                      | WebUI (Web Frontend)              | Clank (Android)                          |
| :------------------- | :------------------------ | :--------------------------------------- | :-------------------------------- | :--------------------------------------- |
| **Default (Normal)** | `state=Default`           | Base menu item style                     | Default row style                 | Standard row appearance                  |
| **Hovered**          | `state=Hovered`           | Target row changes background color      | `.menu-item:hover` styles         | State layer overlay                      |
| **Pressed**          | `state=Pressed`           | Handled via item trigger select          | `.menu-item:active` actions       | `?attr/selectableItemBackground` ripple  |
| **Disabled**         | `State=Disabled` / Grayed | `views::MenuItemView::SetEnabled(false)` | Attribute: `[disabled]` on button | `ListMenuItemProperties.ENABLED = false` |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute        | Figma Design Token                                                     | C++ Views (Desktop)                    | WebUI (Web Frontend)                 | Clank (Android) Equivalent                             |
| :---------------------- | :--------------------------------------------------------------------- | :------------------------------------- | :----------------------------------- | :----------------------------------------------------- |
| **Menu Container BG**   | `--desktop/sys/surface-colors/surface`<br>(`white`)                    | `ui::kColorMenuBackground`             | `--cr-menu-background-color`         | `@macro/menu_bg_color` / `?attr/colorSurfaceBright`    |
| **Menu Text Color**     | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`)               | `ui::kColorMenuItemForeground`         | `--cr-menu-item-text-color`          | `@macro/default_text_color` / `?attr/colorOnSurface`   |
| **Disabled Text Color** | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`) | `ui::kColorMenuItemForegroundDisabled` | `--cr-disabled-text-color`           | `@color/default_text_color_disabled_list`              |
| **Corner Radius**       | `--desktop/corner-radius/12`<br>(`12px`)                               | Resolved by native menu styles         | `border-radius: 8px;` or `12px;`     | `@dimen/popup_bg_corner_radius_16dp` (`16dp`)          |
| **Elevation Shadow**    | `--desktop/elevation/3`<br>(Dual drop shadows)                         | Handled dynamically by native menus    | Handled via custom shadow elevations | `@dimen/menu_elevation` / `MaterialCardView` elevation |
| **Inner Side Padding**  | `--desktop/spacing/16`<br>(`16px` padding)                             | Matches list padding metric parameters | `padding-inline-start: 16px;`        | `16dp` horizontal row padding                          |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Mobile Anchoring Architecture

- **Desktop**: C++ `MenuRunner` and WebUI `<cr-action-menu>` render absolute
  coordinates positioned by mouse pointer right-click events.
- **Clank (Android)**: Menus are anchored to source Views via
  [`AnchoredPopupWindow`](//src/ui/android/java/src/org/chromium/ui/widget/AnchoredPopupWindow.java),
  automatically accounting for soft keyboards, screen bounds, and orientation
  changes.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Mac-Specific vs Mobile App Menus

- **Views on macOS**: Delegates menu rendering to native Cocoa context menus.
- **Clank (Android)**: Employs `AppMenu` with custom header icons, horizontal
  shortcut bars, and icon+text item layouts.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Option Limits**: Keep lists concise to avoid scroll bars.
- **Visual Separators**: Use Dividers strategically to group related commands.
