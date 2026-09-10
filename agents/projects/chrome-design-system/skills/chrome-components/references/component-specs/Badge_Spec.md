# Component Spec: Badge

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Badge** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The Badge component is a highly compact, informational text label (commonly
rendering text like "NEW" or numerical counts). It is placed alongside other
components to highlight updates, status badges, or new features.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                            | WebUI (Desktop)                                             | Clank (Android)                                                                                                                                    |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------- | :---------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Badge`                                                                                                                             | `views::Badge`                                                 | Custom CSS / `new-badge`                                    | Custom Badge Pill / `TextViewWithLeading`                                                                                                          |
| **Source Files**   | [Figma Link: `280:26503`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26503) | [`ui/views/controls/badge.h`](//src/ui/views/controls/badge.h) | `ui/webui/resources/cr_components/help_bubble/new_badge.ts` | [`ui/android/java/src/org/chromium/ui/widget/TextViewWithLeading.java`](//src/ui/android/java/src/org/chromium/ui/widget/TextViewWithLeading.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant                 | C++ `views::Badge` Method        | WebUI Element/Class    | Clank (Android) Implementation                                        |
| :---------------------------- | :------------------------------- | :--------------------- | :-------------------------------------------------------------------- |
| **Default (Tonal Container)** | Not natively supported           | `.new-badge` or custom | `@macro/chip_bg_color` / Pill drawable                                |
| **Cocoa Menu (Primary)**      | `views::Badge` standard behavior | `<new-badge>` / Custom | `@macro/default_control_color_active` / `@color/accent_material_dark` |

______________________________________________________________________

## 3. Component States

This component is non-interactive.

| Interactive State | Figma | C++ `views::Badge` | WebUI         | Clank (Android)  |
| :---------------- | :---- | :----------------- | :------------ | :--------------- |
| **Default**       | N/A   | Normal painting    | Normal layout | Static pill view |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type        | Figma Property            | C++ Views        | WebUI Variable / CSS                | Clank (Android) Equivalent                                   |
| :---------------- | :------------------------ | :--------------- | :---------------------------------- | :----------------------------------------------------------- |
| **Typography**    | Special Label (9px, Bold) | Custom font list | `font-weight: bold; font-size: 9px` | `@style/TextAppearance.TextSmall.Inverse` / 10sp bold        |
| **Padding**       | `4px`                     | Native layout    | `padding: 4px`                      | `paddingStart="6dp"`, `paddingEnd="6dp"`, `paddingTop="2dp"` |
| **Corner Radius** | `4px`                     | Native drawing   | `border-radius: 4px`                | `@dimen/default_rounded_corner_radius` (`4dp`)               |
| **Default Color** | Tonal Container           | N/A              | CSS specific                        | `@macro/chip_bg_color` / `?attr/colorSecondaryContainer`     |
| **Primary Color** | Primary                   | Hardcoded blue   | CSS specific                        | `@macro/default_control_color_active` / `?attr/colorPrimary` |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Theme Toggles**: The `views::Badge` in C++ hardcodes its rendering to a blue
  background. Custom colors require overriding or creating a new View.
- **Clank Implementation**: In Android, badges are rendered either as nested
  `TextView`s with custom shape drawables (e.g.,
  `res/drawable/badge_background.xml`) or integrated into toolbar action icons
  via badge layout layers.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Typography size**: A 9px font size is extremely small and may be scaled up
  automatically by OS-level text scaling settings, deviating from pure Figma
  mockups.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Brevity**: Badges should contain 1-3 characters maximum (e.g., "NEW", "PRO",
  "1").
