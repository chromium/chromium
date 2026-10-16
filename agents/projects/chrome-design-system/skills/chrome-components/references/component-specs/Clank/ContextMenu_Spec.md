# Context Menu Spec (Clank)

This document maps the specifications of the **Context Menu** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Context Menu** is the long-press menu shown for web content (links,
images, videos). Figma models a single fixed example: an image context menu with
a header (60px image preview + image name), a full-width divider, and six
single-line text menu items, on a 24px-radius `surfaceBright` popup. In code,
`ContextMenuCoordinator` builds a `ModelList` of a `HEADER` item, section
dividers and `context_menu_row` items, renders it in a `ContextMenuListView`
inside `context_menu.xml`, and shows it in a `ContextMenuDialog` (full-screen
scrim dialog on phones, anchored popup on tablets).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                         | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Context-menu/Context Menu`<br>`.Menu-items` (nested row)                                                                                                                                                                                                                                                                               | `ContextMenuCoordinator`<br>`ContextMenuListView`<br>`ContextMenuDialog`<br>`ContextMenuHeaderCoordinator`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Source Files**   | [Figma Link: `Context-menu/Context Menu` (`7111:768`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7111-768&m=dev)<br>[Figma Link: `.Menu-items` instance (`7111:773`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7111-773&m=dev) | [chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuCoordinator.java](//src/chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuCoordinator.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuListView.java](//src/chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuListView.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ContextMenuDialog.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ContextMenuDialog.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuHeaderCoordinator.java](//src/chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuHeaderCoordinator.java) |

> Provided link: instance `43301:80461` of `Context-menu/Context Menu` (main
> component `7111:768`, a standalone `COMPONENT`, not a component set). The
> `.Menu-items` row is a hidden (`.`-prefixed) nested component; its main
> component node could not be resolved via the MCP, so the instance is linked.
>
> Related layouts / resources:
> [context_menu.xml](//src/chrome/android/java/res/layout/context_menu.xml),
> [context_menu_header.xml](//src/chrome/android/java/res/layout/context_menu_header.xml),
> [context_menu_header_text.xml](//src/chrome/android/java/res/layout/context_menu_header_text.xml),
> [context_menu_row.xml](//src/chrome/android/java/res/layout/context_menu_row.xml),
> [list_section_divider.xml](//src/ui/android/java/res/layout/list_section_divider.xml),
> `ContextMenuItemText` in
> [chrome/android/java/res/values/styles.xml](//src/chrome/android/java/res/values/styles.xml),
> context menu dimens in
> [components/embedder_support/android/java/res/values/dimens.xml](//src/components/embedder_support/android/java/res/values/dimens.xml).
> WebView has its own implementation (`AwContextMenuCoordinator`), not covered
> here.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Context-menu/Context Menu`: *(no variant properties — single fixed component;
  image header + 6 text items).*
- `.Menu-items`: *(no variant properties exposed; text-only row).*

| Feature / Variant                       | Figma Component                                                         | Clank (Android)                                                                                                                                       |
| :-------------------------------------- | :---------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Image header**                        | `Menu Item` frame: `Image Preview` 60px + `Primary` text ("Image name") | `ContextMenuItemType.HEADER` → `context_menu_header.xml` (`menu_header_image` 60dp + `ContextMenuHeaderTextView`)                                     |
| **Link / page header**                  | *(Not modeled)*                                                         | Same header with favicon / monogram on a 48dp circle (`circle_background`), page title + URL (+ secondary / tertiary URL)                             |
| **No header**                           | *(Not modeled)*                                                         | `hasHeader = !params.getOpenedFromHighlight() && !params.isPage()`                                                                                    |
| **Section divider**                     | `Divider` line (one, below header)                                      | `BasicListMenu.buildMenuDivider()` between every group (`ListItemType.DIVIDER` → `list_section_divider.xml`)                                          |
| **Text menu item**                      | `.Menu-items` (text only)                                               | `context_menu_row.xml` (`menu_row_text`, `@style/ContextMenuItemText`)                                                                                |
| **Item with start icon**                | *(Not modeled)*                                                         | `menu_row_icon` (36dp wide × 24dp, `visibility=gone` by default)                                                                                      |
| **Item with trailing share icon**       | *(Not modeled)*                                                         | `menu_row_share_icon` (`CONTEXT_MENU_ITEM_WITH_ICON_BUTTON`, separate 48dp click target)                                                              |
| **Submenu / flyout**                    | *(Not modeled)*                                                         | `HierarchicalMenuController` + `createAndShowFlyoutPopup()` (max width `flyout_menu_max_width` 300dp)                                                 |
| **Lens chip below menu**                | *(Not modeled)*                                                         | `ContextMenuChipController` / `context_menu_chip.xml` (`ChipView`, `@style/ContextMenuChip`), phones only                                             |
| **Dialog vs. popup presentation**       | *(Not modeled — static menu)*                                           | `ContextMenuUtils.isPopupSupported()` (tablet / `FORCE_CONTEXT_MENU_POPUP`) → `AnchoredPopupWindow`, otherwise centered full-screen dialog with scrim |
| **Incognito**                           | *(Not modeled)*                                                         | Divider forced to `@color/divider_color_light`                                                                                                        |
| **Pressed / Focused / Hovered (items)** | *(Not modeled)*                                                         | `?attr/listChoiceBackgroundIndicator` ripple on `menu_row_item` and header text                                                                       |
| **Dark theme**                          | *(Not modeled on this node)*                                            | Night resolution of `?attr/colorSurfaceBright` (`#37393B`), `colorOnSurface`, `colorOnSurfaceVariant`                                                 |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                | Figma Design Token                                                                                | Clank (Android)                                                                                                                                                                                 |
| :------------------------------ | :------------------------------------------------------------------------------------------------ | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Container background**        | `--clank/sys/color/sBright, sb` (`#ffffff`)                                                       | `?attr/popupBgShape` → `popup_bg_shape_24dp` → `@macro/menu_bg_color` → `?attr/colorSurfaceBright` (baseline light `#FFFFFF` / dark `#37393B`)                                                  |
| **Container corner radius**     | `--clank/shape/corner_radius/24` (`24px`)                                                         | `@dimen/popup_bg_corner_radius_24dp` (`24dp`); `16dp` under `ThemeOverlay.BrowserUI.DesktopDensity`                                                                                             |
| **Elevation / shadow**          | `Current/dialog, menu`: drop shadows `#3C404326` blur 6 + `#3C40431F` blur 2, offset 0            | `?attr/popupBgShadow` → `popup_bg_shadow_24dp` (9-patch `popup_bg_24dp` tinted with `menu_bg_color`) *(shadow pixels not verified)*                                                             |
| **Container width**             | Fixed `283px`                                                                                     | Content-driven: `UiUtils.computeMenuWidth()` clamped to `menu_width_min` `200dp` … min(`menu_width_max` `800dp`, window − 2 × `menu_horizontal_margin` `16dp`)                                  |
| **Container vertical padding**  | Top `0`, bottom `--clank/shape/spacing/4` (`4px`)                                                 | `ContextMenuListView` `paddingVertical="@dimen/context_menu_padding"` (`8dp` top and bottom)                                                                                                    |
| **Header padding**              | `16px` all sides; `16px` gap image → text                                                         | Row `paddingHorizontal` `8dp` + image container `padding` `8dp` = `16dp` start; text view `padding` `8dp` (+ container `8dp`) = `16dp` gap; top = list `8dp` + container `8dp` = `16dp`         |
| **Header image**                | `60 × 60px` image (`object-cover`)                                                                | `@dimen/context_menu_header_image_max_size` (`60dp`), `scaleType="centerInside"`, checkerboard background behind thumbnail                                                                      |
| **Header text**                 | `textSmall` (Google Sans Text, 12/16, 400), `--clank/sys/color/onSurfaceVariant, osv` (`#444746`) | URL: `@style/TextAppearance.TextSmall.Secondary` (12sp / `text_size_small_leading` 16sp, `colorOnSurfaceVariant`); alt text / page title: `TextAppearance.TextSmall.Primary` (`colorOnSurface`) |
| **Divider color**               | `clank/macro/divider/divider_color` (`#dde3ea`)                                                   | `HorizontalDivider` → `?android:attr/listDivider` (theme default, **not resolved/verified**); incognito `@color/divider_color_light` (`baseline_neutral_variant_30`)                            |
| **Divider thickness / spacing** | `1px`, full width, no vertical spacing                                                            | `@dimen/divider_height` (`1dp`) centered in `?attr/listDividerHeight` (`list_divider_layout_height` `16dp`) → ~`7.5dp` above and below                                                          |
| **Item text**                   | `textLarge` (Google Sans Text, 16/24, 400), `--clank/sys/color/onSurface, os` (`#1f1f1f`)         | `@style/ContextMenuItemText` → `TextAppearance.DensityAdaptive.TextLarge.Primary` (`?attr/textLargeSize` 16sp / `textLargeLeading` 24sp, `@color/default_text_color_list` → `colorOnSurface`)   |
| **Item horizontal padding**     | `--clank/shape/spacing/16` (`16px`) on the list container                                         | `menu_row_item` `paddingHorizontal="@dimen/context_menu_list_lateral_padding"` (`16dp`)                                                                                                         |
| **Item height / vertical pad**  | `48px` (`--clank/shape/spacing/12` = `12px` top/bottom + 24px line)                               | `ContextMenuItemText` `minHeight="?attr/minInteractTargetSize"` (`48dp`, `min_click_target_size` on desktop density), `gravity="center_vertical"`                                               |
| **Font family**                 | `clank/font/font_family/alternative` (Google Sans Text)                                           | Inherited from `TextAppearance.TextLarge` / `TextSmall` base styles *(font family resolution not verified)*                                                                                     |
| **Outer margins**               | *(Not modeled)*                                                                                   | `menu_horizontal_margin` `16dp`, `menu_vertical_margin` `48dp` (`min_touch_target_size`)                                                                                                        |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Single static Figma component vs. data-driven list

- **Figma**: One fixed component with hard-coded content (image header + 6
  items) and no variant properties.
- **Clank**: The menu is assembled at runtime from grouped `ModelList`s
  (`ContextMenuMediator.updateAndGetModelList`). Header type (image / link /
  none), number of dividers, icons, share buttons, and submenus all vary. Figma
  can't represent link/page headers, multiple sections, or the no-header case.

### 2. Presentation modes

- Code has two shells: centered full-screen dialog with scrim (phones, plus
  optional Lens chip anchored below) and `AnchoredPopupWindow` (tablets / mouse
  / highlight). Figma only shows the floating card.

### 3. Width

- Figma fixes the card at `283px`. Code sizes the list to its content within
  `200dp`–`800dp` and the window minus `16dp` side margins, so on phones the
  menu is usually much wider than `283px`.

### 4. Nested `.Menu-items` sub-component

- `.Menu-items` is a hidden component in Figma; code renders rows via
  `context_menu_row.xml`, which also supports start icons and a trailing share
  icon button that Figma does not model.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Divider spacing and color

- Figma: a full-width `1px` divider directly between the header and the first
  item (no extra spacing), colored `divider_color` `#dde3ea`.
- Code: `list_section_divider.xml` reserves `16dp` (`listDividerHeight`) with
  the `1dp` line centered, which adds ~`8dp` of vertical space. The line color
  comes from `?android:attr/listDivider`, which I couldn't trace to a Chromium
  token, so whether it matches `#dde3ea` is **unverified**.

### 2. Container vertical padding

- Figma: no top padding (the header supplies its own `16px`) and `4px` bottom
  padding.
- Code: `8dp` top and bottom on the `ContextMenuListView`. The header's top
  inset still adds up to `16dp` (`8dp` list + `8dp` image container), but the
  bottom inset is `8dp`, not `4px`.

### 3. Header text token

- Figma: "Image name" uses `onSurfaceVariant` / `textSmall`.
- Code: the header can show page title and alt text (`TextSmall.Primary`,
  `onSurface`) as well as URL lines (`TextSmall.Secondary`, `onSurfaceVariant`).
  Line counts change dynamically (tapping the header expands to unlimited
  lines). Figma's single "Image name" line doesn't say which field it
  represents.

### 4. Header image scaling

- Figma: `object-cover` fill of a 60px square.
- Code: `centerInside` within `60dp` on a checkerboard background, so non-square
  images get letterboxed instead of cropped.

### 5. Missing interactive states

- Figma has no pressed / focused / hovered states for items or the header. Code
  uses the `listChoiceBackgroundIndicator` ripple and keyboard focus
  (`setItemsCanFocus(true)`).

### 6. Desktop density

- Under `ThemeOverlay.BrowserUI.DesktopDensity`, the corner radius drops to
  `16dp`, item min height uses `min_click_target_size`, and text size uses
  `text_size_large_desktop`. Figma doesn't model any of this.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Group related actions into sections; code inserts a divider between each group
  automatically, so don't add manual dividers.
- Keep item labels short, sentence case, and verb-first (e.g., "Open image in
  new tab", "Copy image").
- Use the header to identify the target (image thumbnail / favicon + title /
  URL) so users can confirm what the actions apply to.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Rows must be at least `48dp` tall
  (`?attr/minInteractTargetSize`). The trailing share icon is its own `48dp`
  target with its own content description.
- **TalkBack**: The header image is `importantForAccessibility="no"`; the header
  text is focusable/clickable (expands truncated text). Dividers are not
  focusable.
- **Keyboard / D-pad**: `ContextMenuListView` allows item focus; submenus open
  as flyouts via `HierarchicalMenuController`.

### 3. Icon Usage Guidelines

- Start icons are optional and decorative (part of the row's click target). Size
  them to `context_menu_chip_icon_size` (`24dp`) in the
  `list_item_start_icon_width` (`36dp`) slot.
- Only use the trailing share icon for the most-recently-used share target.

______________________________________________________________________

> **Not verified**: exact shadow rendering of `popup_bg_24dp` against the Figma
> `Current/dialog, menu` effect; the resolved color of
> `?android:attr/listDivider` in the Chrome theme; font family resolution
> (Google Sans Text); dark-mode Figma values (only light-mode variables were
> returned for this node); runtime rendering (no screenshots compared).
