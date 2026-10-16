# Context Menu Spec (Desktop)

This document maps the specifications of the **Context Menu** (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

The **Context Menu** is a transient, elevated popup listing commands for the
current target, such as a bookmark or a tab. Rows can have a leading checkmark
or icon, a trailing accelerator, and a submenu chevron. Rows support hover and
disabled states, and groups are separated by full-bleed dividers. On Windows,
Linux and ChromeOS, Chrome renders it with C++ Views (`views::MenuRunner` →
`views::MenuItemView`). On macOS, top-level context menus are native `NSMenu`s.
The Figma doc frame marks the component **Views ✓ / WebUI ✗**.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | C++ Views (Desktop)                                                                                                                                                                                                                                                                                                                                                                                                                                      | WebUI (Desktop)                                                                                                                          |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Context Menu` (frame-based container; no component set)<br>`Context Menu Row` (component, boolean props)<br>`Mac Context Menu` (frame) + `Menu Item` (`Hover` variant)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | `views::MenuRunner`<br>`views::MenuItemView`<br>`views::SubmenuView`<br>`views::MenuSeparator`<br>`views::MenuConfig`                                                                                                                                                                                                                                                                                                                                    | *(Not in scope per Figma: "WebUI ✗")*<br>Closest equivalent: `<cr-action-menu>`                                                          |
| **Source Files**   | [Figma Link: `Context Menu` – Default (`323:14693`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-14693&m=dev)<br>[Figma Link: `Context Menu` – Tab Strip (`323:14718`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-14718&m=dev)<br>[Figma Link: `Context Menu Row` (`4483:24507`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=4483-24507&m=dev)<br>[Figma Link: `Mac Context Menu` (`10706:9993`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=10706-9993&m=dev)<br>[Figma Link: `Menu Item` / `Hover=False` (`10706:10048`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=10706-10048&m=dev)<br>Doc frame: [`Key UIs / Context Menus` (`323:14690`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-14690&m=dev) | [ui/views/controls/menu/menu_runner.h](//src/ui/views/controls/menu/menu_runner.h)<br>[ui/views/controls/menu/menu_item_view.h](//src/ui/views/controls/menu/menu_item_view.h)<br>[ui/views/controls/menu/submenu_view.h](//src/ui/views/controls/menu/submenu_view.h)<br>[ui/views/controls/menu/menu_separator.h](//src/ui/views/controls/menu/menu_separator.h)<br>[ui/views/controls/menu/menu_config.h](//src/ui/views/controls/menu/menu_config.h) | [ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts](//src/ui/webui/resources/cr_elements/cr_action_menu/cr_action_menu.ts) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- **`Context Menu Row`** (component, no variants): `LHS Icon?` (boolean, default
  `true`) × `RHS?` (boolean, default `true`) × `Hovered?` (boolean, default
  `false`). There is also an instance-swap icon in the LHS (`check`) and the RHS
  (`chevron_right` / `keyboard_arrow_right`), and the label and the RHS shortcut
  text can be overridden.
- **`Context Menu`** (frame): no properties. Two examples, `DEFAULT` (bookmark
  menu) and `TAB STRIP`, built from `Context Menu Row` instances and `Divider`
  frames.
- **`Menu Item`** (Mac): `Hover` (False; the MCP server returned only this
  value) × `Label` (text).
- **Disabled** is not a property. It is shown only by overriding the label color
  to `state-disabled` on instances (Rename, Cut, Copy, Paste, Delete).

| Feature / Variant                                       | Figma Component                                                  | C++ Views (Desktop)                                                                                                    | WebUI (Desktop)                                                        |
| :------------------------------------------------------ | :--------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------- |
| **Default row**                                         | `Context Menu Row` (`LHS Icon?=false`, `RHS?=false`)             | `MenuItemView::Type::kNormal` (`MenuItemView::AppendMenuItem()` / `ui::SimpleMenuModel::AddItem()`)                    | `<button class="dropdown-item">`                                       |
| **Hovered row**                                         | `Hovered?=true` (`Hover` rect layer)                             | Selected item (`IsSelected()`), painted with `kColorMenuItemBackgroundSelected` via `NativeTheme::kMenuItemBackground` | `.dropdown-item:hover` / `:focus` → `--cr-menu-background-focus-color` |
| **Disabled row**                                        | Instance text color override → `state-disabled`                  | `MenuItemView::SetEnabled(false)` / `MenuModel::IsEnabledAt()` → `style::STYLE_DISABLED`                               | `.dropdown-item[disabled]` (opacity `0.65`)                            |
| **Checked row (leading check)**                         | `LHS Icon?=true` with `check` icon                               | `MenuItemView::Type::kCheckbox` + `MenuDelegate::IsItemChecked()` (`kCheckIcon`)                                       | *(No built-in; manual)*                                                |
| **Leading icon**                                        | `LHS Icon?=true` with an icon swap (e.g. "Send to your devices") | `MenuItemView::SetIcon()` / `MenuModel::GetIconAt()`                                                                   | *(Manual slotted content)*                                             |
| **Accelerator / shortcut**                              | `RHS?=true`, RHS `Text` (e.g. `Ctrl+R`)                          | Minor text from `MenuDelegate::GetAccelerator()` (`MenuConfig::ShouldShowAcceleratorText()`), or `SetMinorText()`      | *(Not supported)*                                                      |
| **Submenu**                                             | `RHS?=true`, RHS icon `keyboard_arrow_right`                     | `MenuItemView::Type::kSubMenu` (`submenu_arrow_image_view_`, `kKeyboardArrowRightFlippableIcon`)                       | *(Not supported; nest a second `<cr-action-menu>` by hand)*            |
| **Divider**                                             | `Divider` frame (`16px` + 1px line)                              | `MenuItemView::Type::kSeparator` → `views::MenuSeparator` (`ui::NORMAL_SEPARATOR`)                                     | `<hr>` / `--cr-hairline`                                               |
| **Radio row**                                           | *(Not modeled in Figma)*                                         | `MenuItemView::Type::kRadio`                                                                                           | —                                                                      |
| **Title / Highlighted (footnote) / Actionable submenu** | *(Not modeled in Figma)*                                         | `Type::kTitle`, `Type::kHighlighted`, `Type::kActionableSubMenu`                                                       | —                                                                      |
| **Secondary title / New badge / Alerted**               | *(Not modeled in Figma)*                                         | `SetSecondaryTitle()`, `SetNewBadgeType()`, `SetAlerted()`                                                             | —                                                                      |
| **Focused (keyboard) row**                              | *(Not modeled in Figma)*                                         | Same as selected/hover (keyboard navigation moves selection)                                                           | `:focus` → same as hover                                               |
| **macOS context menu**                                  | `Mac Context Menu` + `Menu Item`                                 | `MenuRunnerImplMac` → `MenuRunnerImplCocoa` (native `NSMenu`) for `MenuRunner::CONTEXT_MENU` without `IS_NESTED`       | —                                                                      |
| **Dark theme**                                          | *(Not modeled in Figma)*                                         | Resolved dynamically via `ui::ColorProvider`                                                                           | `--color-menu-*` variables                                             |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

Values verified in code are marked ✓ (match) or ⚠ (mismatch). Values marked
*(unverified)* were not checked at runtime.

| Design Attribute                                 | Figma Design Token                                                                                                                                                                        | C++ Views (Desktop)                                                                                                                                                                                                                                                        | WebUI (Desktop, reference only)                                                                   |
| :----------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------ |
| **Menu Container Background**                    | `--desktop/sys/surface-colors/surface` (`#ffffff`)                                                                                                                                        | `ui::kColorMenuBackground` → `kColorPrimaryBackground` → `kColorSysSurface` ✓                                                                                                                                                                                              | `--cr-menu-background-color` → `--color-menu-background` (fallback `--cr-fallback-color-surface`) |
| **Menu Corner Radius**                           | `--desktop/corner-radius/12` (`12px`)                                                                                                                                                     | `MenuConfig::CornerRadiusForMenu()` → `ShapeContextTokens::kMenuAuxRadius` (context menus) / `kMenuRadius` → `ShapeSysTokens::kMediumSmall` = `12` ✓ (`0` on Ozone without compositing)                                                                                    | `--cr-menu-border-radius` (default `4px`) ⚠                                                       |
| **Menu Elevation / Shadow**                      | `desktop/elevation/3`: key `0 4px 8px 3px` @15%, ambient `0 1px 3px 0` @30%                                                                                                               | `BubbleBorder` (`STANDARD_SHADOW`) with `MenuConfig::bubble_menu_shadow_elevation = 12` (submenus `16`). Exact blur/offset *(unverified)*                                                                                                                                  | `--cr-menu-shadow` → `--cr-elevation-2` ⚠                                                         |
| **Menu Vertical Padding**                        | `--desktop/spacing/12` (`12px`) top/bottom                                                                                                                                                | `rounded_menu_vertical_border_size` unset → falls back to corner radius `12` ✓                                                                                                                                                                                             | `padding: 0` ⚠                                                                                    |
| **Menu Width**                                   | Fixed `320px` in the examples                                                                                                                                                             | Content-driven (no minimum width for non-touch menus) ⚠                                                                                                                                                                                                                    | `min-width: 128px`                                                                                |
| **Menu Border Stroke**                           | *(None)*                                                                                                                                                                                  | `MenuConfig::use_outer_border = false` ✓ (`kColorMenuBorder` only on non-bubble paths)                                                                                                                                                                                     | `--cr-border-hcm` in forced colors only                                                           |
| **Row Label Color**                              | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                                                                                                                                     | `ui::kColorMenuItemForeground` → `kColorSysOnSurface` ✓                                                                                                                                                                                                                    | `--cr-primary-text-color` → `--color-menu-item-foreground`                                        |
| **Row Label Color (Hovered)**                    | `on-surface` (unchanged)                                                                                                                                                                  | `kColorMenuItemForegroundSelected` → `kColorMenuItemForeground` ✓                                                                                                                                                                                                          | unchanged                                                                                         |
| **Row Label Color (Disabled)**                   | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`, on-surface @38%)                                                                                                                | `kColorMenuItemForegroundDisabled` → `kColorDisabledForeground` → `PickGoogleColor(kGoogleGrey600, kColorPrimaryBackground, …)` ⚠                                                                                                                                          | `--color-menu-item-foreground-disabled` + `opacity: 0.65` ⚠                                       |
| **Accelerator / Shortcut Text**                  | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)                                                                                                                              | `ui::kColorMenuItemForegroundSecondary` → `kColorSysOnSurfaceSubtle` ✓                                                                                                                                                                                                     | —                                                                                                 |
| **Leading Check / Trailing Chevron Color**       | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)                                                                                                                              | `color_utils::DeriveDefaultIconColor(fg_color)` = `BlendTowardMaxContrast(on-surface, 0x4c)` (≈`#626262`, computed from the code, not observed) ⚠. `kColorMenuIcon` (→ `kColorSysOnSurfaceSubtle`) exists but isn't used for these icons                                   | —                                                                                                 |
| **Hover Background**                             | `--desktop/sys/state-colors/state-hover-on-subtle` (`#1f1f1f0f`, 6%)                                                                                                                      | `kColorMenuItemBackgroundSelected` → `kColorMenuSelectionBackground` = `GetResultingPaintColor(kColorSysStateHoverOnSubtle, kColorMenuBackground)` ✓ (pre-blended to opaque)                                                                                               | `--cr-hover-background-color` (→ `--color-sys-state-hover`) ⚠                                     |
| **Hover Shape**                                  | Full-bleed `320×28` rect, no radius                                                                                                                                                       | `MenuConfig::item_corner_radius = 0` ✓                                                                                                                                                                                                                                     | full-bleed                                                                                        |
| **Divider Color**                                | Hardcoded stroke `#D3E3FD` (no variable bound)                                                                                                                                            | `kColorMenuSeparator` → `kColorSeparator` → `kColorSysDivider` (light: `kColorRefPrimary90` = `#D3E3FD`) ✓ value. Figma should bind a token                                                                                                                                | `--cr-hairline` → `--color-menu-separator` (fallback `--cr-fallback-color-divider`)               |
| **Divider Height / Thickness**                   | `16px` frame (`8px` top/bottom padding) + `1px` line                                                                                                                                      | `MenuConfig::separator_height = 17` (`InitCommon`), `separator_thickness = 1` ✓ (≈)                                                                                                                                                                                        | `1px` hairline                                                                                    |
| **Divider Horizontal Inset**                     | `0` (full width)                                                                                                                                                                          | `separator_horizontal_border_padding = 0` ✓                                                                                                                                                                                                                                | —                                                                                                 |
| **Label Typography**                             | `desktop/body/three`: `--desktop/font/body` (Google Sans Text), `--desktop/font_size/body-three` `13px`, `--desktop/font_weight/regular` `400`, `--desktop/line_height/body-three` `20px` | `MenuConfig::font_list` = `TypographyProvider::GetFont(style::CONTEXT_MENU, style::STYLE_BODY_3)` (`13px`, `NORMAL`) ✓. Line height is not applied; rows use `font_list.GetHeight()`. **Windows** overrides it with the system menu font (`gfx::win::SystemFont::kMenu`) ⚠ | `font: inherit`                                                                                   |
| **Row Height**                                   | `28px` (`4px` + `20px` + `4px`)                                                                                                                                                           | `max(font height, min_icon_height 16) + 2 × item_vertical_margin (6)`. That's about `28–29px`, depending on the font's ascent and descent *(unverified at runtime)*                                                                                                        | `min-height: 32px` ⚠                                                                              |
| **Row Vertical Padding**                         | `--desktop/spacing/4` (`4px`) around a `20px` line box                                                                                                                                    | `MenuConfig::item_vertical_margin = 6` around the font height (≈`16px`)                                                                                                                                                                                                    | `padding: 8px 24px`                                                                               |
| **Row Leading Padding**                          | `--desktop/spacing/16` (`16px`)                                                                                                                                                           | `item_horizontal_border_padding (12) + item_horizontal_padding (8)` = `20` ⚠                                                                                                                                                                                               | `24px` ⚠                                                                                          |
| **Row Trailing Padding (to shortcut / chevron)** | `--desktop/spacing/16` (`16px`)                                                                                                                                                           | Minor text: `trailing_padding` = `12 + 8` = `20`. Chevron: `12 + arrow_to_edge_padding (8)` = `20` ⚠                                                                                                                                                                       | `24px` ⚠                                                                                          |
| **Leading Icon Size**                            | `16px`                                                                                                                                                                                    | `kMenuCheckSize = 16` ✓                                                                                                                                                                                                                                                    | —                                                                                                 |
| **Leading Icon ↔ Label Gap**                     | `--desktop/spacing/8` (`8px`)                                                                                                                                                             | `MenuConfig::icon_label_spacing` = `DISTANCE_RELATED_LABEL_HORIZONTAL` = `12` ⚠                                                                                                                                                                                            | —                                                                                                 |
| **Shortcut ↔ Chevron Gap**                       | `--desktop/spacing/8` (`8px`)                                                                                                                                                             | `item_horizontal_padding = 8` ✓                                                                                                                                                                                                                                            | —                                                                                                 |
| **Chevron Size**                                 | `16px` (`keyboard_arrow_right`)                                                                                                                                                           | `MenuConfig::arrow_size = 16` (`InitCommon`) ✓                                                                                                                                                                                                                             | —                                                                                                 |
| **Mac Menu Container**                           | `surface`, `--desktop/corner-radius/6`, `--desktop/spacing/5` padding, `backdrop-blur 40px`, custom drop shadow                                                                           | Native `NSMenu` (system-rendered)                                                                                                                                                                                                                                          | —                                                                                                 |
| **Mac Menu Item**                                | SF Pro Text Medium `13/16`, `rgba(0,0,0,0.85)`, padding `3px 12px 3px 7px`, radius `6px` (all hardcoded)                                                                                  | Native `NSMenuItem`                                                                                                                                                                                                                                                        | —                                                                                                 |
| **Mac Separator**                                | `rgba(0,0,0,0.1)` 1px, inset `7px` (hardcoded)                                                                                                                                            | Native `NSMenuItem.separatorItem`                                                                                                                                                                                                                                          | —                                                                                                 |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Container is not a Figma component

`Context Menu` is a plain frame that stacks `Context Menu Row` instances and
`Divider` frames. The divider isn't a component either. In code the container is
`SubmenuView` inside `MenuScrollViewContainer`, which adds the bubble border,
shadow and padding. Container-level specs (radius, padding, elevation) are only
captured on the example frames.

### 2. Horizontal metrics differ from Figma

Views uses `item_horizontal_border_padding (12) + item_horizontal_padding (8)` =
**20dp** leading and trailing insets, and a **12dp** icon-to-label gap
(`DISTANCE_RELATED_LABEL_HORIZONTAL`). Figma specifies **16px** insets and an
**8px** gap. With a leading check, labels start at **48dp** in code and at
**40px** in Figma.

### 3. Row height is font-driven, not line-height-driven

Figma uses a `20px` line box with `4px` vertical padding. Views uses
`font_list.GetHeight()` (no line height) plus `item_vertical_margin = 6`, so the
row height depends on the platform font metrics. On Windows,
`MenuConfig::InitPlatform()` swaps in the OS menu font, so typography and row
height don't follow `body-three` there.

### 4. Disabled color

Figma uses `state-disabled` (on-surface @38%). Views maps
`kColorMenuItemForegroundDisabled` to the legacy `kColorDisabledForeground`
(`PickGoogleColor(kGoogleGrey600, …)`), not `kColorSysStateDisabled`. The
material mixer doesn't override it, but it does map `kColorMenuIconDisabled` →
`kColorSysStateDisabled`. Leading icons and chevrons on disabled rows take their
color from the text color, not from Figma.

### 5. Icon color is derived, not tokenized

Figma binds the check and chevron to `on-surface-subtle`. `MenuItemView`
computes `DeriveDefaultIconColor(fg_color)` (30% blend toward max contrast).
That's close to, but not the same as, `kColorSysOnSurfaceSubtle`.

### 6. Elevation scale mismatch

Figma uses `desktop/elevation/3`. Views passes
`bubble_menu_shadow_elevation = 12` (`16` for submenus) to `BubbleBorder`, which
uses a different elevation scale. The resulting shadow geometry wasn't compared
pixel-for-pixel.

### 7. Platform split

- **Windows**: `use_bubble_border = corner_radius > 0` uses the system menu font
  and reads the menu show delay from the OS. In high-contrast mode,
  `native_color_mixers_win.cc` remaps all menu colors.
- **Linux**: `use_bubble_border = true`. GTK themes can remap menu colors
  (`native_chrome_color_mixer_linux.cc`).
- **macOS**: top-level context menus are native `NSMenu`s (`MenuRunnerImplMac`),
  matching the separate `Mac Context Menu` frame. Accelerators are hidden in
  context menus (`show_context_menu_accelerators = false`). Nested menus
  (`IS_NESTED`) still use Views. The Mac frame uses hardcoded values (SF Pro,
  `rgba` colors), not design tokens.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing states and variants in Figma

- No `Disabled` property: disabled rows are local color overrides on instances.
- No focused or keyboard-selected state. In Views, keyboard selection uses the
  same painting as hover.
- Not modeled: radio rows, title rows, highlighted footnote rows, actionable
  submenus (split row with a `VerticalSeparator`), secondary titles, "New"
  badges, alerted rows, minor icons, touchable menus, and dark theme.

### 2. Leading slot reservation

Figma always keeps a `16px` `Icon` frame, even when `LHS Icon?=false`, so every
label is indented. Views reserves the check or icon column only if at least one
item in the submenu is a checkbox, a radio, or has an icon
(`always_reserve_check_region = false`). Otherwise labels sit flush at the
content start.

### 3. `Spacer` element

Every row has a `1×11px` `Spacer` between LHS and RHS. It is not visible in the
examples. It may represent the actionable-submenu `VerticalSeparator` (`1×18dp`
in `MenuConfig`), or it may be a layout helper. Its purpose is unconfirmed.

### 4. Submenu chevron glyph

The Figma main component uses `chevron_right`, while the instances use
`keyboard_arrow_right`. Code uses
`vector_icons::kKeyboardArrowRightFlippableIcon` when
`features::IsRoundedIconsEnabled()`, otherwise
`kSubmenuArrowChromeRefreshOldIcon`. The icon flips in RTL.

### 5. WebUI is out of scope but diverges

The Figma doc frame marks WebUI ✗. `<cr-action-menu>` uses a `4px` radius,
`--cr-elevation-2`, `32px` rows with `8px 24px` padding, and opacity-based
disabled styling. It has no accelerator, submenu or check support. Treat it as a
separate component (a dropdown/action menu), not as an implementation of this
spec.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use context menus for secondary commands on a specific target (a tab, a
  bookmark, a link). Don't use them for primary actions.
- Group related commands with dividers. Keep the most frequent or safest
  commands at the top and destructive commands (Delete, Close) in their own
  group.
- Use short, sentence-case verb phrases ("Add tab to group", "Close other
  tabs"). End a label with an ellipsis only if the command needs more input.
- Disable unavailable commands rather than hiding them when their position helps
  users learn the menu (e.g. Cut/Copy/Paste).
- Use a leading check only for toggles (e.g. "Show bookmarks bar"). Use a
  submenu chevron only for items that open a nested menu. Don't combine the two
  in one row.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **C++ Views**: build menus from `ui::SimpleMenuModel` (via
  `views::MenuModelAdapter`) or a `views::MenuDelegate`, and show them with
  `views::MenuRunner` using `MenuRunner::CONTEXT_MENU`. Items expose
  `ax::mojom::Role::kMenuItem`, `kMenuItemCheckBox` or `kMenuItemRadio`.
  Separators expose `kMenuItemSeparator`. The accessible name includes the minor
  text (accelerator). Arrow keys move the selection (wrapping, except on Mac),
  Right/Left open and close submenus, Enter/Space activate, Esc closes, and
  mnemonics work on Windows and Linux. Provide accelerators through
  `MenuDelegate::GetAccelerator()` rather than hardcoding shortcut text.
- **macOS**: context menus use native `NSMenu`, which provides VoiceOver and
  keyboard behavior. Views styling and tokens don't apply.
- **WebUI** (if a menu is needed): `<cr-action-menu>` sets `role="menu"` on the
  dialog. Slotted `.dropdown-item` elements need `role="menuitem"`, and arrow
  key navigation is built in.

### 3. Icon Usage Guidelines

- Leading icons are `16px`, using Google Symbols (rounded, FILL 0). Use them
  sparingly (e.g. "Send to your devices") and keep them consistent within a
  group.
- The checkmark (`check` / `kCheckIcon`) and submenu chevron
  (`keyboard_arrow_right` / `kKeyboardArrowRightFlippableIcon`) are built into
  `MenuItemView`. Don't add them as custom icons.
