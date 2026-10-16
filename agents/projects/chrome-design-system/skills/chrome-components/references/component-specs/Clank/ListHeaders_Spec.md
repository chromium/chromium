# List Headers Spec (Clank)

This document maps the specifications of the **List Headers** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

**Headers** are non-interactive (or expand/collapse) section labels that sit
above groups of list items. Figma ships four configurations: a date-style
`Text Only` label, a `Header` category title, a `Text + Icon` expandable group
header, and a `Text + Dropdown` header with an inline filter value. In Clank
these are separate, feature-specific implementations: AndroidX
`PreferenceCategory` (Settings), the History `DateView`, bookmark section
headers, and `RecentTabsGroupView` / `ExpandablePreferenceGroup` for expandable
groups.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                        | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Headers`                                                                                                                                              | `ChromeBasePreferenceCategory`<br>`ExpandablePreferenceGroup`<br>`RecentTabsGroupView`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Source Files**   | [Figma Link: `Headers` (`7125:56846`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7125-56846&m=dev) | [components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBasePreferenceCategory.java](//src/components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ChromeBasePreferenceCategory.java)<br>[components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ExpandablePreferenceGroup.java](//src/components/browser_ui/settings/android/widget/java/src/org/chromium/components/browser_ui/settings/ExpandablePreferenceGroup.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/ntp/RecentTabsGroupView.java](//src/chrome/android/java/src/org/chromium/chrome/browser/ntp/RecentTabsGroupView.java) |

> Source page:
> [`↳ Lists` (`6371:41566`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-41566),
> showcase frame `Components/List/Headers` (`40052:3097`).
>
> Related layouts/styles (not components themselves): `@style/DateView` +
> [date_view.xml](//src/chrome/android/java/res/layout/date_view.xml) (History
> date headers, a plain `TextView`);
> [bookmark_section_header_v2.xml](//src/chrome/android/java/res/layout/bookmark_section_header_v2.xml);
> [recent_tabs_group_item.xml](//src/chrome/android/java/res/layout/recent_tabs_group_item.xml).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Headers`: `Config` (Text Only | Text + Icon | Text + Dropdown | Header) ×
  `Scheme` (Default | Baseline). Only four combinations exist:
  `Text Only`/`Default`, `Header`/`Default`, `Text + Icon`/`Baseline`,
  `Text + Dropdown`/`Baseline`.

| Feature / Variant                  | Figma Component                                                             | Clank (Android)                                                                                                             |
| :--------------------------------- | :-------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------- |
| **Date / group label**             | `Config=Text Only, Scheme=Default`                                          | History `@style/DateView` (`TextAppearance.TextMedium.Secondary`)                                                           |
| **Category title**                 | `Config=Header, Scheme=Default`                                             | `ChromeBasePreferenceCategory` (AndroidX `Preference.Category.Material`); bookmarks `bookmark_section_header_v2.xml`        |
| **Expandable group header**        | `Config=Text + Icon, Scheme=Baseline` (`keyboard_arrow_up`)                 | `RecentTabsGroupView` (`expand_collapse_icon`); `ExpandablePreferenceGroup` (`checkable_image_view_widget`)                 |
| **Header + inline dropdown value** | `Config=Text + Dropdown, Scheme=Baseline` (`arrow_drop_down` + "Last hour") | *(No shared component — closest is a feature-specific spinner, e.g. `preference_spinner_single_line.xml`)* — not verified   |
| **Baseline (non-dynamic) colors**  | `Scheme=Baseline`                                                           | `*.Baseline` text appearances (e.g. `TextAppearance.TextMediumThick.Primary.Baseline` → `default_text_color_list_baseline`) |
| **Accessibility heading**          | *(Not modeled)*                                                             | `android:accessibilityHeading="true"` (`date_view.xml`, `bookmark_section_header_v2.xml`)                                   |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                    | Figma Design Token                                                                       | Clank (Android)                                                                                                                                    |
| :---------------------------------- | :--------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Text Only — color / type**        | `onSurfaceVariant` (`#444746`); `textMedium` 14/20 Regular                               | `DateView` → `TextAppearance.TextMedium.Secondary` → `?attr/colorOnSurfaceVariant`, `14sp` / `20sp` ✅                                             |
| **Text Only — padding**             | `16px` horizontal, `14px` vertical (`--clank/shape/spacing/14`), 48px tall               | `DateView` margins: `16dp` start/end (`list_item_default_margin`), **`12dp` top / `10dp` bottom**                                                  |
| **Header — color / type**           | `onSurfaceVariant` (`#444746`); `textMediumThick` 14/20 **Medium (500)**                 | AndroidX `PreferenceCategory` title: `?android:attr/colorAccent` (Chrome primary), ~14sp medium — *AndroidX library default, not verified in tree* |
| **Header — padding**                | `16px` horizontal, `16px` top, no bottom                                                 | AndroidX `preference_category_material` layout — not verified                                                                                      |
| **Bookmarks section header**        | (closest: `Header`)                                                                      | `TextAppearance.TextMediumThick.Primary.Baseline` (onSurface baseline, 14sp medium), `16dp` horizontal, `18dp` top / `8dp` bottom margins          |
| **Text + Icon — label**             | `onSurface` (`#1f1f1f`); `textMedium` 14/20 Regular; `12px` vertical / `16px` horizontal | `RecentTabsGroupView`: `device_label` `TextAppearance.TextLarge.Primary` (16sp) + `time_label` `TextMedium.Secondary`; `16dp` start padding        |
| **Text + Icon — icon**              | `keyboard_arrow_up` 24px, `onSurface`, gap `10px`                                        | `expand_collapse_icon`, `8dp` start / `16dp` end margin, `small_icon_background_selector`                                                          |
| **Text + Dropdown — label**         | `onSurfaceVariant`; `textLarge` 16/24; `15px` vertical padding                           | Not verified                                                                                                                                       |
| **Text + Dropdown — value + arrow** | `onSurface`; `textLarge` 16/24; `arrow_drop_down` 24px; gap `8px`                        | Not verified                                                                                                                                       |
| **Font family**                     | `clank/font/font_family/alternative` (Google Sans Text)                                  | App default text font (`TextAppearance.*` do not set `fontFamily`; theme-level) — not verified                                                     |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. No shared header widget

- Figma has a single `Headers` set. In code, each surface builds its own header:
  a styled `TextView` (`DateView`), a layout (`bookmark_section_header_v2.xml`),
  an AndroidX `PreferenceCategory`, or a custom view (`RecentTabsGroupView`).
  Paddings and type differ between them.

### 2. Category header color

- **Figma `Header`**: `onSurfaceVariant` in Medium weight.
- **Clank Settings**: Chrome doesn't override `preferenceCategoryStyle` in
  `PreferenceTheme`, so it inherits the AndroidX default title color
  (`colorAccent`, which is primary-colored in Chrome themes). This is likely a
  color mismatch, but it comes from the library default and isn't checked in
  tree.
- **Bookmarks** uses `onSurface` (baseline) Medium. That is a third value.

### 3. Vertical rhythm

- `Text Only` is 14/14px in Figma but 12/10dp in `DateView`, so the History
  header comes out 4dp shorter.

### 4. "Text + Icon" semantics

- In Figma this is a single-line 14sp label. `RecentTabsGroupView` is a two-line
  (16sp title + 14sp time) expandable row, so it's closer to a `List-item` with
  a trailing icon than to a header.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Interactive states

`Text + Icon` and `Text + Dropdown` are interactive, but Figma models no
pressed, focused, or expanded/collapsed states (only `keyboard_arrow_up`). Code
animates and rotates the expand icon and shows a ripple.

### 2. `Scheme` axis

`Default` vs `Baseline` encodes dynamic vs non-dynamic (baseline) color. In
code, only a few headers use the `*.Baseline` text appearances; most follow
dynamic color.

### 3. Missing accessibility annotation

Figma doesn't mark headers as headings. Code sets
`android:accessibilityHeading="true"` on History and Bookmarks headers.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use `Text Only` for temporal or grouping labels (dates, sections) and `Header`
  for Settings category titles.
- Keep headers short and in sentence case. Don't end them with punctuation.
- Use `Text + Icon` only when the group can collapse. Use `Text + Dropdown` only
  when the dropdown filters the list below it.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Set `android:accessibilityHeading="true"` on every header so TalkBack heading
  navigation works.
- Expandable headers must expose expanded/collapsed state (for example, by
  updating the `contentDescription` or using `AccessibilityNodeInfo` actions)
  and meet the `48dp` touch target.

### 3. Icon Usage Guidelines

- Use `keyboard_arrow_up` / `keyboard_arrow_down` for expand/collapse and
  `arrow_drop_down` for inline dropdowns, at 24px in `onSurface`.
