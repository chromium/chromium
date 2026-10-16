# Bottom Sheet Spec (Clank)

This document maps the specifications of the **Bottom Sheet** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Bottom Sheet** is a dismissable, elevated surface anchored to the bottom
of the screen. It holds supplementary content (action sheets and informational
sheets) while keeping the underlying page visible. Figma models it as a
`Bottom-sheet` container (optional header plus an instance-swap content slot)
and a `Bottom-sheet-header` component set with three header types
(`Preview Card`, `Contextual Search`, `Contextual Definition`), plus a shared
`.Drag-handle`. In code the container is the `BottomSheetController` /
`BottomSheetContent` framework. The `Preview Card` header matches the Ephemeral
Tab toolbar. The two `Contextual *` headers match the compositor-drawn
Contextual Search `OverlayPanel` bar, which isn't a `BottomSheet`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Bottom-sheet`<br>`Bottom-sheet-header`<br>`.Drag-handle`                                                                                                                                                                                                                                                                                                                                                                                                                                              | `BottomSheetController`<br>`BottomSheetContent`<br>`EphemeralTabSheetContent`<br>`ContextualSearchPanel`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `Bottom-sheet` (`36416:17791`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=36416-17791&m=dev)<br>[Figma Link: `Bottom-sheet-header` (`32019:16940`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32019-16940&m=dev)<br>[Figma Link: `.Drag-handle` (`62613:28692`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=62613-28692&m=dev) | [components/browser_ui/bottomsheet/android/java/src/org/chromium/components/browser_ui/bottomsheet/BottomSheetController.java](//src/components/browser_ui/bottomsheet/android/java/src/org/chromium/components/browser_ui/bottomsheet/BottomSheetController.java)<br>[components/browser_ui/bottomsheet/android/java/src/org/chromium/components/browser_ui/bottomsheet/BottomSheetContent.java](//src/components/browser_ui/bottomsheet/android/java/src/org/chromium/components/browser_ui/bottomsheet/BottomSheetContent.java)<br>[chrome/browser/ui/android/ephemeraltab/java/src/org/chromium/chrome/browser/ephemeraltab/EphemeralTabSheetContent.java](//src/chrome/browser/ui/android/ephemeraltab/java/src/org/chromium/chrome/browser/ephemeraltab/EphemeralTabSheetContent.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/compositor/overlay_panel/contextualsearch/ContextualSearchPanel.java](//src/chrome/android/java/src/org/chromium/chrome/browser/compositor/overlay_panel/contextualsearch/ContextualSearchPanel.java) |

> Source page:
> [`↳ Bottom Sheets` (`6380:51126`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6380-51126).
> This is a documentation page. It has a guidance frame (`43229:27361`) with
> Usage, Anatomy and Behavior sections, and a showcase frame (`43180:17780`)
> with light and dark examples ("Preview card", "Contextual search", "Contextual
> definition", "Bottom sheet base"). It only holds instances. The main
> components live on the `↳ Core components` page. A local
> `.local-New Tab Group` instance (`77255:7864`) on the page is an example, not
> part of the component.
>
> Supporting implementation files (not primary components): `BottomSheetView` /
> `BottomSheetCoordinator` (internal:
> `components/browser_ui/bottomsheet/android/internal/.../`),
> `@layout/bottom_sheet`, `@drawable/bottom_sheet_background`,
> `@drawable/drag_handlebar`, `@drawable/top_round_shadow` (9-patch), and
> `@layout/sheet_tab_toolbar`
> (`chrome/browser/ui/android/toolbar/java/res/layout/sheet_tab_toolbar.xml`)
> for the Preview Card header. The Contextual Search bar is drawn by
> `OverlayPanelBase` / `ContextualSearchSceneLayer` and
> `chrome/browser/android/compositor/layer/overlay_panel_layer.cc`.
>
> Related Figma components on the same `Core components` page that this spec
> doesn't cover: `NTP-Bottom-sheets` (`7116:53665`), `Sheets` (`7280:625`) and,
> on the Sys UI page, `Sheet.Share` (`40383:6291`).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Bottom-sheet` (single `COMPONENT`, no variant properties): `Header` (BOOLEAN,
  default true) × `Swap Placeholder` (INSTANCE_SWAP, default `Placeholder`
  `35692:11268`, a 380×81 dashed content slot). The header is a nested
  `Bottom-sheet-header` set to `Type=Preview Card`. The container has a 24px
  bottom padding.
- `Bottom-sheet-header` (`COMPONENT_SET`): `Type` (Preview Card | Contextual
  Search | Contextual Definition; default Preview Card) × `Handle` (BOOLEAN,
  true) × `Header` (BOOLEAN, true) × `Actions` (BOOLEAN, false) ×
  `Open Externally Icon` (BOOLEAN, true) × `Close Icon` (BOOLEAN, true) × `Url`
  (BOOLEAN, false). `Actions`, `Open Externally Icon` and `Close Icon` only
  affect `Preview Card`. `Url` only affects `Contextual Search`.
- `.Drag-handle` (single `COMPONENT`, no properties): 36×4 pill.
- Light and dark come from the token mode, not from a property.

| Feature / Variant                              | Figma Component                                                                                         | Clank (Android)                                                                                                                                                                                                                                                             |
| :--------------------------------------------- | :------------------------------------------------------------------------------------------------------ | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Sheet container (base)**                     | `Bottom-sheet`                                                                                          | `BottomSheetController#requestShowContent(BottomSheetContent, boolean)`; view is `BottomSheetView` (`@layout/bottom_sheet`)                                                                                                                                                 |
| **Content slot**                               | `Swap Placeholder`                                                                                      | `BottomSheetContent#getContentView()` (put into `@id/bottom_sheet_content`)                                                                                                                                                                                                 |
| **Header slot**                                | `Header=true` (nested `Bottom-sheet-header`)                                                            | `BottomSheetContent#getToolbarView()` (put into `@id/bottom_sheet_toolbar_container`). It's optional, so content can return `null`                                                                                                                                          |
| **Drag handle**                                | `Handle=true` / `.Drag-handle`                                                                          | Sheet-level: `BottomSheetContent#showHandlebar()` turns on `@id/handlebar` (`@drawable/drag_handlebar`). Many features draw their own handle in their own layout, e.g. `@id/drag_handlebar` in `sheet_tab_toolbar.xml`, or an `ImageView` tinted `@macro/drag_handle_color` |
| **Preview Card header**                        | `Type=Preview Card` (favicon, title, origin)                                                            | `EphemeralTabSheetContent` toolbar (`@layout/sheet_tab_toolbar`): `@id/favicon`, `@id/title`, `@id/origin`                                                                                                                                                                  |
| **Preview Card: Url / security row**           | Always shown in `Preview Card` (`lock` 16px + `website.com`)                                            | `@id/security_icon` + `@id/origin` (`EphemeralTabSheetContent#updateURL` / `setSecurityIcon`)                                                                                                                                                                               |
| **Preview Card: actions**                      | `Actions=true` + `Open Externally Icon` / `Close Icon` (48px targets)                                   | `@id/open_in_new_tab` (`GONE` by default, shown with `showOpenInNewTabButton`) and `@id/close` (always shown)                                                                                                                                                               |
| **Contextual Search header**                   | `Type=Contextual Search` (Google G in a 36px slot, search term, "Tap to see search results")            | `ContextualSearchPanel` bar: `ContextualSearchBarControl` with `contextual_search_term_view` / `contextual_search_context_view` + `contextual_search_caption_view`; Google G is `R.drawable.ic_logo_googleg_24dp`                                                           |
| **Contextual Search: Url row**                 | `Url=true` (lock + URL)                                                                                 | *(No equivalent found in the Contextual Search bar)*                                                                                                                                                                                                                        |
| **Contextual Definition header**               | `Type=Contextual Definition` (36px `primary` circle with `book` icon, term · pronunciation, definition) | `ContextualSearchPanel` card/quick action icon (`ContextualSearchImageControl`, `contextual_search_card_icon_view`, 36dp); term + caption text views                                                                                                                        |
| **Elevation / shadow**                         | `Current/bottomSheets` effect (key + ambient drop shadows)                                              | `@drawable/top_round_shadow` 9-patch in `ShadowLayerView` (`bottom_sheet_shadow_length` 16dp, `_large` 28dp); `BottomSheetContent#getSheetBackgroundGlowSpecOverride()`                                                                                                     |
| **Modal vs non-modal background**              | *(Not modeled; one `bottom_sheet_bg_color` token)*                                                      | `BottomSheetCoordinator#updateBackgroundColor()` blends between non-modal (`?attr/colorSurface`) and modal/scrim (`@color/bottom_sheet_bg_color`) by sheet offset; `getSheetBackgroundColorOverride()`                                                                      |
| **Sheet states (peek / half / full / hidden)** | *(Not modeled as properties; covered in the Behavior guidance)*                                         | `BottomSheetController.SheetState`; `BottomSheetContent#getPeekHeight()`, `getHalfHeightRatio()`, `getFullHeightRatio()`, `swipeToDismissEnabled()`                                                                                                                         |
| **Large form factor / desktop popup**          | *(Not modeled; the 412px mobile width only)*                                                            | `SheetLayoutMode` (`bottom_sheet_large_form_factor_width` 480dp, `@layout/bottom_sheet_desktop`)                                                                                                                                                                            |
| **Dark theme**                                 | Dark showcase (token mode switch)                                                                       | Night resources: `color-night/bottom_sheet_bg_color.xml`, `color-night/divider_color.xml`                                                                                                                                                                                   |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                                    | Figma Design Token                                                                                                  | Clank (Android)                                                                                                                                                                                    |
| :-------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Sheet background (light)**                        | `clank/macro/bottom_sheets/bottom_sheet_bg_color` = `white`                                                         | `@color/bottom_sheet_bg_color` → `?attr/colorSurface` (white) ✅. Non-modal: `SemanticColorUtils.getColorSurface()` ✅                                                                             |
| **Sheet background (dark)**                         | `clank/macro/bottom_sheets/bottom_sheet_bg_color` = `#282a2c`                                                       | Modal: `color-night/bottom_sheet_bg_color` → `?attr/colorSurfaceContainerHigh` (`#282a2c`) ✅. Non-modal: `?attr/colorSurface` ⚠️ (darker, see §4)                                                 |
| **Static `@drawable/bottom_sheet_background` fill** | n/a                                                                                                                 | `@macro/default_bg_color` (`?attr/colorSurface`), tinted at runtime by `BottomSheetView#setSheetBackgroundColor`                                                                                   |
| **Contextual Search bar background**                | `bottom_sheet_bg_color` (white / `#282a2c`)                                                                         | `@macro/overlay_panel_bar_background_color` → `@macro/default_bg_color` (`?attr/colorSurface`) ⚠️ in dark                                                                                          |
| **Top corner radius**                               | `clank/shape/corner_radius/16` (16px, top-left/right)                                                               | `@dimen/bottom_sheet_corner_radius` = 16dp ✅ (`@drawable/bottom_sheet_background`)                                                                                                                |
| **Elevation / shadow**                              | Key: `0 0 13px 0 rgba(37,39,43,0.17)`; Ambient: `0 0 3px 0 rgba(37,39,43,0.12)` (`clank/elevation/bottom_sheets/*`) | `@drawable/top_round_shadow` 9-patch, `bottom_sheet_shadow_length` 16dp. *(Not checked: the 9-patch's alpha and blur can't be compared numerically to the Figma values)*                           |
| **Drag handle size**                                | 36×4px, radius 2px                                                                                                  | `@drawable/drag_handlebar`: **40dp**×4dp, radius 2dp ⚠️                                                                                                                                            |
| **Drag handle color (light)**                       | `clank/macro/drag_handlebar/drag_handle_color` = `#dde3ea`                                                          | `@macro/drag_handle_color` → `@color/divider_color` → `?attr/colorSurfaceContainerHighest` ✅ (sheet-level handle in `drag_handlebar.xml` uses `@color/divider_color` directly)                    |
| **Drag handle color (dark)**                        | `drag_handle_color` = `#444746`                                                                                     | `color-night/divider_color` → `?attr/colorOutlineVariant` (`#444746`) ✅                                                                                                                           |
| **Handle area padding**                             | `pt` = `clank/shape/spacing/8` (8px), `px` 8px                                                                      | Sheet-level `@id/handlebar`: `paddingTop="6dp"` ⚠️. Preview Card toolbar: 4dp spacer + 4dp `marginTop` = 8dp ✅. Desktop popup: `bottom_sheet_handlebar_padding_vertical_desktop` 8dp ✅           |
| **Preview Card headline padding**                   | `pt` 6px, `pb` 8px, `px` 16px (`spacing/6`, `/8`, `/16`), gap 16px, height 54px                                     | `sheet_tab_toolbar`: `paddingStart/End` 16dp ✅, favicon `marginEnd` 16dp ✅, `minHeight` 62dp + 2dp spacer + 2dp progress bar (*total height not compared exactly*)                               |
| **Preview Card favicon**                            | 24px (`globe` fallback, `onsurface`)                                                                                | `@dimen/ephemeral_tab_favicon_size` 24dp ✅; fallback `R.drawable.ic_globe_24dp` tinted `@color/default_icon_color_tint_list` (`colorOnSurface`) ✅                                                |
| **Title text**                                      | `textMedium` 14/20, regular, `clank/sys/color/onsurface` (`#1f1f1f` / `#e3e3e3`)                                    | `@style/TextAppearance.TextMedium.Primary` (14sp / 20sp leading, `@color/default_text_color_list` → `colorOnSurface`) ✅                                                                           |
| **Origin / URL text**                               | `textMedium` 14/20, `clank/sys/color/onsurfacevariant` (`#444746` / `#c4c7c5`)                                      | `@style/TextAppearance.TextMedium.Secondary` (`default_text_color_secondary` → `colorOnSurfaceVariant`) ✅. Ellipsizes at the **start** in code, at the end in Figma                               |
| **Security (lock) icon**                            | 16px, `onsurfacevariant`                                                                                            | `@id/security_icon` `minHeight` 16dp, tinted `@macro/default_icon_color` (`colorOnSurface`) ⚠️                                                                                                     |
| **Action icons (open in new / close)**              | 24px glyph in 48px target, `onsurfacevariant`                                                                       | 48×48dp `ImageView` with 12dp padding ✅; tint `@macro/default_icon_color` (`colorOnSurface`) ⚠️                                                                                                   |
| **Contextual header height**                        | 8 + 4 + 58 = 70px                                                                                                   | `@dimen/overlay_panel_bar_height` 70dp ✅                                                                                                                                                          |
| **Contextual headline padding**                     | `px` 12px, gap 12px, `pt` 6px, `pb` 8px                                                                             | `overlay_panel_button_padding` 12dp; `contextual_search_callout_icon_margin` 12dp; text margins 7dp, `OverlayPanelTextViewLayout` `paddingStart` 53dp. *(Composite layout; not checked px for px)* |
| **Contextual leading icon**                         | 36px slot (`Google Super G` 24px + 6px padding; or 36px `primary` circle + 20px `book` in `onprimary`)              | `@dimen/contextual_search_bar_image_size` 36dp ✅, `contextual_search_callout_icon_size` 36dp ✅; Google G `ic_logo_googleg_24dp` ✅. *(Definition circle/icon colors not checked)*                |
| **Contextual primary text**                         | `textLarge` 16/24, `onsurface`                                                                                      | `@style/OverlayPanelTextView` → `TextAppearance.TextLarge.Primary` (16sp / 24sp) ✅                                                                                                                |
| **Contextual caption / definition text**            | `textMedium` 14/20, `onsurfacevariant`                                                                              | `@style/ContextualSearchCaptionTextView` → `TextAppearance.TextMedium.Secondary` ✅                                                                                                                |
| **Contextual Search URL text color**                | Hard-coded `#444746` (no token)                                                                                     | n/a. Figma issue, see §5                                                                                                                                                                           |
| **Font family**                                     | `clank/font/font_family/alternative` (Google Sans Text)                                                             | Comes from the app theme's text appearance. *(Not checked per view)*                                                                                                                               |
| **Sheet width**                                     | 412px (mobile frame)                                                                                                | Full width on phones; `bottom_sheet_narrow_width` 600dp / `bottom_sheet_large_form_factor_width` 480dp on larger screens                                                                           |
| **Bottom content padding**                          | `clank/shape/spacing/24` (24px) on `Bottom-sheet`                                                                   | Each feature's content layout sets its own. *(No framework-level equivalent)*                                                                                                                      |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

- **Two separate systems for one Figma family.** `Preview Card` and the base
  `Bottom-sheet` run on the View-based `BottomSheetController` framework.
  `Contextual Search` and `Contextual Definition` are drawn in the compositor by
  `OverlayPanel` / `ContextualSearchPanel` (cc layers in
  `overlay_panel_layer.cc`, with Android views captured as resources). Token
  changes have to land in both places (e.g. `drag_handlebar`, `top_round_shadow`
  and `top_round_foreground` are shared drawables, but colors come from separate
  semantic utils).
- **Header is feature-owned, not a reusable widget.** Figma ships one
  `Bottom-sheet-header` component. Code has no shared header view: each
  `BottomSheetContent` returns its own `getToolbarView()` (Ephemeral Tab uses
  `sheet_tab_toolbar.xml`, many others inline their own handle and title).
- **Drag handle duplication.** The framework handle
  (`BottomSheetContent#showHandlebar()`) is opt-in and off by default. Many
  features add their own `ImageView` with `@drawable/drag_handlebar` tinted
  `@macro/drag_handle_color`. Spacing around the handle varies by feature:
  framework 6dp top, Ephemeral Tab 8dp, desktop 8dp.
- **Background color depends on modality.** Figma has one
  `bottom_sheet_bg_color` token. Code blends the color at runtime between
  non-modal `colorSurface` and modal `bottom_sheet_bg_color`
  (`colorSurfaceContainerHigh` in dark) based on sheet offset. So the Figma dark
  value (`#282a2c`) only matches a modal or scrimmed sheet. Content can also
  override the color (`getSheetBackgroundColorOverride`).
- **Shadow is a 9-patch, not a two-layer elevation.** Figma uses key + ambient
  drop shadows. Code uses `top_round_shadow` with a fixed 16dp (or 28dp) length
  and an optional `GlowSpec` tint.
- **States not in Figma.** Peek, half, full, hidden, scrim, swipe-to-dismiss,
  back-press handling, the keyboard curtain, large form factor and desktop popup
  layouts all exist in code but aren't modeled in the Figma component.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **Drag handle width:** Figma `.Drag-handle` is **36px** wide.
  `@drawable/drag_handlebar` is **40dp**. Possible code/design drift; confirm
  which is right.
- **Framework handle top padding:** Figma 8px vs `@layout/bottom_sheet`
  `@id/handlebar` `paddingTop="6dp"`.
- **Icon tint (possible code bug):** In the Preview Card, Figma uses
  `onsurfacevariant` for the lock, `open_in_new` and `close` icons.
  `sheet_tab_toolbar.xml` tints `@id/security_icon`, `@id/close` and
  `@id/open_in_new_tab` with `@macro/default_icon_color`, which resolves to
  `?attr/colorOnSurface`. Consider `@macro/default_icon_color_secondary`
  (`?attr/colorOnSurfaceVariant`).
- **Close button always shown:** Figma defaults `Actions=false` (no action
  icons). Ephemeral Tab always shows `@id/close`, and shows `open_in_new_tab`
  only when the tab can be promoted.
- **Url row in Preview Card:** Figma lists `Url` as a header boolean, but it
  only toggles content in `Contextual Search`. The Preview Card always shows
  lock + origin. Code matches the Preview Card. `Contextual Search` with
  `Url=true` has no code counterpart that I found.
- **Origin ellipsis:** Code ellipsizes the origin at the start
  (`android:ellipsize="start"`) so the domain end stays visible. Figma truncates
  at the end.
- **Dark-mode background for Contextual Search and non-modal sheets:** Figma
  `#282a2c` vs code `colorSurface` (`overlay_panel_bar_background_color` and
  non-modal sheets).
- **Figma hygiene:** In the `Contextual Search` variant, the URL text uses a
  hard-coded `#444746` instead of `clank/sys/color/onsurfacevariant`, so it
  won't switch in dark mode. The text layer is named "Defintion" (typo) in
  `Contextual Definition`.
- **Progress bar and fading shadow:** `sheet_tab_toolbar.xml` adds a 2dp
  `ProgressBar` and a `FadingShadowView` under the header. Neither is in the
  Figma `Preview Card` header.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Per the Figma guidance, bottom sheets are dismissable, elevated surfaces
  anchored to the bottom of the screen, used for **supplementary and
  contextual** content. They keep the underlying context visible.
- Two kinds in Clank: **action sheets** (an action or list of actions relevant
  to the user's context) and **informational sheets** (the user can see the
  sheet content and the web content behind it at the same time).
- Don't use a bottom sheet when the user must complete a critical action. Use a
  dialog instead (see [Dialog](./Dialog_Spec_Clank.md)). Bottom sheets are meant
  to interrupt less than dialogs.
- Use `Preview Card` for previewing a page (Ephemeral Tab), and the
  `Contextual *` headers only for Touch-to-Search (Contextual Search).
- Show content through `BottomSheetController#requestShowContent` with a
  `BottomSheetContent`. Don't build ad-hoc bottom-anchored views. Use
  `getPriority()` and `canBeSuppressed()` to arbitrate between sheets.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Provide sheet state announcements through `getSheetContentDescription`,
  `getSheetHalfHeightAccessibilityStringId`,
  `getSheetFullHeightAccessibilityStringId` and
  `getSheetClosedAccessibilityStringId`.
- The drag handle should be focusable and clickable with a content description
  (e.g. `@string/accessibility_custom_tab_drag_handle` in `sheet_tab_toolbar`)
  so TalkBack users can expand or collapse the sheet without dragging.
- Header action buttons need a 48dp minimum touch target
  (`@dimen/min_touch_target_size`) and a `contentDescription` (`@string/close`,
  `@string/contextmenu_open_in_new_tab`).
- Support back-press dismissal (`handleBackPress` /
  `getBackPressStateChangedSupplier`) and swipe-to-dismiss where it fits.

### 3. Icon Usage Guidelines

- Leading icon: 24px favicon (fallback `globe`) for Preview Card. A 36px slot
  for Contextual headers (Google G, or a `primary` filled circle with a 20px
  `onprimary` glyph for definitions).
- Trailing actions: 24px Google Symbols (`open_in_new`, `close`) in 48px
  targets, tinted `onsurfacevariant`.
- Security indicator: 16px icon (lock / info / warning from
  `EphemeralTabMediator`) before the origin text.
