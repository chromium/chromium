# Component Spec: SearchBoxOmnibox

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **SearchBoxOmnibox** (Autocomplete Suggestions Dropdown) component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **SearchBoxOmnibox** component represents the autocomplete suggestion popup container that expands downwards below the Omnibox. It populates dynamic search terms, favicon cards, browser navigation history nodes, bookmarks, and site links as the user types queries.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `SearchBoxOmnibox` | `views::OmniboxPopupViewViews` | `<omnibox-popup-searchbox>` / `<cr-searchbox-dropdown>` |
| **Source Files** | [Figma Link: `34968:6820`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=34968-6820&m=dev) | [chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h](//src/chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h) | [chrome/browser/resources/omnibox_popup/omnibox_popup_searchbox.ts](//src/chrome/browser/resources/omnibox_popup/omnibox_popup_searchbox.ts)<br>[ui/webui/resources/cr_components/searchbox/searchbox_dropdown.ts](//src/ui/webui/resources/cr_components/searchbox/searchbox_dropdown.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Search List Layout** | Contains suggestion rows (`.base.searchbox.List-search`) | Instantiates `OmniboxResultView` entries | Hosts dynamic `<cr-searchbox-match>` rows |
| **History Icon** | Uses nested `history` vector symbols | Renders history clocks via custom vector assets | Standard SVG history clock icons |
| **Favicon Asset** | Houses favicon container wrapper | Fetches and renders site-specific icon bitmaps | Displayed using favicon styling in `<cr-searchbox-icon>` |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | Base suggestion view rendering | Default suggestions row style |
| **Hovered** | `state=Hovered` | Displays background focus highlights | `:hover` styled background overlays |
| **Pressed** | `state=Pressed` | Triggers navigation click event | Click/Select trigger transitions |
| **Disabled** | `state=Disabled` | Grayed out/non-clickable rows | N/A |
| **Focused** | *(Commonly represented)* | Selected row changes background highlighting | `selected` property updates active row background |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Dropdown Container BG** | `--desktop/sys/surface-colors/surface`<br>(`white`) | `kColorOmniboxResultsBackground` | `--cr-menu-background-color` |
| **On-Surface Text** | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`) | `kColorOmniboxResultsText` | `--cr-primary-text-color` |
| **Dimmed Text Color** | `--desktop/sys/surface-colors/on-surface-subtle`<br>(`#474747`) | `kColorOmniboxResultsTextDimmed` | `--cr-secondary-text-color` |
| **Corner Radius** | `--desktop/corner-radius/16`<br>(`16px`) | Matches popup widget corner radii specs | `border-radius: 12px;` or `16px;` |
| **Elevation Drop Shadow** | `--desktop/elevation/4`<br>(Dual drop shadows) | Handled dynamically by native window elevation | Handled via CSS shadow/elevation maps |

---

## 5. Architectural & Implementation Gaps

### 1. Floating Native Widget Overlay vs. inline DOM Trees
*   **Figma**: Placed as a static layout node below the omnibox.
*   **C++ Views**: Built as a separate, floating frameless window (`views::Widget`) that is dynamically positioned and resized directly below the location bar to sit above other web contents without clipping.
*   **WebUI**: Embedded directly as a standard shadow DOM child element (`<cr-searchbox-dropdown>`) inside the parent `<omnibox-popup-searchbox>` input host, flowing inline within the HTML renderer.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Active Highlighting Model
*   **Figma**: Handled statically.
*   **C++ Views**: Leverages custom selection managers. When users press Up/Down keys, the active row's background dynamically shifts to `kColorOmniboxResultsBackgroundHovered` or `kColorOmniboxResultsBackgroundSelected` to preserve accessible focus.
*   **WebUI**: Handled reactively using the `selectedMatchIndex` property in `<cr-searchbox-dropdown>`, setting active classes on child nodes.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Clean List Margins**: Ensure text overflow uses ellipses (`text-overflow: ellipsis`) to prevent long URLs from breaking column lines.
*   **Navigability**: Keyboard arrow triggers must smoothly navigate the list entries.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Mojo IPC Coordination**: The WebUI dropdown syncs dynamic list choices directly with the native C++ Autocomplete controller via Mojo bindings inside `searchbox.mojom`.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::WidgetDelegateView (Popup overlay manager)
              └── OmniboxPopupViewViews (Popup suggestion overlay)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── LitElement / CrLitElement (Web UI host)
              └── OmniboxPopupSearchboxElement (Custom WebUI searchbox container)
                     └── SearchboxDropdownElement (cr-searchbox-dropdown suggestion wrapper)
    ```
