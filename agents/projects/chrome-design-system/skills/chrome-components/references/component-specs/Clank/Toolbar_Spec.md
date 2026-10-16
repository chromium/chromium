# Toolbar Spec (Clank)

This document maps the specifications of the **Toolbar** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Toolbar** is Chrome's top browser toolbar. It holds the omnibox (location
bar) and the main browser actions: home, back/forward/refresh on tablet, new tab
("magic"/adaptive button), tab switcher and the app menu. Figma models it as two
component sets: `Toolbar-mobile` (412×56, with page-context variants such as
Default, Incognito, NTP, CCT, Tab switcher) and `Toolbar-tablet` (1280×56,
`Tablet` and `AL` (Android desktop) variants). In code these map to
`ToolbarPhone` and `ToolbarTablet` (both subclasses of `ToolbarLayout`). The
CCT, Tab switcher and Bookmarks variants are separate surfaces in code
(`CustomTabToolbar`, `HubToolbarView`, `SelectableListToolbar`).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                    | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Toolbar-mobile`<br>`Toolbar-tablet`                                                                                                                                                                                                                                                                                               | `ToolbarPhone`<br>`ToolbarTablet`<br>`ToolbarLayout`<br>`CustomTabToolbar`<br>`HubToolbarView`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Source Files**   | [Figma Link: `Toolbar-mobile` (`40904:13291`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=40904-13291&m=dev)<br>[Figma Link: `Toolbar-tablet` (`80761:54035`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=80761-54035&m=dev) | [chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarPhone.java](//src/chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarPhone.java)<br>[chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarTablet.java](//src/chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarTablet.java)<br>[chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarLayout.java](//src/chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarLayout.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/customtabs/features/toolbar/CustomTabToolbar.java](//src/chrome/android/java/src/org/chromium/chrome/browser/customtabs/features/toolbar/CustomTabToolbar.java)<br>[chrome/browser/hub/internal/android/java/src/org/chromium/chrome/browser/hub/HubToolbarView.java](//src/chrome/browser/hub/internal/android/java/src/org/chromium/chrome/browser/hub/HubToolbarView.java) |

> Source page:
> [`↳ Toolbars` (`6371:36076`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-36076).
> It holds only instances: an overview card (`40052:23902`), a mobile showcase
> (`43200:1771`, 36 `Toolbar-mobile` instances) and a tablet showcase
> (`80113:6106`, 4 `Toolbar-tablet` instances). Both component sets live on the
> `↳ Core components` page.
>
> Related nested Figma components (not covered here as separate specs):
> `Omnibox` (`42437:1523`), `Omnibox - Tablet` (`31543:37821`),
> `Contextual page` (`46169:34000`), `.Tab-switcher` (`54577:12508`), `.NTB`
> (`54577:18501`), `Icons-Local` (`54642:8913`), `Divider` (`54738:67712`). In
> code the omnibox is `LocationBarPhone` / `LocationBarTablet`
> (`chrome/browser/ui/android/omnibox/`).
>
> Layouts:
> [toolbar_phone.xml](//src/chrome/browser/ui/android/toolbar/java/res/layout/toolbar_phone.xml),
> [toolbar_tablet.xml](//src/chrome/browser/ui/android/toolbar/java/res/layout/toolbar_tablet.xml),
> [hub_toolbar_layout.xml](//src/chrome/browser/hub/internal/android/res/layout/hub_toolbar_layout.xml).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- **`Toolbar-mobile`**: `Toolbar Type` (CCT | Default | NTP | NTP Scrolled |
  Search | Malicious | Permissions | Bookmarks | Tab switcher | DEPRECATED |
  Publisher defined | Incognito) × `Theme` (Default | Agnostic). Only 12
  combinations exist (`Incognito` is only paired with `Agnostic`; every other
  type uses `Default`). Non-variant properties: `RSH icons` (boolean, default
  true), `LSH icons` (boolean, true), `Contextual Action` (boolean, false),
  `Magic button` (boolean, true), `Magic icon` / `Magic icon agnostic` (instance
  swap), `Bottom Divider` (boolean, true).
- **`Toolbar-tablet`**: `Device` (Tablet | AL). Non-variant properties:
  `Bottom Divider`, `Avatar`, `Right Icons`, `Left Icons` (all boolean, default
  true).

| Feature / Variant                     | Figma Component                                                         | Clank (Android)                                                                                                                                                                               |
| :------------------------------------ | :---------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Default (phone)**                   | `Toolbar-mobile` · `Toolbar Type=Default, Theme=Default` (`41423:4719`) | `ToolbarPhone` (`toolbar_phone.xml`) with `LocationBarPhone`                                                                                                                                  |
| **Incognito**                         | `Toolbar Type=Incognito, Theme=Agnostic` (`7214:51558`)                 | `ToolbarPhone` in incognito branding (`isIncognitoBranded()`); colors from `ChromeColors.getDefaultThemeColor(ctx, true)` and `ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage` |
| **Publisher defined**                 | `Toolbar Type=Publisher defined` (`7214:51564`)                         | `ToolbarPhone` with a site theme color (`getToolbarDataProvider().getPrimaryColor()`); text box color derived in `ThemeUtils`                                                                 |
| **Malicious**                         | `Toolbar Type=Malicious` (`7214:51570`)                                 | ⚠️ No matching red toolbar treatment found. `LocationBarModel.getMaliciousContentStatus()` exists but no toolbar background swap was found in `toolbar/`                                      |
| **Permissions**                       | `Toolbar Type=Permissions` (`43133:33921`)                              | Status icon in `LocationBarPhone` (`location_on` permission icon); not a separate toolbar mode                                                                                                |
| **NTP**                               | `Toolbar Type=NTP` (`7214:51585`)                                       | `ToolbarPhone` NTP state (location bar hidden until scrolled; `toolbar_edge_padding_ntp`)                                                                                                     |
| **NTP Scrolled**                      | `Toolbar Type=NTP Scrolled` (`32198:10908`)                             | `ToolbarPhone` NTP scroll/expansion (`mLocationBarBackgroundColorForNtp`, fakebox → omnibox animation)                                                                                        |
| **Search**                            | `Toolbar Type=Search` (`32199:15173`)                                   | Focused omnibox state (`urlHasFocus()`) in `ToolbarPhone`                                                                                                                                     |
| **CCT**                               | `Toolbar Type=CCT` (`43133:33278`)                                      | `CustomTabToolbar` (`new_custom_tab_toolbar.xml`)                                                                                                                                             |
| **Tab switcher**                      | `Toolbar Type=Tab switcher` (`41896:11924`)                             | `HubToolbarView` (`hub_toolbar_layout.xml`: action button, `pane_switcher_card`, menu)                                                                                                        |
| **Bookmarks**                         | `Toolbar Type=Bookmarks` (`43133:34071`)                                | `SelectableListToolbar` (see [Top App Bar](./TopAppBar_Spec_Clank.md))                                                                                                                        |
| **DEPRECATED**                        | `Toolbar Type=DEPRECATED` (`7214:51597`)                                | N/A (deprecated in Figma)                                                                                                                                                                     |
| **Tablet**                            | `Toolbar-tablet` · `Device=Tablet` (`29340:8664`)                       | `ToolbarTablet` (`toolbar_tablet.xml`) with `LocationBarTablet`                                                                                                                               |
| **AL (Android desktop)**              | `Device=AL` (`80761:54036`)                                             | `ToolbarTablet` with `ToolbarHoverableButton.AdaptiveDensity` + `*_desktop` dimens and `extensions_toolbar_container_stub`                                                                    |
| **Home button**                       | `LSH icons` / `Left Icons` (`home`)                                     | `HomeButton` (`@id/home_button`)                                                                                                                                                              |
| **Back / Forward / Refresh (tablet)** | `LHS` › `back`, `forward`, `refresh`                                    | `@id/back_button`, `@id/forward_button`, `@id/refresh_button` (`ChromeImageButton`)                                                                                                           |
| **Magic button (new tab / adaptive)** | `Magic button` + `Magic icon` swap (`add`)                              | Optional/adaptive button (`@id/optional_button_stub`, `OptionalButtonView`)                                                                                                                   |
| **Contextual action**                 | `Contextual Action` (`Contextual page`, `mobile_text_2`)                | ⚠️ Not verified; likely page-action/contextual chip in the location bar                                                                                                                       |
| **MTB (tablet)**                      | `RHS` › `MTB` (`g_translate`)                                           | Optional button on tablet (`@id/optional_button_stub`)                                                                                                                                        |
| **Tab switcher button**               | `Icons-Local` (count)                                                   | `ToggleTabStackButton` (`@id/tab_switcher_button`)                                                                                                                                            |
| **App menu**                          | `more_vert`                                                             | `menu_button.xml` (`MenuButton`)                                                                                                                                                              |
| **Avatar / identity disc**            | `Avatar` (tablet), `Identity disk` (NTP)                                | `@id/signin_button_stub` / identity disc (`toolbar_identity_disc_size` 24dp)                                                                                                                  |
| **Extensions (AL)**                   | `RHS` › `extensions` (`chrome_extension`)                               | `@id/extensions_toolbar_container_stub`                                                                                                                                                       |
| **Bottom divider**                    | `Bottom Divider`                                                        | Toolbar hairline (`@id/toolbar_hairline`, `ToolbarLayout.getToolbarHairlineColor()`)                                                                                                          |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                     | Figma Design Token                                                                                  | Clank (Android)                                                                                                                                                                                            |
| :----------------------------------- | :-------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Toolbar background (default)**     | `--clank/sys/color/surface` (white)                                                                 | `ChromeColors.getDefaultThemeColor(ctx, false)` → `?attr/colorSurface`; tablet XML uses `@macro/default_bg_color` (`?attr/colorSurface`) ✅                                                                |
| **Toolbar background (incognito)**   | `--clank/sys/color/surface` dark (#131314)                                                          | `@color/toolbar_background_incognito` → `gm3_baseline_surface_dark` ✅                                                                                                                                     |
| **Toolbar background (malicious)**   | `--clank/macro/toolbar_mobile/toolbar_malicious_bg_color` (#B3261E)                                 | ⚠️ No equivalent found                                                                                                                                                                                     |
| **Omnibox background (phone)**       | `--clank/macro/omnibox/omnibox_bg_color` (#E9EEF6)                                                  | `@color/toolbar_text_box_bg_color` → `?attr/colorSurfaceContainerHigh` (night: `colorSurfaceContainerHighest`) ✅                                                                                          |
| **Omnibox background (tablet)**      | `--clank/sys/color/scontainerhigh` (#E9EEF6)                                                        | `@drawable/modern_toolbar_tablet_text_box_background` → `toolbar_text_box_bg_color` ✅ (Figma uses a different token name than phone)                                                                      |
| **Omnibox background (incognito)**   | `--clank/sys/color/scontainerhighest` dark (#333537)                                                | `@color/toolbar_text_box_background_incognito` → `gm3_baseline_surface_container_highest_dark` ✅                                                                                                          |
| **Omnibox background (malicious)**   | `--clank/sys/state_layer/surface_20%` (white @ 20%)                                                 | Not checked (no malicious mode found)                                                                                                                                                                      |
| **Omnibox height**                   | 40px                                                                                                | `@dimen/modern_toolbar_background_size` 40dp (phone); tablet holder `modern_toolbar_tablet_background_size` 48dp with 4dp top/bottom inset (`modern_toolbar_background_vertical_offset`) → 40dp visible ✅ |
| **Omnibox corner radius**            | `--clank/shape/corner_radius/100` (full pill)                                                       | `@dimen/modern_toolbar_background_corner_radius` 24dp (fully rounds 40/48dp) ✅                                                                                                                            |
| **Omnibox inner padding**            | `--clank/shape/spacing/8` (incognito: 10 left / 12 right); icon-text gap `spacing/4`                | `location_bar_icon_starting_padding` 8dp, `location_bar_icon_margin_end` 4dp, `url_bar_end_padding` 8dp — partially checked                                                                                |
| **URL text**                         | `textLarge`: `alternative` font (Google Sans Text), 16 / 24, regular, `--clank/sys/color/onsurface` | `@dimen/location_bar_url_text_size` 16sp ✅; font family/line height not checked                                                                                                                           |
| **Icon color**                       | `--clank/sys/color/onsurface` (#1F1F1F; dark #E3E3E3; malicious `static/white`)                     | `@color/default_icon_color_tint_list`; themed via `ThemeUtils.getThemedToolbarIconTint` ✅ (exact color not checked)                                                                                       |
| **Icon size**                        | 24px (omnibox status icon glyph 20px)                                                               | 24dp drawables; `location_bar_status_icon_width` 24dp (phone) / 48dp (tablet)                                                                                                                              |
| **Toolbar height**                   | 56px                                                                                                | `@dimen/toolbar_height_no_shadow` → `default_action_bar_height` (56dp) ✅                                                                                                                                  |
| **Phone horizontal padding**         | `--clank/shape/spacing/12` start/end, `spacing/8` top/bottom                                        | `toolbar_edge_padding` 8dp (`OmniboxResourceProvider.getToolbarSidePadding`) ⚠️                                                                                                                            |
| **Phone NTP padding**                | `spacing/16` / `spacing/24`                                                                         | `toolbar_edge_padding_ntp` 16dp ✅ (24 not checked)                                                                                                                                                        |
| **Trailing icon spacing**            | 24px icons, `--clank/shape/spacing/16` gap (40px pitch)                                             | `ToolbarHoverableButton`: `toolbar_button_width` 48dp × 56dp (48dp pitch) ⚠️                                                                                                                               |
| **Tablet horizontal padding**        | `--clank/shape/spacing/4`                                                                           | `@dimen/tablet_toolbar_start_padding` 4dp ✅                                                                                                                                                               |
| **Tablet button size**               | 48×48 slots                                                                                         | `?attr/toolbarButtonWidth/Height` (48dp; `*_desktop` → `large_icon_background_size` on AL) ✅                                                                                                              |
| **Divider**                          | `--clank/macro/divider/divider_color` (#DDE3EA), 1px                                                | `SemanticColorUtils.getDividerColor` (incognito: `divider_color_light`); themed: overlay of `toolbar_hairline_overlay_*`                                                                                   |
| **Tab switcher: pane switcher card** | `--clank/sys/color/scontainer`, radius `corner_radius/16`, max 48px tall, `spacing/4`               | `hub_pane_switcher_background.xml` 16dp radius (hardcoded), `hub_pane_switcher_card_height` 48dp, `hub_pane_switcher_vertical_padding` 4dp ✅ (color not checked)                                          |
| **Tab switcher: selected tab**       | `--clank/sys/color/sbright`, radius `corner_radius/12`, 40px                                        | `hub_pane_switcher_tab_radius` 12dp, `hub_pane_switcher_tab_size` 40dp ✅ (color not checked)                                                                                                              |
| **Tab switcher: new tab button**     | `--clank/sys/color/primary` bg, `onprimary` icon, radius 12, 40px                                   | `hub_toolbar_action_button_size` 40dp ✅ (color/radius not checked)                                                                                                                                        |
| **CCT title / URL**                  | `textLarge` onsurface / `textSmall` (16 line height) `onsurfacevariant`                             | Not checked                                                                                                                                                                                                |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

- **One Figma set, several code surfaces.** `Toolbar-mobile` combines the
  browsing toolbar (`ToolbarPhone`), Custom Tabs (`CustomTabToolbar`), the Hub
  tab switcher (`HubToolbarView`) and Bookmarks (`SelectableListToolbar`) as
  values of one `Toolbar Type` axis. Code has no shared "type" enum. Most of the
  browsing types (Default, Incognito, Publisher defined, NTP, NTP Scrolled,
  Search, Permissions) are runtime states of `ToolbarPhone` +
  `LocationBarPhone`, not separate styles.
- **Dynamic vs static color.** Figma shows fixed values. Code derives toolbar
  and text-box colors at runtime (`ThemeUtils`, `ChromeColors`, NTP Aurora
  overlay `getNtpAuroraLocationBarBackgroundColor`, theme-color overlays with
  `LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA`). Publisher-defined colors in
  Figma are only examples.
- **Token naming inconsistency in Figma.** The phone omnibox uses
  `macro/omnibox/omnibox_bg_color`, the tablet omnibox uses
  `sys/color/scontainerhigh`. Both resolve to #E9EEF6 and both map to
  `toolbar_text_box_bg_color`.
- **Tablet omnibox geometry.** Code lays out a 48dp holder and insets the
  drawable 4dp top/bottom to draw 40dp. Figma draws the 40px pill directly. The
  result looks the same, but the hit area is larger in code.
- **Theme axis.** `Theme=Agnostic` is only used with `Incognito`. Code doesn't
  apply dynamic color in incognito (fixed `*_dark` baseline colors), which is
  likely what "Agnostic" means here.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- ⚠️ **Malicious toolbar.** Figma has a red (#B3261E) toolbar with a white
  omnibox at 20% opacity and white icons. No matching color resource or
  toolbar-background change was found in `chrome/browser/ui/android/toolbar/`.
  This may be a proposed design or behind a flag elsewhere. Needs confirmation.
- ⚠️ **Trailing icon pitch.** Figma places 24px icons with 16px gaps (40px
  pitch) inside 12px side padding. Code uses 48dp-wide touch targets
  (`toolbar_button_width`) with `toolbar_edge_padding` 8dp. Positions differ by
  several dp. Figma doesn't model the 48dp touch targets.
- ⚠️ **Phone side padding.** Figma uses 12px. Code uses `toolbar_edge_padding`
  8dp (the button's own padding adds to this).
- ⚠️ **Incognito divider.** Figma's `Divider` instance isn't present in the
  `Incognito` variant. Code still draws a hairline in incognito
  (`divider_color_light`), unless it's hidden elsewhere. Not verified at
  runtime.
- **Interactive states.** Figma doesn't define hover, pressed or focus states
  for toolbar buttons or the omnibox. Code uses `default_icon_background`
  (`ToolbarHoverableButton`) ripple/hover, and the omnibox has a hover highlight
  (`modern_toolbar_text_box_background_highlight`) and a focus height increase
  (`location_bar_background_on_focus_height_increase` 8dp).
- **Contextual action / magic button.** Figma exposes them as booleans and
  instance swaps. In code the adaptive button is driven by
  `OptionalBrowsingModeButtonController` and segmentation logic. It isn't a
  static slot.
- **Tab switcher.** The Figma pane switcher shows three tabs (regular,
  incognito, tab groups). Code generates pane buttons dynamically from
  registered Hub panes.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Toolbars hold navigation-critical actions (Figma overview: "quick access to
  frequently used functions within a specific context"). Keep the action set
  minimal: home, omnibox, one adaptive/contextual action, tab switcher, menu.
- Use a flat `surface` fill with a bottom divider rather than elevation.
- Use the `Incognito` variant (fixed dark palette) for incognito tabs, and don't
  apply dynamic/publisher colors there.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Every button sets `contentDescription` and `tooltipText` (e.g.
  `@string/accessibility_toolbar_btn_home`, `accessibility_toolbar_btn_back`).
- Keep the 48dp minimum touch target (`toolbar_button_width`,
  `@dimen/min_touch_target_size`) even though Figma shows 24px icon glyphs.
- On tablet/AL, buttons use `ToolbarHoverableButton.AdaptiveDensity` so hover
  states and keyboard focus work with a mouse or trackpad.

### 3. Icon Usage Guidelines

- 24px Google Symbols (outline, `FILL 0`, `ROND 50`): `home`, `arrow_back`,
  `arrow_forward`, `refresh`, `add`, `more_vert`, `chrome_extension`. The
  omnibox status icon (`page_info`, `warning`, `location_on`,
  `incognito_circle`) uses a 20px glyph.
- The tab switcher icon is a local component (`Icons-Local`) with a Roboto
  Condensed Bold 12px tab count. In code it's `ToggleTabStackButton`, which
  draws its own count.
