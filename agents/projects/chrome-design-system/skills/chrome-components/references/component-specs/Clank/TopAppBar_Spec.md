# Top App Bar Spec (Clank)

This document maps the specifications of the **Top App Bar** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **Top App Bar** shows the title, navigation and actions for the current
screen. It uses a flat `surface` fill rather than a drop shadow, and can show a
bottom divider when content scrolls under it (per the Figma guidance). Figma
models it as one 56px non-variant component with boolean slots (leading back
icon, tab group color dot, up to 4 trailing icons, dividers). Code has no single
Chromium widget for it. It's built from the Material `MaterialToolbar`
(Settings), the Chromium `SelectableListToolbar` (History, Bookmarks, Downloads,
pickers), and the custom `TabGridDialogToolbarView` (tab group dialog).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                          | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Top-app-bar`                                                                                                                                            | `MaterialToolbar` (`androidx.appcompat.widget.Toolbar`)<br>`SelectableListToolbar`<br>`TabGridDialogToolbarView`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Source Files**   | [Figma Link: `Top-app-bar` (`7119:1200`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7119-1200&m=dev) | [chrome/android/java/res/layout/settings_activity.xml](//src/chrome/android/java/res/layout/settings_activity.xml) (hosts `MaterialToolbar`)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableListToolbar.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableListToolbar.java)<br>[chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabGridDialogToolbarView.java](//src/chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabGridDialogToolbarView.java) |

> Source page:
> [`↳ Top App Bars` (`6380:43142`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6380-43142).
> It has a showcase frame (`40052:19804`) with light/dark examples labeled
> "default", "on scroll (shows divider)" and "Tab group dialog (shows color
> indicator)", plus a guidance frame (`40052:20481`). The component itself lives
> on the `↳ Core components` page.
>
> `MaterialToolbar` comes from Material Components
> (`com.google.android.material:material`), not Chromium. Chromium styles it
> with `@style/SettingsToolbarStyle` (Settings, via the `toolbarStyle` theme
> attribute) and `@style/ModernToolbar` (selectable-list screens). Both have the
> parent `Widget.Material3Expressive.Toolbar`.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Top-app-bar` (single `COMPONENT`, no variant properties). Boolean properties:
  `LHS Icon` (default false) × `Color Indicator` (false) × `RSH Icons` (true) ×
  `Icon 1` / `Icon 2` / `Icon 3` / `Icon 4` (true) × `Bottom Divider` (false) ×
  `Small Divider` (false). Icon slots are `Icon Placeholder` instance-swaps
  (defaults: `arrow_back`, `search`, `help`, `close`, `more_vert`). Light/dark
  comes from the token mode, not a property.

| Feature / Variant                | Figma Component                                           | Clank (Android)                                                                                                                                                                                                                                                                                                            |
| :------------------------------- | :-------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Title only (default)**         | `LHS Icon=false`, `RSH Icons=false`                       | `Toolbar#setTitle()`; `titleTextAppearance` = `@style/TextAppearance.Headline.Primary`                                                                                                                                                                                                                                     |
| **Leading back / up icon**       | `LHS Icon=true` (`arrow_back`)                            | `Toolbar#setNavigationIcon()` (Settings / `ModernToolbar`); `SelectableListToolbar` `NavigationButton.BACK`; `@layout/toolbar_back_button` in `TabGridDialogToolbarView`                                                                                                                                                   |
| **Trailing action icons (1–4)**  | `RSH Icons=true`, `Icon 1…4`                              | Options menu items (`Toolbar#inflateMenu` / `Menu#add`), e.g. Settings help (`ic_help_24dp`); search and close in `SelectableListToolbar`; overflow (`more_vert`) via `ActionMenuView`                                                                                                                                     |
| **On-scroll divider**            | `Bottom Divider=true` (full-bleed 1px)                    | `SelectableListLayout` shows a `FadingShadowView` (`toolbar_shadow_color`), **not** a 1dp divider. Settings: `AppBarLayout app:liftOnScroll="true"` with elevation and color overlay turned off (no visible divider). Tab group dialog: `tab_grid_dialog_hairline` `ImageView` (`TabGridDialogView#setHairlineVisibility`) |
| **Small divider under title**    | `Small Divider=true` (196px divider under the text block) | *(No equivalent found; usage unclear)*                                                                                                                                                                                                                                                                                     |
| **Tab group color indicator**    | `Color Indicator=true` (16px dot after the back icon)     | `TabGridDialogToolbarView`: `@id/tab_group_color_icon` (`@drawable/tab_group_dialog_color_icon`, an oval tinted with the group color)                                                                                                                                                                                      |
| **Editable title**               | *(Not modeled; static `Title` text)*                      | `TabGridDialogToolbarView` title is an `EditText` (rename tab group)                                                                                                                                                                                                                                                       |
| **Selection mode / search mode** | *(Not modeled)*                                           | `SelectableListToolbar` selection mode (count title + actions) and search mode (`showSearchView`)                                                                                                                                                                                                                          |
| **Dark theme**                   | Dark showcase (token mode switch)                         | Night-mode resolution of `?attr/colorSurface`, `?attr/colorOnSurface`, `?attr/colorOnSurfaceVariant`, `@color/divider_color`                                                                                                                                                                                               |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute               | Figma Design Token                                                                                    | Clank (Android)                                                                                                                                                                                                                                                                                                                         |
| :----------------------------- | :---------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Container background**       | `--clank/sys/color/surface`<br>(light `#ffffff` / dark `#131314`)                                     | `ModernToolbar`: `@macro/default_bg_color` → `?attr/colorSurface` ✅ (`@null` on `sw600dp`).<br>Settings: `AppBarLayout#setBackgroundColor(SemanticColorUtils.getSettingsBackgroundColor())` → `?attr/colorSurfaceContainerHigh` ⚠️<br>Tab group dialog: `@color/tab_grid_dialog_background_color` → `@color/legacy_bg_color_elev_1` ⚠️ |
| **Title color**                | `--clank/sys/color/onSurface, os`<br>(light `#1f1f1f` / dark `#e3e3e3`)                               | `SettingsToolbarStyle` `titleTextColor` = `?attr/colorOnSurface` ✅; `ModernToolbar` = `@macro/default_text_color` → `?attr/colorOnSurface` ✅; tab dialog `TextAppearance.Headline.Primary` → `@color/default_text_color_list` → `?attr/colorOnSurface` ✅                                                                             |
| **Title typography**           | `clank/font/font_size/headline` `22px` / `line_height/headline` `28px` / weight regular / Google Sans | `@style/TextAppearance.Headline.Primary` → `@dimen/headline_size` `22sp`, `leading` `@dimen/headline_size_leading` `28sp`, `@font/accent_font` ✅                                                                                                                                                                                       |
| **Leading (nav) icon color**   | `--clank/sys/color/onSurface, os` (`#1f1f1f` / `#e3e3e3`)                                             | Settings `navigationIconTint` = `?attr/colorOnSurface` ✅; `SelectableListToolbar` / tab dialog `@color/default_icon_color_tint_list` → `@macro/default_icon_color` → `?attr/colorOnSurface` ✅                                                                                                                                         |
| **Trailing action icon color** | `--clank/sys/color/onSurfaceVariant, osv`<br>(light `#444746` / dark `#c4c7c5`)                       | Settings help `ic_help_24dp` `android:tint="?attr/colorControlNormal"` → `@macro/default_control_color_normal` → `?attr/colorOnSurfaceVariant` ✅.<br>`SelectableListToolbar` and tab dialog (`plus`, `more_vert`) use `default_icon_color_tint_list` → `?attr/colorOnSurface` ⚠️                                                       |
| **Icon size**                  | `24px` (`Icon Placeholder`, Google Symbols)                                                           | `*_24dp` vector drawables ✅                                                                                                                                                                                                                                                                                                            |
| **Height**                     | `56px`                                                                                                | `?attr/actionBarSize` → `@dimen/default_action_bar_height` `56dp` ✅; tab dialog `main_content` `56dp` ✅                                                                                                                                                                                                                               |
| **Horizontal padding**         | `--clank/shape/spacing/16` (`16px`)                                                                   | Settings / `ModernToolbar`: M3 Expressive Toolbar defaults *(insets not verified)*. Tab dialog: `@dimen/tab_grid_dialog_app_bar_padding` `8dp` + 48dp button (icon at x=20dp) ⚠️                                                                                                                                                        |
| **Vertical padding**           | `--clank/shape/spacing/14` (`14px`)                                                                   | Content vertically centered in 56dp (`gravity="center_vertical"`) ✅                                                                                                                                                                                                                                                                    |
| **Leading icon → title gap**   | `--clank/shape/spacing/16` (`16px`) → title at x=56                                                   | M3 Toolbar `contentInsetStartWithNavigation` *(not verified)*. Tab dialog: 48dp button + 40dp color container + `@dimen/tab_group_toolbar_title_margin_start` `8dp` → title at x≈104dp (Figma x=88) ⚠️                                                                                                                                  |
| **Trailing icon spacing**      | `--clank/shape/spacing/24` (`24px`) between 24px icons (48px pitch)                                   | 48dp action buttons (`ToolbarHoverableButton` `@dimen/toolbar_button_width` `48dp`; M3 action menu items) → 48dp pitch ✅                                                                                                                                                                                                               |
| **Color indicator**            | `16px` dot                                                                                            | `@dimen/tab_group_color_icon_item_size` `18dp` oval inside a 40dp × 48dp container ⚠️                                                                                                                                                                                                                                                   |
| **Divider color**              | `--clank/macro/divider/divider_color`<br>(light `#dde3ea` / dark `#444746`)                           | `@color/divider_color` → light `?attr/colorSurfaceContainerHighest`, night `?attr/colorOutlineVariant` ✅ (used by `toolbar_hairline` / `divider_line_bg`). `SelectableListLayout` uses `@color/toolbar_shadow_color` (`black_alpha_11`) fading shadow ⚠️                                                                               |
| **Divider thickness**          | `1px`, full bleed (`left/right: -16px`)                                                               | `@dimen/toolbar_hairline_height` `1dp` ✅ (where a hairline is used)                                                                                                                                                                                                                                                                    |
| **Elevation / shadow**         | None (color fill, per guidance)                                                                       | Settings: `setElevation(0)`, `setStateListAnimator(null)`, `elevationOverlayEnabled=false` ✅. Selectable lists: fading shadow on scroll ⚠️                                                                                                                                                                                             |
| **Corner radius**              | `0`                                                                                                   | `0` ✅ (tab dialog bg uses `@dimen/tab_grid_dialog_bg_radius` on the dialog container)                                                                                                                                                                                                                                                  |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. No single shared top app bar widget

- **Figma**: one `Top-app-bar` component with boolean slots.
- **Clank**: at least three separate implementations (Settings `MaterialToolbar`
  \+ `SettingsToolbarStyle`, `SelectableListToolbar` + `ModernToolbar`,
  `TabGridDialogToolbarView` as a hand-built `LinearLayout`). They don't share a
  style, so spacing, icon tint and the scroll treatment differ between screens.

### 2. Settings container color

- **Figma**: `surface`.
- **Clank**: Settings sets the app bar (and the page) to
  `colorSurfaceContainerHigh` via
  `SemanticColorUtils.getSettingsBackgroundColor`. This may be an intentional
  Settings-specific surface that the Figma component doesn't model.

### 3. Scroll separation treatment

- **Figma**: a 1px `divider_color` line on scroll ("on scroll (shows divider)").
- **Clank**: `SelectableListLayout` uses a `FadingShadowView` (shadow, not a
  divider), which goes against the guidance ("instead of elevating with a drop
  shadow…"). Settings turns off the lift overlay and has no visible scroll
  divider *(checked statically; runtime behavior not verified)*. Only the tab
  group dialog has a hairline.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Trailing icon color

`SelectableListToolbar` and `TabGridDialogToolbarView` tint trailing icons with
`colorOnSurface`. Figma uses `colorOnSurfaceVariant` (Settings help matches
Figma).

### 2. Tab group dialog layout

Code uses 8dp side padding with 48dp touch-target buttons, an 18dp color dot in
a 40dp container, and an 8dp title margin. Figma uses 16px padding, a 16px dot
and 16px gaps. The title start (≈104dp vs 88px) and dot size differ. Code also
uses a `legacy_bg_color_elev_1` background rather than `surface`. Its trailing
actions are `plus` + `more_vert`, not the Figma defaults.

### 3. Unmodeled states and modes

Figma doesn't model pressed/hover/focus states on icon buttons (code uses ripple
/ `default_icon_background`), selection mode, search mode, an editable title, a
subtitle, or the two-row tab group toolbar
(`tab_grid_dialog_toolbar_two_row.xml`).

### 4. `Small Divider`

No code counterpart found. Its intended use (possibly an editable-title
underline) should be confirmed with design.

### 5. Not verified

- Exact `Widget.Material3Expressive.Toolbar` insets (`contentInsetStart`,
  `contentInsetStartWithNavigation`, nav button padding), so the 16px padding
  and 16px icon→title gap for Settings / `ModernToolbar` aren't confirmed.
- Runtime tint of `SelectableListToolbar` menu items that aren't set via
  `mIconColorList`.
- The dark-mode value of `legacy_bg_color_elev_1` and any runtime override of
  the tab group dialog background color (e.g. via theme utils).

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use the top app bar for the screen title plus at most a few high-value
  actions. Put the rest in the overflow (`more_vert`) menu.
- Separate the bar from content with a color fill or a 1dp divider on scroll,
  not a drop shadow (Figma guidance).
- **Labeling**: Use short, sentence-case titles that name the current screen.
  Truncate with an ellipsis instead of wrapping (`singleLine` /
  `ellipsize="end"`).

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Every icon button needs a hit area of at least `48dp`
  (`@dimen/min_touch_target_size`), even though the glyph is 24dp.
- **TalkBack**: Give every icon-only action a `contentDescription` (or a menu
  item title). The back button should announce "Navigate up" / "Back". The
  decorative tab group color dot is `importantForAccessibility="no"`.
- **Keyboard / D-pad**: Make sure toolbar buttons are focusable in a logical
  order (navigation → title → actions → overflow).

### 3. Icon Usage Guidelines

- Use 24dp Material/Google Symbols outlined glyphs. Use `onSurface` for the
  leading navigation icon and `onSurfaceVariant` for trailing actions.
- Use at most 4 trailing icons (the Figma slot limit). Put extra actions in the
  overflow menu.
