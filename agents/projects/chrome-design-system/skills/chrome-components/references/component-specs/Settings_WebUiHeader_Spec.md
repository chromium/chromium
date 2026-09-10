# Component Spec: Settings - WebUiHeader

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **WebUiHeader** component across Figma
and WebUI (Desktop).

______________________________________________________________________

## Overview

The `WebUiHeader` is a branding, navigation, and search header component used at
the top of utility and settings pages in Chromium desktop WebUI. It contains the
application branding/title, a responsive search field or address bar, and slots
for contextual action buttons or navigation controls.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                             | C++ Views (Desktop) | WebUI (Desktop)                                                                                                                                                                                                                                                                | Clank (Android) |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------- |
| **Component Name** | `WebUiHeader`                                                                                                                                               | **N/A**             | `<cr-toolbar>` and `<cr-toolbar-search-field>`                                                                                                                                                                                                                                 | **N/A**         |
| **Source Files**   | [Figma Link: `60352:8418`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=60352-8418&t=FMUhNTpxly8KQpO3-11) | **N/A**             | [ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar.ts)<br>[ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts) | **N/A**         |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

- **Desktop / Tablet (Wide)**:
  - Figma: `breakpoint="Desktop"`
  - WebUI: `<cr-toolbar>` wide mode.
- **Phone / Narrow**:
  - Figma: `breakpoint="Desktop-Small"`
  - WebUI: `<cr-toolbar narrow>` showing navigation drawer icon.
- **Search Active / Inputting**:
  - Figma: `state="Focus"`
  - WebUI: `<cr-toolbar-search-field showing-search>` is true.

______________________________________________________________________

## 3. Component States

| State                      | Figma Property     | WebUI CSS / State Property         |
| :------------------------- | :----------------- | :--------------------------------- |
| **Default**                | `state="Default"`  | `:host` / standard rendering       |
| **Hover (Search Field)**   | —                  | `:host(:hover) #stateBackground`   |
| **Focused (Search Field)** | `state="Focus"`    | `:host([search-focused_])` outline |
| **Scrolled Header**        | `state="Scrolled"` | `.cr-scrollable-top-shadow`        |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Visual Property             | Figma Design Token                                                           | WebUI CSS / Custom Property                        |
| :-------------------------- | :--------------------------------------------------------------------------- | :------------------------------------------------- |
| **Header Height**           | `56px`                                                                       | `var(--cr-toolbar-height, 56px)`                   |
| **Header Title Font**       | `family: "typeface/Title" (Roboto:Medium)`, Weight: `500`, Size: `22px`      | `font-size: 170%`, `font-weight: 500`              |
| **Search Container Height** | `36px`                                                                       | `height: 36px`                                     |
| **Search Corner Radius**    | `100px`                                                                      | `--cr-toolbar-search-field-border-radius: 100px`   |
| **Search Normal BG**        | `var(--desktop/sys/base-colors/base-container, #edf2fa)`                     | `var(--cr-toolbar-search-field-background)`        |
| **Search Focus Outline**    | `border-2 border-[var(--desktop/sys/state-colors/state-focus-ring,#0b57d0)]` | `outline: 2px solid var(--cr-focus-outline-color)` |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Responsive Layouts**: Desktop WebUI switches between wide and narrow search
  layout via container queries.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Shadow Elevation**: WebUI top scroll shadow is handled by parent container
  scroll observer classes (`.cr-scrollable-top-shadow`).

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- Position the header sticky at the top of the viewport layout.
- Support instant search filtering with clear/cancel action.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Ensure `<cr-toolbar>` has `role="banner"`.
