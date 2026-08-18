# Component Spec: ListItemBadge

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **ListItemBadge** component across
Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The ListItemBadge component acts as a supplementary metadata pill, typically
docked within lists or rows. It displays text, icons, and occasionally
crossed-out original values (e.g., for "Updated Status").

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                            | WebUI (Desktop)         | Clank (Android)                                                                                                                                                                                                                    |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------- | :---------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `ListItemBadge`                                                                                                                     | Custom / `views::Badge`                                        | Custom CSS / `cr-badge` | Custom Badge Pill / `ChipView`                                                                                                                                                                                                     |
| **Source Files**   | [Figma Link: `280:27248`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-27248) | [`ui/views/controls/badge.h`](//src/ui/views/controls/badge.h) | N/A                     | [`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant      | C++ Classes   | WebUI Element/Class | Clank (Android) Implementation                      |
| :----------------- | :------------ | :------------------ | :-------------------------------------------------- |
| **Default**        | Generic View  | Custom HTML/CSS     | `ChipView` / `@drawable/pill_background`            |
| **Updated Status** | Custom layout | Custom HTML/CSS     | Custom `LinearLayout` with strikethrough `TextView` |

______________________________________________________________________

## 3. Component States

This component is strictly informational and does not feature interactive states
(Hovered, Pressed) natively.

| Interactive State | Figma | C++             | WebUI         | Clank (Android) |
| :---------------- | :---- | :-------------- | :------------ | :-------------- |
| **Default**       | N/A   | Normal painting | Normal layout | Static pill     |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type             | Figma Property        | C++ Views       | WebUI Variable / CSS   | Clank (Android) Equivalent                 |
| :--------------------- | :-------------------- | :-------------- | :--------------------- | :----------------------------------------- |
| **Default Background** | Neutral Container     | Background      | CSS classes            | `?attr/colorSurfaceContainer`              |
| **Updated Background** | Tertiary Container    | N/A             | CSS classes            | `?attr/colorTertiaryContainer`             |
| **Corner Radius**      | Fully Rounded (999px) | Canvas rounding | `border-radius: 999px` | `@dimen/chip_corner_radius` (999dp)        |
| **Typography**         | Body Five (11px)      | N/A             | Font sizes             | `@style/TextAppearance.TextSmall` (`12sp`) |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **No direct UI primitive**: WebUI, Views, and Android do not offer a single
  generic built-in `ListItemBadge` with automatic strikethrough styling;
  developers compose a pill drawable with formatted text spans or nested
  `TextView`s.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Strikethrough styling**: The "Updated Status" variant requires explicit
  formatting (e.g. `Paint.STRIKE_THRU_TEXT_FLAG` or `StrikethroughSpan` in
  Android, `text-decoration: line-through` in WebUI).

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Context**: Use within complex list items to highlight changes, tags, or
  count indicators.
