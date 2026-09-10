# Component Spec: List Items

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **List Items** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The List Items component (also referred to as URL List Item, Rich Hover Button,
or Selectable Item View) is used to display rows of structured metadata. It
typically includes a leading visual (such as a favicon, image, or folder icon),
a title, an optional description/subtitle, optional badges/chips, and an
optional trailing action icon. It supports interactive hover, active, selection,
and focus states.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                                                  | WebUI (Desktop)                                                                                                                                    | Clank (Android)                                                                                                                                                                                                                                                            |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `List Items`                                                                                                                        | `RichHoverButton`                                                                                                    | `<cr-url-list-item>`                                                                                                                               | `SelectableItemView` / `ListItemBuilder`                                                                                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `280:28521`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-28521) | [`chrome/browser/ui/views/controls/rich_hover_button.h`](//src/chrome/browser/ui/views/controls/rich_hover_button.h) | [`ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts`](//src/ui/webui/resources/cr_elements/cr_url_list_item/cr_url_list_item.ts) | [`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableItemView.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableItemView.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant                     | C++ `RichHoverButton` Property                | WebUI `<cr-url-list-item>` Property                   | Clank (Android) Property / MVC                                   |
| :-------------------------------- | :-------------------------------------------- | :---------------------------------------------------- | :--------------------------------------------------------------- |
| **Size (Large, Medium, Compact)** | Governed by Layout / `CalculatePreferredSize` | `size` attribute (`"compact"`, `"medium"`, `"large"`) | `@dimen/list_item_default_min_height` (`56dp`) / `72dp` (2-line) |
| **Type (Website, Folder)**        | Differentiated via `SetIcon()`                | `isFolder_` property                                  | `PropertyModel` (`ICON`, `IS_FOLDER`)                            |
| **hasImage (true/false)**         | `SetIcon(ui::ImageModel)`                     | `imageUrls` (array)                                   | `PropertyModel` (`ICON_DRAWABLE` / Favicon helper)               |
| **hasChips (true/false)**         | `SetCustomView()`                             | `<slot name="badges">` & `hasBadges`                  | `ChipView` nested in title container                             |
| **Title / Subtitle**              | `SetTitleText()`, `SetSubtitleText()`         | `title` and `description`                             | `PropertyModel.TITLE`, `PropertyModel.DESCRIPTION`               |
| **Trailing Action**               | `SetActionIcon()`                             | `<slot name="suffix">`                                | `PropertyModel.MORE_BUTTON_CLICK_LISTENER` / `ChromeImageButton` |

______________________________________________________________________

## 3. Component States

| Interactive State           | Figma             | C++ `RichHoverButton`                | WebUI `<cr-url-list-item>`                             | Clank (Android)                                              |
| :-------------------------- | :---------------- | :----------------------------------- | :----------------------------------------------------- | :----------------------------------------------------------- |
| **Default**                 | `state="Default"` | `ButtonState::STATE_NORMAL`          | Standard layout                                        | Standard list item layout                                    |
| **Hovered**                 | `state="Hovered"` | `ButtonState::STATE_HOVERED`         | `:host(.hovered)`, `:host([force-hover])`              | Hover layer overlay                                          |
| **Pressed / Active**        | N/A               | `ButtonState::STATE_PRESSED`         | `:host(.active)` / `var(--cr-active-background-color)` | `?attr/selectableItemBackground` ripple                      |
| **Selected (Multi-Select)** | N/A               | Table multi-selection highlight      | CSS selected attribute                                 | `SelectableItemView.setChecked(true)` with checkmark overlay |
| **Focused**                 | N/A               | `views::FocusRing` applied to button | `:host-context(.focus-outline-visible)`                | Hardware / TalkBack accessibility focus                      |
| **Disabled**                | N/A               | `ButtonState::STATE_DISABLED`        | `[disabled]` attribute                                 | `android:enabled="false"`                                    |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type                        | Figma Property              | C++ Views                            | WebUI Variable / CSS               | Clank (Android) Equivalent                                        |
| :-------------------------------- | :-------------------------- | :----------------------------------- | :--------------------------------- | :---------------------------------------------------------------- |
| **Item Height (Large / 2-Line)**  | 68px                        | Calculated via views layout          | `--cr-url-list-item-height` (68px) | `@dimen/list_item_two_line_min_height` (`72dp`)                   |
| **Item Height (Medium / 1-Line)** | 48px                        | Calculated via views layout          | `--cr-url-list-item-height` (48px) | `@dimen/list_item_default_min_height` (`56dp`)                    |
| **Item Height (Compact)**         | 36px                        | Calculated via views layout          | `--cr-url-list-item-height` (36px) | `@dimen/list_item_compact_min_height` (`48dp`)                    |
| **Padding**                       | 6px 16px                    | Insets                               | `--cr-url-list-item-padding`       | `@dimen/list_item_start_padding` (`16dp`), `end_padding` (`16dp`) |
| **Title Typography**              | Body Large (14px/20px)      | Native Typography Provider           | Default font inheritance           | `@style/TextAppearance.TextLarge.Primary` (`16sp`)                |
| **Subtitle Typography**           | Body Medium (12px/16px)     | Native Typography Provider           | Default font inheritance           | `@style/TextAppearance.TextMedium.Secondary` (`14sp`)             |
| **Hover Background**              | Overlay (#1f1f1f / opacity) | `kColorHoverButtonBackgroundHovered` | `--cr-hover-background-color`      | State layer overlay                                               |
| **Active / Ripple**               | Overlay                     | Derived from Native Theme            | `--cr-active-background-color`     | `?attr/selectableItemBackground`                                  |
| **Asset Size (Medium)**           | 40px                        | Icon resolution                      | `height: 40px; width: 40px`        | `40dp` / `@dimen/list_item_icon_size`                             |
| **Asset Corner Radius**           | 4px / 8px                   | Handled by ImageView                 | `border-radius: 4px` / `8px`       | `@dimen/default_rounded_corner_radius` (`8dp`)                    |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Multi-Select Controller Integration**: In Clank,
  [`SelectableListLayout`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableListLayout.java)
  and
  [`SelectableItemView`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/selectable_list/SelectableItemView.java)
  integrate long-press multi-select gesture tracking, animating between default
  favicons and selection checkmarks automatically.
- **Layout Structure**: C++ `RichHoverButton` uses `views::TableLayout`, WebUI
  uses CSS Flex/Grid with slots, and Clank uses Android `RecyclerView` with
  `ModelList` / `PropertyModel` adapters.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Touch Density**: Clank enforces a minimum list item height of `56dp`
  (1-line) or `72dp` (2-line) to ensure touch targets meet Android Material
  accessibility standards, whereas Desktop allows compact `36px`–`48px` rows.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Content Density**: Use Compact size for dense settings/bookmarks, and 2-line
  standard items for History and Tab Resumption.
- **Favicons & Images**: Always provide fallback icons when remote images fail
  to resolve.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Manage `aria-label` and `aria-description`. Use `asAnchor` for link
  navigation.
- **C++ Views**: Managed via `HoverButton::GetViewAccessibility()`.
- **Clank (Android)**: `SelectableItemView` handles TalkBack announcement of
  checked/unchecked state and row actions.
