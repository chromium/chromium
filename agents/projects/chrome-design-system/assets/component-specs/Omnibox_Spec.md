# Component Spec: Omnibox

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Omnibox** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Omnibox** component is the central address bar and search bar of the
Google Chrome browser. It acts as a unified input interface for typing search
queries, entering URLs, displaying navigation states, secure padlock indicators,
and rich permission chips (e.g. location, camera prompts).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                                                | WebUI (Desktop)                                                                                                                                    | Clank (Android)                                                                                                                                                                                                                                                                                                                                      |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Omnibox`                                                                                                                           | `views::OmniboxViewViews`                                                                                          | `<cr-toolbar-search-field>`                                                                                                                        | `LocationBarCoordinator` / `UrlBar`                                                                                                                                                                                                                                                                                                                  |
| **Source Files**   | [Figma Link: `288:16532`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=288-16532) | [chrome/browser/ui/views/omnibox/omnibox_view_views.h](//src/chrome/browser/ui/views/omnibox/omnibox_view_views.h) | [ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts) | [chrome/android/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/omnibox/UrlBar.java](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/UrlBar.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant         | Figma Component                  | C++ Views (Desktop)                             | WebUI (Web Frontend)                | Clank (Android)                              |
| :------------------------ | :------------------------------- | :---------------------------------------------- | :---------------------------------- | :------------------------------------------- |
| **NTP Style**             | `variant=NTP`                    | Configured for New Tab Page layout context      | Renders search-focused look         | Fake search box transitioning to LocationBar |
| **WebUI Style**           | `variant=WebUI`                  | N/A (Standard settings search)                  | `<cr-toolbar-search-field>` styling | N/A                                          |
| **URL Style**             | `variant=URL`                    | Core address bar parsing / security decorations | N/A                                 | `UrlBar` with security icon & action chips   |
| **URL w/Permission Chip** | `variant=URL w/Permissions Chip` | `ContentSettingImageView` / Permission chip     | N/A                                 | `LocationBarCoordinator` permission status   |

______________________________________________________________________

## 3. Component States

| State                | Figma Component          | C++ Views (Desktop)                         | WebUI (Web Frontend)               | Clank (Android)                        |
| :------------------- | :----------------------- | :------------------------------------------ | :--------------------------------- | :------------------------------------- |
| **Default (Normal)** | `state=Default`          | Normal rendering, unfocused look            | Default search bar overlay styling | Unfocused toolbar state                |
| **Hovered**          | `state=Hovered`          | Displays background highlights when hovered | `:hover` class styles on input     | State layer overlay                    |
| **Pressed**          | `state=Pressed`          | Handled inside event clicks                 | Tap/Click trigger animations       | Focus transition animation             |
| **Disabled**         | `state=Disabled`         | Non-editable address bar input              | Attribute: `disabled`              | `UrlBar.setEnabled(false)`             |
| **Focused**          | *(Commonly represented)* | Shows autocomplete list dropdown            | `:focus-within` styling applied    | Expands full-bleed suggestions overlay |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute         | Figma Design Token                                                | C++ Views (Desktop)                  | WebUI (Web Frontend)                     | Clank (Android) Equivalent                                    |
| :----------------------- | :---------------------------------------------------------------- | :----------------------------------- | :--------------------------------------- | :------------------------------------------------------------ |
| **Container Background** | `--desktop/sys/component-colors/omnibox-container`<br>(`#edf2fa`) | `kColorOmniboxBackground`            | `--cr-toolbar-search-field-background`   | `@macro/omnibox_bg_color` / `?attr/colorSurfaceContainerHigh` |
| **On-Container Text**    | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`)          | `kColorOmniboxText`                  | `--cr-toolbar-search-field-text`         | `@macro/default_text_color` / `?attr/colorOnSurface`          |
| **Subtle Text Color**    | `--desktop/sys/surface-colors/on-surface-subtle`<br>(`#474747`)   | `kColorOmniboxResultsTextDimmed`     | `--cr-toolbar-search-field-prompt-color` | `@macro/default_text_color_secondary`                         |
| **Primary Theme Accent** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)             | `ui::kColorSysPrimary`               | `--google-blue-600`                      | `@macro/default_control_color_active` / `?attr/colorPrimary`  |
| **Corner Radius**        | `--desktop/corner-radius/fully-rounded`<br>(`999px`)              | Handled via shape clipping bounds    | `border-radius: 100px;`                  | `@dimen/omnibox_curved_corner_radius` (999dp)                 |
| **LHS Spacing**          | `--desktop/spacing/5`<br>(`5px` padding)                          | Controlled by omnibox layout margins | Bounded by standard layout padding       | `@dimen/location_bar_url_action_offset`                       |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Multi-Platform Omnibox Implementations

- **Desktop Views**: Core address bar implemented in C++ Views
  (`OmniboxViewViews`).
- **WebUI**: Settings/History search implemented via independent Lit component
  (`<cr-toolbar-search-field>`).
- **Clank (Android)**: Implemented via
  [`LocationBarCoordinator`](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java),
  [`UrlBar`](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/UrlBar.java),
  and the autocomplete MVC pipeline.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Focus Expansion Model

- **Desktop Views**: Omnibox maintains its width in place and drops down a
  floating popup widget.
- **Clank (Android)**: Focusing the Omnibox triggers a full-screen transition:
  hiding the toolbar buttons, elevating the location bar to the top of the
  viewport, and rendering suggestions in a full-height list with soft keyboard
  interaction.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Input Clearness**: Keep search placeholders simple.
- **Security Trust**: Ensure the secure padlock or warning indicators accurately
  communicate site security status.
