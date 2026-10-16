# List Items Spec (Clank)

This document maps the specifications of the **List Items** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

The **List-item** is the generic one- or two-line row in Clank, with an optional
leading action (radio, checkbox, switch, round check, avatar, or icon), a title
and subtitle, and an optional trailing action (switch, checkbox, chevron, or
icon) that can be split off by a vertical divider. Clank has no single "list
item" widget. The same pattern comes from AndroidX `Preference` rows in Settings
(`ChromeBasePreference`, `ChromeSwitchPreference`,
`ChromeBaseCheckBoxPreference`) and from the `SelectableItemView` /
`modern_list_item_view.xml` row used in History, Downloads, and similar lists.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                            | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `List-item`                                                                                                                                                | `ChromeBasePreference`<br>`ChromeSwitchPreference`<br>`ChromeBaseCheckBoxPreference`<br>`SelectableItemView`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `List-item` (`36283:13592`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=36283-13592&m=dev) | [components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBasePreference.java](//src/components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBasePreference.java)<br>[components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeSwitchPreference.java](//src/components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeSwitchPreference.java)<br>[components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBaseCheckBoxPreference.java](//src/components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBaseCheckBoxPreference.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableItemView.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableItemView.java) |

> Source page:
> [`↳ Lists` (`6371:41566`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-41566),
> showcase frame `Components/List/List-items` (`40052:3031`). The component set
> itself lives on the `↳ Core components` page.
>
> Related styles/layouts: `@style/PreferenceItem`, `@style/SwitchPreference`,
> `@style/CheckBoxPreference`, `@style/PreferenceTitle`,
> `@style/PreferenceSummary`
> ([settings styles.xml](//src/components/browser_ui/settings/android/java/res/values/styles.xml));
> `@style/ListItemContainer`, `ListItemStartIcon`, `ListItemEndIcon`
> ([widget styles.xml](//src/components/browser_ui/widget/android/java/res/values/styles.xml));
> [modern_list_item_view.xml](//src/components/browser_ui/widget/android/java/res/layout/modern_list_item_view.xml),
> [title_and_description_layout.xml](//src/components/browser_ui/widget/android/java/res/layout/title_and_description_layout.xml).
> Rows with an LHS radio map to `RadioButtonWithDescription` (see
> [RadioButtons_Spec_Clank.md](./RadioButtons_Spec_Clank.md)); rows with an LHS
> checkbox map to `CheckBoxWithDescription` (see
> [Checkbox_Spec_Clank.md](./Checkbox_Spec_Clank.md)).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `List-item`: `Type` (1-line | 2-line). Boolean properties: `Left Actions`
  (default true), `Right Actions` (default true), `Title` (default true),
  `Icon base` (default false), `LHS icon 2` (default false), `Vertical Divider`
  (default false), `Horizontal Divider` (default false),
  `Bottom Divider w/ padding` (default false), `Full Bottom Divider` (default
  false). Instance swaps: `LHS Action` (default `Radio-button`, `7068:51826`)
  and `RHS Action` (default `Switch`, `32019:16781`).
- Slot applicability (from the Figma variant code): `Horizontal Divider` only
  renders in `Type=1-line` (under the text); `Title` only has an effect in
  `Type=2-line` (1-line always shows the title). The guidance frame refers to
  `LHS/RHS Action Light` and `LHS/RHS Action Dark` properties, but the MCP
  export exposes only one `LHS Action` / `RHS Action` swap each. The exact
  property names weren't confirmed in the Figma properties panel.

| Feature / Variant                | Figma Component                                                | Clank (Android)                                                                                                                               |
| :------------------------------- | :------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------- |
| **1-line row**                   | `Type=1-line`                                                  | `ChromeBasePreference` with title only; `SelectableItemView` with empty description                                                           |
| **2-line row**                   | `Type=2-line`                                                  | `Preference` `title` + `summary` (`PreferenceTitle` / `PreferenceSummary`); `SelectableItemView` `title` + `description`                      |
| **Hide title (subtitle-only)**   | `Title=false`                                                  | *(No direct equivalent; set summary only)*                                                                                                    |
| **Leading radio**                | `Left Actions=true`, `LHS Action=Radio-button`                 | `RadioButtonWithDescription`                                                                                                                  |
| **Leading checkbox**             | `LHS Action=Checkbox`                                          | `CheckBoxWithDescription`                                                                                                                     |
| **Leading icon in tonal circle** | `Icon base=true` (globe in `scontainer` circle)                | `Preference` `android:icon` (no container circle); `SelectableItemView` `start_icon` (`ListItemStartIcon`, 36dp, background set per feature)  |
| **Leading avatar / favicon**     | `LHS icon 2=true` (`Identity disk`, 24px)                      | `SelectableItemView` `start_icon` with a favicon/avatar drawable                                                                              |
| **Trailing switch**              | `Right Actions=true`, `RHS Action=Switch`                      | `ChromeSwitchPreference` (`@layout/preference_switch` → `Widget.BrowserUI.Switch`)                                                            |
| **Trailing checkbox**            | `RHS Action=Checkbox`                                          | `ChromeBaseCheckBoxPreference`                                                                                                                |
| **Trailing chevron / icon**      | `RHS Action=chevron_right` / icon                              | `SelectableItemView` `end_button` (`ChromeImageButton`, `ListItemEndIcon` 56dp wide); `ChromeImageViewPreference` for icon widgets            |
| **Vertical divider before RHS**  | `Vertical Divider=true` (24px tall)                            | `ChromeSwitchPreference` / `ChromeImageViewPreference` *(no generic divider; feature-specific)* — not verified                                |
| **Full-bleed bottom divider**    | `Full Bottom Divider=true`                                     | `PreferenceItem` `allowDividerBelow=true` + `SettingsFragment` `android:divider=@drawable/list_divider`                                       |
| **Inset bottom divider**         | `Bottom Divider w/ padding=true` (16px side inset)             | *(No shared inset-divider style; feature-specific `ItemDecoration`)*                                                                          |
| **Disabled**                     | *(Not modeled)*                                                | `setEnabled(false)` → `default_text_color_list` / `default_text_color_secondary_list` @ `default_disabled_alpha` (38%)                        |
| **Pressed / Focused / Selected** | *(Not modeled)*                                                | `PreferenceLayout` `?android:attr/activatedBackgroundIndicator`; `SelectableItemView` `list_item_rounded_background_selector` + check overlay |
| **Dark theme**                   | Dark showcase (manual swap of `LHS/RHS Action Dark` instances) | Night-mode resolution of the same `?attr` tokens                                                                                              |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                   | Figma Design Token                                                                                                        | Clank (Android)                                                                                                                                                                                                               |
| :--------------------------------- | :------------------------------------------------------------------------------------------------------------------------ | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Title color**                    | `--clank/sys/color/onSurface, os` (`#1f1f1f`)                                                                             | `@color/default_text_color_list` → `?attr/colorOnSurface`                                                                                                                                                                     |
| **Subtitle color**                 | `--clank/sys/color/onSurfaceVariant, osv` (`#444746`)                                                                     | `@color/default_text_color_secondary_list` → `?attr/colorOnSurfaceVariant`                                                                                                                                                    |
| **Title typography**               | `textLarge`: Google Sans Text Regular, `16px` / `24px`, `0` tracking                                                      | `@style/TextAppearance.TextLarge.Primary` (`@dimen/text_size_large` `16sp`, `@dimen/text_size_large_leading` `24sp`)                                                                                                          |
| **Subtitle typography**            | `textMedium`: Google Sans Text Regular, `14px` / `20px`                                                                   | `@style/TextAppearance.TextMedium.Secondary` (`14sp` / `20sp`)                                                                                                                                                                |
| **Leading / trailing icon color**  | LHS radio/checkbox glyph `onSurface`; `Icon base` / `LHS icon 2` globe `onSurfaceVariant`; RHS chevron `onSurfaceVariant` | Feature-specific. `default_icon_color_tint_list` → `@macro/default_icon_color` (`?attr/colorOnSurface`); `default_icon_color_secondary_tint_list` → `?attr/colorOnSurfaceVariant`. Which one each caller uses was not checked |
| **Icon base container**            | `--clank/sys/color/scontainer, -sc` (`#f0f4f9`), `100px` radius, `4px` padding around a `16px` glyph (24px total)         | *(No shared token; feature-specific `setBackground`)*                                                                                                                                                                         |
| **Divider color**                  | `--clank/macro/divider/divider_color` (`#dde3ea`)                                                                         | `@color/divider_color` → light `?attr/colorSurfaceContainerHighest`, night `?attr/colorOutlineVariant` (via `@drawable/list_divider`)                                                                                         |
| **Horizontal padding**             | `--clank/shape/spacing/16` (`16px`)                                                                                       | `@dimen/list_item_default_margin` `16dp`; Preference `?android:attr/listPreferredItemPaddingStart/End` (AndroidX/Material default `16dp`)                                                                                     |
| **Vertical padding (1-line)**      | `--clank/shape/spacing/16` (`16px`), giving a 56px row (58px with the default 26px `Switch`)                              | `PreferenceLayout` `paddingTop/Bottom` `16dp`; `ChromeSwitchPreference` min height `@dimen/settings_toggle_item_min_height` (`57.5dp`)                                                                                        |
| **Vertical padding (2-line)**      | `--clank/shape/spacing/12` (`12px`), giving a 68px row                                                                    | `@dimen/settings_item_vertical_padding_multi_line` `12dp`; `SelectableItemView` row min height `@dimen/list_item_min_height` `64dp`                                                                                           |
| **Content gap (LHS → text → RHS)** | `--clank/shape/spacing/16` (`16px`)                                                                                       | `ListItemStartIcon` `marginEnd` `@dimen/list_item_start_icon_right_margin` **`20dp`** (small icon variant `16dp`)                                                                                                             |
| **Leading slot size**              | `24px` glyph (radio/checkbox/avatar); icon base `24px`                                                                    | `ListItemStartIcon` `36dp` box (`ListItemSmallStartIcon` `20dp`)                                                                                                                                                              |
| **Trailing slot size**             | `24px` glyph / `42×26` switch                                                                                             | `ListItemEndIcon` `56dp` wide × row height; switch `Widget.BrowserUI.Switch` (M3 `52×32` track)                                                                                                                               |
| **Row background**                 | Transparent (on surface)                                                                                                  | `?android:attr/activatedBackgroundIndicator` (Preference); `list_item_rounded_background_selector` (`HistoryItemView`, etc.)                                                                                                  |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. No single Clank list-item widget

- **Figma**: One `List-item` component with boolean slots covers settings rows,
  selectable rows, and action rows.
- **Clank**: The pattern is split between AndroidX `Preference` subclasses
  (Settings) and `SelectableItemView` / feature layouts (History, Downloads,
  Bookmarks, Recent tabs). Each sets its own paddings, icon sizes, and
  backgrounds, so for any given screen you have to check the actual
  implementation.

### 2. Start-icon spacing and size

- **Figma**: Every slot is 24px, with a 16px gap between LHS, text, and RHS.
- **Clank**: `ListItemStartIcon` is a 36dp box with a 20dp end margin (16dp for
  `ListItemSmallStartIcon`), so text in modern list items starts about 8dp
  further right than Figma shows.

### 3. Dividers

- `Full Bottom Divider` maps onto the AndroidX Preference divider
  (`allowDividerBelow` + `list_divider`). `Bottom Divider w/ padding` and
  `Vertical Divider` have no shared implementation; features draw their own.
- The light divider color matches (`#dde3ea` = `colorSurfaceContainerHighest`).
  In dark mode, code uses `colorOutlineVariant`; the Figma dark value was not
  checked.

### 4. Icon base container

- The tonal `scontainer` circle behind the leading icon isn't part of any shared
  Clank style. Features that need it (for example bookmarks compact rows, which
  use `colorSurfaceContainerLow`) set it themselves.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing interactive states in Figma

There are no Pressed / Focused / Hovered / Disabled / Selected variants. Code
renders ripples (`activatedBackgroundIndicator`,
`list_item_rounded_background_selector`), dims disabled rows to 38% through the
color state lists, and shows a selection check in `SelectableItemView`.

### 2. Light/dark action swapping

Figma requires designers to manually swap `LHS/RHS Action Light` / `Dark`
instances for dark mode. Code resolves this automatically from theme attributes,
so you don't need separate assets.

### 3. Switch sizing

Figma's nested `Switch` is `42×26`. Code uses the M3 `MaterialSwitch`
(`Widget.BrowserUI.Switch`). See [Switch_Spec_Clank.md](./Switch_Spec_Clank.md).

### 4. Touch target

Figma 1-line rows are 56px tall, which is above the 48dp minimum. In code,
Preferences use `?android:attr/listPreferredItemHeightSmall` (48dp) as a floor,
and modern list items use 64dp.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use 1-line rows for simple navigational or toggle settings. Use 2-line rows
  when a summary helps the user understand the current state.
- Use one leading control *or* one trailing control for the primary action.
  Don't put a radio and a switch in the same row.
- Put a vertical divider before the trailing control only when the row body and
  the control do different things (for example, the row opens details and the
  switch toggles).
- **Labeling**: Sentence case, concise titles. Summaries describe the current
  value or the consequence.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Touch target**: Make the whole row clickable and at least `48dp` tall
  (`@dimen/min_touch_target_size`).
- **TalkBack**: For Preferences, title + summary + widget state are announced
  together. For custom rows, set `contentDescription` on icon-only end buttons
  and mark decorative start icons `importantForAccessibility="no"` (as
  `modern_list_item_view.xml` does).
- **Keyboard / D-pad**: Rows should be focusable, with a visible ripple/focus
  background.

### 3. Icon Usage Guidelines

- Leading glyphs are 24px Google Symbols. Use `onSurface` for controls and
  `onSurfaceVariant` for secondary trailing icons such as `chevron_right`.
- Use favicons/avatars (`LHS icon 2`) only for entity rows (sites, accounts).
