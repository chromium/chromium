# List Bookmarks Spec (Clank)

This document maps the specifications of the **List Bookmarks** (Clank)
component across Figma and code.

______________________________________________________________________

## Overview

**Bookmark list rows** show bookmarks and folders in the Bookmarks manager in
two densities: a **compact** row (48px circular icon, title, overflow menu) and
a **visual/thumbnail** row (80px folder or image thumbnail, title, metadata,
price-tracking chip, overflow menu or selection check). In Clank both are
implemented by `ImprovedBookmarkRow` (inflated from
`improved_bookmark_row_layout.xml` or `improved_bookmark_row_layout_visual.xml`
depending on `BookmarkRowDisplayPref`). Folder thumbnails use
`ImprovedBookmarkFolderView`, and the price chip uses `ShoppingAccessoryView`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `List-bookmarks-compact`<br>`List-bookmarks-thumbnail`<br>`.Folder-thumbnail`                                                                                                                                                                                                                                                                                                                                                                                                                                          | `ImprovedBookmarkRow`<br>`ImprovedBookmarkFolderView`<br>`ShoppingAccessoryView`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Source Files**   | [Figma Link: `List-bookmarks-compact` (`32130:19989`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32130-19989&m=dev)<br>[Figma Link: `List-bookmarks-thumbnail` (`32119:3975`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32119-3975&m=dev)<br>[Figma Link: `.Folder-thumbnail` (`32119:1319`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32119-1319&m=dev) | [chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ImprovedBookmarkRow.java](//src/chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ImprovedBookmarkRow.java)<br>[chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ImprovedBookmarkFolderView.java](//src/chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ImprovedBookmarkFolderView.java)<br>[chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ShoppingAccessoryView.java](//src/chrome/browser/bookmarks/android/java/src/org/chromium/chrome/browser/bookmarks/ShoppingAccessoryView.java) |

> Source page:
> [`↳ Lists` (`6371:41566`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-41566),
> showcase frame `Components/List/Bookmarks` (`40052:3156`). `.Folder-thumbnail`
> is a private (`.`-prefixed) sub-component nested in
> `List-bookmarks-thumbnail`.
>
> MVC: `ImprovedBookmarkRowCoordinator`, `ImprovedBookmarkRowViewBinder`,
> `ImprovedBookmarkRowProperties`; color helpers in `BookmarkViewUtils`.
> Layouts:
> [improved_bookmark_row_layout.xml](//src/chrome/android/java/res/layout/improved_bookmark_row_layout.xml),
> [improved_bookmark_row_layout_visual.xml](//src/chrome/android/java/res/layout/improved_bookmark_row_layout_visual.xml),
> [improved_bookmark_row_content.xml](//src/chrome/android/java/res/layout/improved_bookmark_row_content.xml),
> [improved_bookmark_folder_view_layout.xml](//src/chrome/android/java/res/layout/improved_bookmark_folder_view_layout.xml).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `List-bookmarks-compact`: `Type` (1-line | 2-line). Booleans: `Left Icon`
  (default true), `Right Icon` (default true).
- `List-bookmarks-thumbnail`: `Type` (Full Image | Icon | 1 Image | 2 Image | 3
  Image) × `Selected` (False | True). Booleans: `Show Metadata` (default true),
  `Show Price Indicator` (default true), `RHS icon` (default true).
- `.Folder-thumbnail`: `Type` (Icon | Full Image | 1 Image | 2 Image | 3 Image).
  Boolean: `Counter` (default true).

| Feature / Variant                      | Figma Component                                              | Clank (Android)                                                                                    |
| :------------------------------------- | :----------------------------------------------------------- | :------------------------------------------------------------------------------------------------- |
| **Compact density**                    | `List-bookmarks-compact`                                     | `BookmarkRowDisplayPref.COMPACT` → `improved_bookmark_row_layout.xml`                              |
| **Visual density**                     | `List-bookmarks-thumbnail`                                   | `BookmarkRowDisplayPref.VISUAL` → `improved_bookmark_row_layout_visual.xml`                        |
| **Compact 1-line / 2-line**            | `Type=1-line` / `Type=2-line`                                | `title` + optional `description` (`title_and_description_layout.xml`)                              |
| **Leading folder/favicon icon**        | `Left Icon=true` (`folder` in 48px circle)                   | `start_image` `RoundedCornerImageView` 48dp, radius `improved_bookmark_icon_radius` 24dp           |
| **Overflow menu**                      | `Right Icon=true` / `RHS icon=true` (`more_vert`)            | `more` `ListMenuButton` (`min_touch_target_size` wide, `ic_more_vert_24dp`)                        |
| **Folder, no images**                  | `Type=Icon` (folder `Type=Icon` + `Counter`)                 | `ImprovedBookmarkFolderView` `no_image_placeholder_*` + `child_count_text`                         |
| **Folder, 1 / 2 / 3 images**           | `Type=1 Image` / `2 Image` / `3 Image`                       | `ImprovedBookmarkFolderView` `primary_image` + `secondary_image` + `child_count_*` backgrounds     |
| **Bookmark with full image**           | `Type=Full Image`                                            | `start_image` 80dp, radius `improved_bookmark_row_outer_corner_radius` 16dp                        |
| **Metadata line**                      | `Show Metadata=true`                                         | `description` (`TextAppearance.TextMedium.Secondary`)                                              |
| **Price tracking chip**                | `Show Price Indicator=true` (`notifications_active` + price) | `ShoppingAccessoryView`                                                                            |
| **Selected (multi-select mode)**       | `Selected=True` (`Round Checkbox` replaces `more_vert`)      | `setSelected(true)` → `check_image` (`material_ic_check_circle_filled_24dp`) + selected background |
| **Drag handle / local-only indicator** | *(Not modeled)*                                              | `drag_handle`, `local_bookmark_image` (`ic_cloud_off_24dp`)                                        |
| **Pressed / Focused / Hovered**        | *(Not modeled)*                                              | `improved_bookmark_row_visual_background` → `list_item_rounded_background_selector`                |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                                                                                                                                                     | Clank (Android)                                                                                                                                                                                                     |
| :------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Title**                        | `textLarge` 16/24 Regular, `onSurface` (`#1f1f1f`)                                                                                                                                     | `TextAppearance.TextLarge.Primary` → `?attr/colorOnSurface`, `16sp`/`24sp` ✅                                                                                                                                       |
| **Metadata / 2nd line**          | `textMedium` 14/20, `onSurfaceVariant` (`#444746`)                                                                                                                                     | `TextAppearance.TextMedium.Secondary` → `?attr/colorOnSurfaceVariant` ✅                                                                                                                                            |
| **Compact icon size / shape**    | `48px`, `--clank/shape/corner_radius/100` (circle)                                                                                                                                     | `@dimen/improved_bookmark_start_image_size_compact` `48dp`, `@dimen/improved_bookmark_icon_radius` `24dp` ✅                                                                                                        |
| **Compact icon container color** | `--clank/sys/color/scontainer, -sc` (`#f0f4f9`)                                                                                                                                        | `BookmarkViewUtils.getIconBackground`: regular → **`colorSurfaceContainerLow`**; special folders → `colorPrimaryContainer`                                                                                          |
| **Compact icon glyph color**     | `folder` `onSurfaceVariant` (`#444746`)                                                                                                                                                | `default_icon_color_secondary_tint_list` (regular) / `default_icon_color_on_accent1_container` (special)                                                                                                            |
| **Compact vertical padding**     | `--clank/shape/spacing/8` (`8px`)                                                                                                                                                      | `paddingVertical="8dp"` ✅                                                                                                                                                                                          |
| **Compact horizontal inset**     | `0px` (text gap `16px`)                                                                                                                                                                | `layout_marginHorizontal` `10dp` + `paddingStart` `6dp` (= 16dp) ; `16dp` spacer views before/after text ✅                                                                                                         |
| **Visual row height / padding**  | `96px` height, `2px` outer + `6px` inner padding                                                                                                                                       | `paddingVertical="6dp"`, `layout_marginVertical` `1dp`; 80dp thumbnail → ~94dp row (approx.)                                                                                                                        |
| **Thumbnail size / radius**      | `80px`, `--clank/shape/corner_radius/16`                                                                                                                                               | `@dimen/improved_bookmark_row_size` / `start_image_size_visual` `80dp`, `improved_bookmark_row_outer_corner_radius` `16dp` ✅                                                                                       |
| **Folder (Icon) background**     | `--clank/sys/color/primaryContainer, -pc` (`#d3e3fd`)                                                                                                                                  | Special folders: `colorPrimaryContainer` ✅; regular folders: `colorSurfaceContainerLow` (Figma doesn't model this)                                                                                                 |
| **Folder (Icon) glyph**          | `list_alt` 24px at `12px` inset, `--clank/sys/color/onPrimaryContainer, -opc` (`#0842a0`)                                                                                              | Special folders: `default_icon_color_on_accent1_container` (see compact glyph row); exact resolved attr not checked                                                                                                 |
| **Folder counter text**          | `textLarge` 16/24, `onSurface`                                                                                                                                                         | `TextAppearance.RegularFolderChildCount` (TextLarge.Primary, `16dp`); special: `SpecialFolderChildCount` (`OnAccent1Container`)                                                                                     |
| **Folder mosaic tiles**          | `39px` tiles, `41px` offsets. Counter tile: `Icon` → text only, no fill; `2 Image` → `scontainer` fill; `1 Image` → `scontainer` fill + `2px` `surface` border + `4px` top-left radius | `improved_bookmark_row_secondary_inner_size` `39dp`, `secondary_outer_size` `41dp`, `improved_bookmark_row_inner_corner_radius` `4dp`; counter bg **`colorSurfaceContainerLow`**, separators `colorSurface`         |
| **Price chip**                   | bg `scontainer` (`#f0f4f9`), `8px` radius, `8×2px` padding, `textSmall` 12/16 `onSurfaceVariant`, 16px icon                                                                            | `ShoppingAccessoryView`: default `SemanticColorUtils.getColorSurfaceContainer` ✅; price-drop `price_indicator_bg_color` / `price_indicator_text_color`; padding `shopping_chip_padding` — radius/type not verified |
| **Row background (unselected)**  | `--clank/sys/color/surface` (white)                                                                                                                                                    | `improved_bookmark_row_visual_background` base `?attr/colorSurface`, radius 16dp ✅                                                                                                                                 |
| **Row background (selected)**    | `--clank/sys/color/scontainerlow, -scl` (`#f8fafd`), `16px` radius                                                                                                                     | `@color/color_on_surface_with_alpha_8` overlay, radius 16dp (outline radius switches to `default_rounded_corner_radius` 8dp when selected)                                                                          |
| **Selection check**              | `Round Checkbox`: `primary` fill, `onPrimary` check, 20px in 24px box                                                                                                                  | `material_ic_check_circle_filled_24dp` tinted `@macro/default_icon_color_accent1` (primary) ✅                                                                                                                      |
| **Overflow icon**                | `more_vert` 24px, `onSurfaceVariant`                                                                                                                                                   | `ic_more_vert_24dp`, `default_icon_color_tint_list` — exact resolved color not verified                                                                                                                             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. One code row, two Figma components

- The Figma `List-bookmarks-compact` and `List-bookmarks-thumbnail` are both
  `ImprovedBookmarkRow`. The density is a user preference
  (`BookmarkRowDisplayPref`) that picks the layout. The two Figma components
  should evolve together.

### 2. Container color for regular items

- Figma uses `scontainer` (compact) and `primaryContainer` (folder icon) for all
  items. Code uses `colorPrimaryContainer` only for **special** folders (Mobile
  bookmarks, Reading list, and so on) and `colorSurfaceContainerLow` for
  everything else (`BookmarkViewUtils.getIconBackground`). Figma doesn't model
  the regular-folder look. The Figma `.Folder-thumbnail` `Type=Icon` even uses
  the Reading-list glyph (`list_alt`), so it shows a special folder only.

### 3. Selected state treatment

- **Figma**: Fills the row with `scontainerlow`.
- **Clank**: Draws an 8% `onSurface` overlay (`color_on_surface_with_alpha_8`)
  and changes the outline radius to 8dp. The two look similar but use different
  tokens.

### 4. Desktop-form-factor sizing

- Code switches to 36dp start images with 8dp radius when the desktop bookmarks
  layout/dialog is enabled (`improved_bookmark_start_image_size_desktop`). Figma
  doesn't model this.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Code-only affordances

Drag handle (reorder mode), the local-only/cloud-off indicator, and disabled-row
alpha (`default_disabled_alpha`) have no Figma variants.

### 2. Missing interactive states

There are no pressed, hovered, or focused variants. Code uses
`list_item_rounded_background_selector` ripples.

### 3. Price chip variants

Figma shows a single neutral chip. Code also has a price-drop variant with
`price_indicator_bg_color` / `price_indicator_text_color`.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use compact rows for dense lists and visual rows when images help users
  recognize items. Respect the user's display preference instead of hard-coding
  a density.
- Show the folder child count only in the `Icon` thumbnail state or as the
  mosaic counter tile. Don't duplicate it in the metadata line.
- In selection mode, replace the overflow menu with the selection check. Never
  show both.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Thumbnails, folder views, and start images are decorative
  (`importantForAccessibility="no"`). The row content description must include
  the title and selection state.
- Give the overflow `ListMenuButton` a content description (for example, "More
  options for {title}"). It's sized to `@dimen/min_touch_target_size` (`48dp`).
- Drag-to-reorder should also offer an accessible alternative (move up/down
  actions).

### 3. Icon Usage Guidelines

- Use `folder` (24px) in the compact container for folders and the site favicon
  for bookmarks. In visual mode, prefer the page image and fall back to the
  folder mosaic or icon.
- Use `more_vert` for overflow and `notifications_active` (16px) for the
  price-tracking chip.
