# Component Spec: Omnibox

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Omnibox** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **Omnibox** component is the central address bar and search bar of the Google Chrome browser. It acts as a unified input interface for typing search queries, entering URLs, displaying navigation states, secure padlock indicators, and rich permission chips (e.g. location, camera prompts).

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Omnibox` | `views::OmniboxViewViews` | `<cr-toolbar-search-field>` |
| **Source Files** | [Figma Link: `288:16532`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=288-16532&m=dev) | [chrome/browser/ui/views/omnibox/omnibox_view_views.h](//src/chrome/browser/ui/views/omnibox/omnibox_view_views.h) | [ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **NTP Style** | `variant=NTP` | Configured for New Tab Page layout context | Renders search-focused look |
| **WebUI Style** | `variant=WebUI` | N/A (Standard settings search) | `<cr-toolbar-search-field>` standard styling |
| **URL Style** | `variant=URL` | Core address bar parsing / security decorations | N/A (WebUI doesn't serve as main address bar) |
| **URL w/Permission Chip**| `variant=URL w/Permissions Chip`| `ContentSettingImageView` / Location Permission chip in location bar | N/A (Main frame permission bubbles) |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | Normal rendering, unfocused look | Default search bar overlay styling |
| **Hovered** | `state=Hovered` | Displays background highlights when hovered | `:hover` class styles on input container |
| **Pressed** | `state=Pressed` | Handled inside event clicks | Tap/Click trigger animations |
| **Disabled** | `state=Disabled` | Non-editable address bar input | Attribute: `disabled` on input control |
| **Focused** | *(Commonly represented)* | Shows autocomplete list dropdown and focused outlines | `:focus-within` styling applied |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Container Background** | `--desktop/sys/component-colors/omnibox-container`<br>(`#edf2fa`) | `kColorOmniboxBackground` | `--cr-toolbar-search-field-background` |
| **On-Container Text** | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`) | `kColorOmniboxText` | `--cr-toolbar-search-field-text` |
| **Subtle Text Color** | `--desktop/sys/surface-colors/on-surface-subtle`<br>(`#474747`) | `kColorOmniboxResultsTextDimmed` | `--cr-toolbar-search-field-prompt-color` |
| **Primary Theme Accent** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`) | `ui::kColorSysPrimary` | `--google-blue-600` |
| **Corner Radius** | `--desktop/corner-radius/fully-rounded`<br>(`999px`) | Handled via shape clipping bounds | `border-radius: 100px;` |
| **LHS Spacing** | `--desktop/spacing/5`<br>(`5px` padding) | Controlled by omnibox layout margins | Bounded by standard layout padding |

---

## 5. Architectural & Implementation Gaps

### 1. Unified Desktop View vs. Segmented WebUI Interfaces
*   **Figma**: Models NTP, WebUI, and browser address bars inside a single variant set.
*   **Chromium Codebase**: The browser's native Omnibox is implemented solely in C++ Views (`OmniboxViewViews`), while settings and history pages recreate subset search bar components in WebUI via independent Lit web components (`<cr-toolbar-search-field>`). The WebUI versions are completely separate and do not share layout code with the native browser address bar.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Permissions Chip Integration
*   **Figma**: Places the Permissions Chip natively inside the Omnibox container bounds.
*   **C++ Views**: Implemented as independent view units (`LocationBarView` containing separate child chips like `ContentSettingImageView` and `PermissionChip`) that sit adjacent to the text field inside the bar, rather than being inline text field nodes.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Input Clearness**: Keep search placeholders simple ("Search Settings" or "Search history").
*   **Security Trust**: Ensure the secure padlock or warning icons align correctly to guarantee user confidence.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Textfield (Click/text input handlers)
              └── OmniboxView (Abstract omnibox core)
                     └── OmniboxViewViews (Native views implementation)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser base element)
       └── CrLitElement (Lit reactive base component)
              └── CrToolbarSearchFieldElement (WebUI toolbar search element)
    ```
