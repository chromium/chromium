# List Items Spec (Desktop)

This document maps the specifications of the **List Items** (Desktop) component
across Figma and code.

______________________________________________________________________

## Overview

**List Items** are rows that represent a URL (website) or a folder of URLs, used
in surfaces such as the Bookmarks, Reading List and History side panels. Each
row has a leading asset (favicon, page image, or folder icon with count), a
title, an optional description and badges (chips), and a trailing overflow
action that appears on hover. It comes in `Large`, `Medium` and `Compact` sizes
and is implemented in WebUI by `<cr-url-list-item>`; badges use
`<sp-list-item-badge>`. There is no matching C++ Views control.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                  | C++ Views (Desktop)              | WebUI (Desktop)                                                                                                                                                                                                                                                                                |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `List Items` (component set, 22 variants)<br>`List Item Badge` (nested in `Has Chips?=Yes` variants)                                                                                                                                                                                                                             | *(No direct equivalent; see §4)* | `<cr-url-list-item>`<br>`<sp-list-item-badge>`                                                                                                                                                                                                                                                 |
| **Source Files**   | [Figma Link: `List Items` (`280:28521`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-28521)<br>[Figma Link: `List Item Badge` instance (`I48:3220;4483:25591`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-28500) | —                                | [ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts](//src/ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts)<br>[chrome/browser/resources/side_panel/shared/sp_list_item_badge.ts](//src/chrome/browser/resources/side_panel/shared/sp_list_item_badge.ts) |

> [!NOTE] `<sp-list-item-badge>` is a side-panel-scoped element, not a shared
> `cr_elements` component. `<cr-url-list-item>` only exposes a `badges` slot.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `List Items`: `Size` (Large | Medium | Compact) × `Type` (Website | Folder) ×
  `Has Image?` (Yes | No) × `Has Chips?` (Yes | No) × `Theme` (Light) × `State`
  (Default | Hovered). The set is sparse and contains 11 Default/Hovered pairs:
  - Large: Website (Image No/Yes × Chips No/Yes), Folder (Image No/Yes, Chips
    No)
  - Medium: Website (Image No/Yes, Chips No)
  - Compact: Website (Image No, Chips No/Yes), Folder (Image No, Chips No)
- `List Item Badge`: there are no exposed axes in this frame. Two visual
  treatments appear: neutral (Compact, `$750`) and tertiary/"discounted" (Large,
  `$699` and struck-through `$750`).

| Feature / Variant                      | Figma Component                                  | WebUI (Desktop)                                                                                                                            |
| :------------------------------------- | :----------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------- |
| **Large size**                         | `Size=Large` (68px tall)                         | `<cr-url-list-item size="large">` (`CrUrlListItemSize.LARGE`)                                                                              |
| **Medium size**                        | `Size=Medium` (48px tall)                        | `<cr-url-list-item size="medium">` *(default)*                                                                                             |
| **Compact size**                       | `Size=Compact` (36px tall, single line)          | `<cr-url-list-item size="compact">`                                                                                                        |
| **Website**                            | `Type=Website`                                   | Set `.url`. Leave `count` undefined.                                                                                                       |
| **Folder**                             | `Type=Folder`                                    | Set `.count` (sets reflected `is-folder_`). Leave `url` undefined.                                                                         |
| **Favicon (no image)**                 | `Has Image?=No`, `Type=Website`                  | `.favicon` (16px) shown when `url` is set and `imageUrls` is empty, or when `size=compact`                                                 |
| **Page image**                         | `Has Image?=Yes`, `Type=Website`                 | `.imageUrls=[…]`. The first image is rendered in `.image-container` (`cr-auto-img`). Compact always falls back to the favicon.             |
| **Folder icon + count (no image)**     | `Has Image?=No`, `Type=Folder`                   | `.folder.cr-icon.icon-folder-open` plus `.count`. The count is visible only in `size=large`. `folder-icon` slot to override.               |
| **Folder image mosaic**                | `Has Image?=Yes`, `Type=Folder` (Large only)     | `.imageUrls` (first two used) in `.folder-and-count` 2×2 grid with `.count` in the bottom-right cell                                       |
| **Description (URL / "23 bookmarks")** | Secondary text line                              | `.description` (+ optional `.descriptionMeta`, `reverseElideDescription`)                                                                  |
| **Chips**                              | `Has Chips?=Yes` → `List Item Badge`             | `<sp-list-item-badge slot="badges">` (sets reflected `has-badges`)                                                                         |
| **Badge – neutral**                    | Neutral badge (Compact)                          | `<sp-list-item-badge>` *(default)*                                                                                                         |
| **Badge – discounted/updated**         | Tertiary badge with struck-through price (Large) | `<sp-list-item-badge was-updated>` + child `slot="previous-badge"`                                                                         |
| **Trailing action (more_vert)**        | `RHS Action` shown in `State=Hovered` only       | `<cr-icon-button slot="suffix" iron-icon="cr:more-vert">`. Shown on `.hovered`, on keyboard `:focus-within`, or with `always-show-suffix`. |
| **Default state**                      | `State=Default`                                  | No class                                                                                                                                   |
| **Hovered state**                      | `State=Hovered`                                  | `:host(.hovered)` (`MouseHoverableMixinLit`) or `force-hover` attribute                                                                    |
| **Pressed / Active**                   | *(Not modeled in Figma)*                         | `:host(.active)` (toggled on `pointerdown`/`pointerup`)                                                                                    |
| **Keyboard focus**                     | *(Not modeled in Figma)*                         | `:host-context(.focus-outline-visible):host(:focus-within)` (background fill. There is no outline ring except in forced-colors mode.)      |
| **Dark theme**                         | *(Only `Theme=Light` modeled)*                   | Resolved via `--color-*` variables. `@media (prefers-color-scheme: dark)` swaps hardcoded image borders.                                   |
| **Leading prefix (e.g. checkbox)**     | *(Not modeled in Figma)*                         | `slot="prefix"` (used by Power Bookmarks edit mode)                                                                                        |
| **Custom content (e.g. rename input)** | *(Not modeled in Figma)*                         | `slot="content"` (hides `.metadata`)                                                                                                       |
| **Link vs. button semantics**          | *(Not modeled in Figma)*                         | `asAnchor` / `asAnchorTarget` (native `<a>`), otherwise `<button>`                                                                         |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

Code values come from
[cr_url_list_item.css](//src/ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.css)
and
[sp_list_item_badge.css](//src/chrome/browser/resources/side_panel/shared/sp_list_item_badge.css).
Color-ID mappings come from
[material_ui_color_mixer.cc](//src/ui/color/material_ui_color_mixer.cc) and
[material_side_panel_color_mixer.cc](//src/chrome/browser/ui/color/material_side_panel_color_mixer.cc).

### Colors

| Design Attribute                   | Figma Design Token                                                    | WebUI (Desktop)                                                                                                                                                                | Status                     |
| :--------------------------------- | :-------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------- |
| **Row background (Default)**       | `--desktop/sys/base-colors/base-container-elevated` (`#ffffff`)       | None. Host is transparent and inherits from the container. The side panel uses `kColorSidePanelContentBackground` → `kColorSysBaseContainer`, **not** `BaseContainerElevated`. | ❌ Mismatch (implicit)     |
| **Row background (Hovered)**       | Unchanged `base-container-elevated`. No hover overlay token exported. | `--cr-hover-background-color` → `--color-sys-state-hover` (fallback on-surface @ 8%)                                                                                           | ❌ Mismatch                |
| **Row background (Pressed/Focus)** | *(Not modeled)*                                                       | `--cr-active-background-color` → `--color-sys-state-pressed` (fallback on-surface @ 12%)                                                                                       | ⚠️ Not in Figma            |
| **Title text**                     | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                 | `--cr-primary-text-color` → `--color-primary-foreground` (`kColorPrimaryForeground` → `kColorSysOnSurface`)                                                                    | ✅                         |
| **Description text**               | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)          | `--cr-secondary-text-color` → `--color-secondary-foreground` (`kColorSecondaryForeground` → `kColorSysOnSurfaceSubtle`)                                                        | ✅                         |
| **Favicon / image container**      | `--desktop/sys/container-colors/neutral-container` (`#f2f2f2`)        | `--color-list-item-url-favicon-background` (`kColorListItemUrlFaviconBackground` → `kColorSysNeutralContainer`)                                                                | ✅                         |
| **Folder container**               | `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`)          | `--color-list-item-folder-icon-background` (`kColorListItemFolderIconBackground` → `kColorSysTonalContainer`). The CSS fallback is `--cr-fallback-color-primary-container`.    | ✅ (fallback name differs) |
| **Folder icon / count foreground** | `--desktop/sys/container-colors/on-tonal-container` (`#041e49`)       | `--color-list-item-folder-icon-foreground` (`kColorListItemFolderIconForeground` → `kColorSysOnTonalContainer`)                                                                | ✅                         |
| **Folder mosaic separators**       | `base-container-elevated` behind images (gaps)                        | Hardcoded `2px solid white` (`black` in dark mode)                                                                                                                             | ❌ Hardcoded               |
| **Image overlay**                  | *(None)*                                                              | `.image-container { background: black }` + `img { opacity: 95% }` (light mode only)                                                                                            | ⚠️ Code-only               |
| **Trailing icon (more_vert)**      | `--desktop/sys/surface-colors/on-surface-subtle`                      | `<cr-icon-button>` default icon color (`--cr-icon-button-fill-color`). Not set by the list item.                                                                               | ⚠️ Not verified            |
| **Badge background (neutral)**     | `--desktop/sys/container-colors/neutral-container`                    | `--color-side-panel-badge-background` (`kColorSidePanelBadgeBackground` → `kColorSysNeutralContainer`)                                                                         | ✅                         |
| **Badge foreground (neutral)**     | `--desktop/sys/surface-colors/on-surface-subtle`                      | `--color-side-panel-badge-foreground` (`kColorSidePanelBadgeForeground` → `kColorSysOnSurfaceVariant`)                                                                         | ❌ Token differs           |
| **Badge background (updated)**     | `--desktop/sys/tertiary-colors/tertiary-container` (`#c4eed0`)        | `--color-side-panel-badge-background-updated` (`→ kColorSysTertiaryContainer`)                                                                                                 | ✅                         |
| **Badge foreground (updated)**     | `--desktop/sys/tertiary-colors/on-tertiary-container` (`#072711`)     | `--color-side-panel-badge-foreground-updated` (`→ kColorSysOnTertiaryContainer`)                                                                                               | ✅                         |
| **Struck-through previous price**  | `--desktop/sys/surface-colors/on-surface-subtle`                      | `--color-side-panel-badge-foreground` (`→ kColorSysOnSurfaceVariant`)                                                                                                          | ❌ Token differs           |

### Typography

| Design Attribute          | Figma Design Token                                                                              | WebUI (Desktop)                                                                         | Status                          |
| :------------------------ | :---------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------- | :------------------------------ |
| **Title**                 | `desktop/body/four (medium)`: 12px / 500 / line-height 18px                                     | `.title`: `font-size: 12px; font-weight: 500`. Line-height is inherited (not set).      | ⚠️ Line-height not pinned       |
| **Description (L/M)**     | 11px (hardcoded, not tokenized) / 400 / `--desktop/line_height/body-five` 16px; box height 14px | `.description`: `11px / 400`; `.descriptions { height: 14px }`. Line-height is not set. | ⚠️                              |
| **Description (Compact)** | `desktop/font_size/body-four` 12px / 400 / line-height 18px ("23 bookmarks")                    | `:host([size=compact]) .description { font-size: 12px }`                                | ✅                              |
| **Folder count**          | `desktop/body/four (medium)`: 12px / 500 / 18px                                                 | `.count { font-weight: 500 }`. Font-size is inherited (not set).                        | ⚠️                              |
| **Badge text**            | `desktop/body/five (regular)`: 11px / 400 / 16px                                                | `:host { font-size: 11px; line-height: normal }`                                        | ⚠️ Line-height `normal` vs 16px |

### Layout, spacing & shape

| Design Attribute                  | Figma                                                                                                          | WebUI (Desktop)                                                                                  | Status                                   |
| :-------------------------------- | :------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------- | :--------------------------------------- |
| **Row height L / M / C**          | 68 / 48 / 36px                                                                                                 | `--cr-url-list-item-height` default 68 / 48 / 36px                                               | ✅                                       |
| **Row padding L / M / C**         | `spacing/6`·`spacing/16` / `spacing/4`·`spacing/16` / `spacing/6`·`spacing/16`                                 | `6px 16px` / `4px 16px` / `6px 16px` (`--cr-url-list-item-padding`)                              | ✅                                       |
| **Asset size L / M / C**          | 56 / 40 / 24px                                                                                                 | `#iconContainer` 56 / 40 / 24px                                                                  | ✅                                       |
| **Asset corner radius L / M / C** | `corner-radius/8` / `corner-radius/8` / `corner-radius/4`                                                      | 8px / 8px / 4px (hardcoded)                                                                      | ✅ (hardcoded)                           |
| **Asset → text gap L / M / C**    | `spacing/16` / `spacing/16` / `spacing/8`                                                                      | `margin-inline-end` 16 / 16 / 8px                                                                | ✅                                       |
| **Favicon size**                  | 16px (Compact wrapper `corner-radius/2`)                                                                       | `.favicon` 16px with no radius                                                                   | ⚠️ Minor                                 |
| **Folder icon size**              | 20px (Large) / 16px (Compact), glyph `folder`                                                                  | `--cr-icon-size` 20px / 16px (compact), glyph `icon-folder-open`                                 | ⚠️ Glyph differs (not visually verified) |
| **Title ↔ description gap**       | Large `spacing/2`; Medium `spacing/4`; Compact `spacing/8` (title ↔ badge) / `spacing/4` (folder title ↔ desc) | `.metadata` gap: Large 2px, Medium/Compact 4px. `.badges` adds `margin-inline-start: 4px`.       | ✅ (Compact badge ≈ 4+4px)               |
| **Large: text ↔ chips**           | Column is `justify-between` across 56px with `pt 2px` (chips bottom-aligned)                                   | `.descriptions` column `gap: 4px`, vertically centered                                           | ⚠️ Approximate                           |
| **Chip gap**                      | `spacing/4`                                                                                                    | `.badges { gap: 4px }`                                                                           | ✅                                       |
| **Badge padding**                 | `2px 8px 2px 6px` (+16px line-height = 20px)                                                                   | `height: 20px; padding: 4px 8px 4px 6px`                                                         | ✅ (same outer size)                     |
| **Badge radius**                  | `corner-radius/fully-rounded` (999px)                                                                          | `border-radius: 100px`                                                                           | ✅                                       |
| **Badge icon size / gap**         | 12px icon, `spacing/4` gap, 2px between prices                                                                 | `::slotted([icon])` 12px + `padding-inline-end: 4px`; previous price `padding-inline-start: 2px` | ✅                                       |
| **Trailing action**               | 16px `more_vert`, 16px from right edge                                                                         | `cr-icon-button` 24px / 16px icon, `.suffix { margin-inline-end: -4px }` → icon 16px from edge   | ✅                                       |
| **Row width**                     | 320px (Figma artboard)                                                                                         | `width: 100%`                                                                                    | ✅ (fluid)                               |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

- **No C++ Views equivalent.** `ui/views/controls/` has no URL/folder list-row
  control. The closest Views building blocks are `HoverButton` and
  `RichHoverButton` in `chrome/browser/ui/views/controls/`. They were **not
  mapped or token-checked** because their layout model is quite different (no
  folder mosaic, badges or size enum).
- **Badges are not part of the shared element.** Figma ships `List Item Badge`
  with List Items, but code puts badge styling in the side-panel-only
  `<sp-list-item-badge>`. Other `cr-url-list-item` users, such as
  `history_embeddings`, `organizer_panel` and `contextual_tasks`, must supply
  their own badge.
- **Row background is implicit.** Figma paints `base-container-elevated` on each
  row. Code leaves the host transparent, so the row takes whatever surface it is
  placed on. In the side panel that is `kColorSysBaseContainer`
  (`kColorSidePanelContentBackground`), which differs from Figma's elevated
  token.
- **Hardcoded values.** The mosaic separators use `2px solid white/black`.
  Corner radii (4/8px), sizes and font sizes are literal values in CSS instead
  of `--cr-*` tokens. The image container also has an intentional 1px
  anti-aliasing offset (5px/9px).
- **Typography line-heights are inherited.** The CSS does not set the Figma 18px
  (body-four) or 16px (body-five) line-heights, so the rendered text box depends
  on the host page.
- **Folder count clamping.** `getDisplayedCount_()` shows `99+` only when
  `count > 999`, so 3-digit counts (100–999) render in full. The comment says
  the box "only fits 3 characters".

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **Hovered state:** In Figma, the only change on hover is the `more_vert`
  trailing action appearing. The row fill stays `base-container-elevated` and no
  state-layer token is exported. In code, hover also applies a
  `--cr-hover-background-color` (sys-state-hover) fill. One of the two needs to
  be updated: either Figma adds the hover overlay, or this is an intentional
  difference.
- **Pressed / focus states are missing in Figma.** Code applies the
  `sys-state-pressed` fill for `.active` and for keyboard `:focus-within`. It
  draws no outline ring (`outline: 0`) except in forced-colors mode.
- **Dark theme:** Only `Theme=Light` is modeled.
- **Sparse variant matrix:** Figma has no Medium folder, Medium with chips, or
  Compact with image. In code:
  - Compact ignores `imageUrls`, which matches Figma.
  - Medium folders are possible, but the `.folder-and-count` grid and `.count`
    are only styled for `size=large`, so a Medium folder shows only the folder
    icon. This is undefined in Figma.
- **Compact badge layout:** In Figma, the badge sits inline right after the
  title with an 8px gap. In code, `.descriptions` becomes `display: contents`
  and the badges get `margin-inline-start: 4px` plus the 4px metadata gap (≈8px
  total).
- **Badge foreground token:** Figma uses `on-surface-subtle`, but code uses
  `kColorSysOnSurfaceVariant`. These are different sys tokens (`Neutral30` vs.
  `NeutralVariant30` in light mode).
- **Folder glyph:** Figma uses the `folder` (closed) symbol, but code uses
  `icon-folder-open` (`icon_folder_open.svg`). This was not compared visually.
- **Focus/hover suffix visibility:** Figma only shows `RHS Action` on hover.
  Code also shows it on keyboard focus (`:focus-within`) and when
  `always-show-suffix` is set.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Pick a size based on information density:
  - `Large` for image-rich lists that show price chips (shopping/bookmarks).
  - `Medium` for standard URL lists with a secondary URL line.
  - `Compact` for dense, single-line lists (e.g. folder trees).
- Use the same size within a list. Mixing sizes breaks vertical rhythm.
- Keep the title short and meaningful (page title or folder name). Put the
  URL/domain or the item count (e.g. "23 bookmarks") in the description.
- Limit chips to short status or price information. Use the updated/discounted
  (tertiary) badge only for a positive change, with the previous value struck
  through.
- Put secondary actions in the trailing `more_vert` menu. Don't add multiple
  trailing buttons.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- WebUI:
  - The whole row is a single focusable `<button>`, or `<a>` when `asAnchor` is
    set. Use `asAnchor` for real navigations to get the native link context menu
    and middle-click.
  - Provide `itemAriaLabel` / `itemAriaDescription` when the visible title or
    description is not descriptive enough. They default to `title` and
    `description`.
  - The trailing `cr-icon-button` needs its own `aria-label` and `title`.
  - In tree-like lists (bookmarks), the consumer sets `role="treeitem"`.
  - Use `FocusOutlineManager` so the keyboard focus fill only appears for
    keyboard navigation.
- When `actionable` is set, `<sp-list-item-badge>` becomes focusable
  (`role="button"`). Its current `tabindex="1"` (positive) is an a11y
  anti-pattern and worth reviewing.
- C++ Views: N/A (no equivalent control).

### 3. Icon Usage Guidelines

- Leading asset:
  - Website: favicon (16px, centered in the neutral container) or a page image
    filling the container.
  - Folder: folder glyph on the tonal container. Large shows the item count in
    the bottom-right cell and up to two preview images.
- Use the `folder-icon` slot for special folders (e.g. Bookmarks bar, Shopping
  collection icons in Power Bookmarks).
- Badge icons are 12px and inherit the badge foreground color (e.g.
  `notifications_active` for price tracking).
- Trailing action: 16px `more_vert` inside a 24px `cr-icon-button`.

______________________________________________________________________

### Verification notes

- Checked: `cr_url_list_item.{ts,html.ts,css}`, `sp_list_item_badge.{ts,css}`,
  `power_bookmark_row_item.html.ts` (usage), `material_ui_color_mixer.cc`,
  `material_side_panel_color_mixer.cc`, `sys_color_mixer.cc`,
  `cr_shared_vars.css`.
- Figma design context fetched for `280:28500`, `3606:19405`, `280:28498`,
  `280:28517`, `280:28497`, `280:28507`, `3606:19407` (all Light theme). Other
  variants were inferred from the frame screenshot.
- Not checked:
  - Views analogues (`HoverButton`, `RichHoverButton`)
  - `cr-icon-button` icon color resolution
  - Actual rendered line-heights in side-panel hosts
  - Folder SVG glyph shapes
  - Dark-mode token values
  - Other consumers' badge implementations (history embeddings, organizer panel,
    contextual tasks)
