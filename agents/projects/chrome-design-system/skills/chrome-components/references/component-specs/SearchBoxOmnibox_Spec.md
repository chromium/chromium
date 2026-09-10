# Component Spec: SearchBoxOmnibox

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **SearchBoxOmnibox** (Autocomplete
Suggestions Dropdown) component across Figma, C++ Views (Desktop), WebUI
(Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **SearchBoxOmnibox** component represents the autocomplete suggestion popup
container that expands downwards below the Omnibox. It populates dynamic search
terms, favicon cards, browser navigation history nodes, bookmarks, and site
links as the user types queries.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                       | C++ Views (Desktop)                                                                                                            | WebUI (Desktop)                                                                                                                                                                                                                                                                            | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `SearchBoxOmnibox`                                                                                                                    | `views::OmniboxPopupViewViews`                                                                                                 | `<omnibox-popup-searchbox>` / `<cr-searchbox-dropdown>`                                                                                                                                                                                                                                    | `AutocompleteCoordinator` / `OmniboxSuggestionsDropdown`                                                                                                                                                                                                                                                                                                                                                                                       |
| **Source Files**   | [Figma Link: `34968:6820`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=34968-6820) | [chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h](//src/chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h) | [chrome/browser/resources/omnibox_popup/omnibox_popup_searchbox.ts](//src/chrome/browser/resources/omnibox_popup/omnibox_popup_searchbox.ts)<br>[ui/webui/resources/cr_components/searchbox/searchbox_dropdown.ts](//src/ui/webui/resources/cr_components/searchbox/searchbox_dropdown.ts) | [chrome/android/java/src/org/chromium/chrome/browser/omnibox/suggestions/AutocompleteCoordinator.java](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/suggestions/AutocompleteCoordinator.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/omnibox/suggestions/OmniboxSuggestionsDropdown.java](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/suggestions/OmniboxSuggestionsDropdown.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant      | Figma Component                      | C++ Views (Desktop)                             | WebUI (Web Frontend)                      | Clank (Android)                               |
| :--------------------- | :----------------------------------- | :---------------------------------------------- | :---------------------------------------- | :-------------------------------------------- |
| **Search List Layout** | Contains suggestion rows             | Instantiates `OmniboxResultView` entries        | Hosts dynamic `<cr-searchbox-match>` rows | `OmniboxSuggestionsDropdown` / `ModelList`    |
| **History Icon**       | Uses nested `history` vector symbols | Renders history clocks via custom vector assets | Standard SVG history clock icons          | History icon drawable                         |
| **Favicon Asset**      | Houses favicon container wrapper     | Fetches and renders site-specific icon bitmaps  | Displayed using favicon styling           | `FaviconResolver` / `BaseSuggestionView` icon |

______________________________________________________________________

## 3. Component States

| State                | Figma Component          | C++ Views (Desktop)                          | WebUI (Web Frontend)                              | Clank (Android)                    |
| :------------------- | :----------------------- | :------------------------------------------- | :------------------------------------------------ | :--------------------------------- |
| **Default (Normal)** | `state=Default`          | Base suggestion view rendering               | Default suggestions row style                     | Standard suggestion row            |
| **Hovered**          | `state=Hovered`          | Displays background focus highlights         | `:hover` styled background overlays               | State layer overlay                |
| **Pressed**          | `state=Pressed`          | Triggers navigation click event              | Click/Select trigger transitions                  | Ripple touch effect                |
| **Focused**          | *(Commonly represented)* | Selected row changes background highlighting | `selected` property updates active row background | Keyboard/TalkBack active highlight |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute          | Figma Design Token                                              | C++ Views (Desktop)                            | WebUI (Web Frontend)                  | Clank (Android) Equivalent                                  |
| :------------------------ | :-------------------------------------------------------------- | :--------------------------------------------- | :------------------------------------ | :---------------------------------------------------------- |
| **Dropdown Container BG** | `--desktop/sys/surface-colors/surface`<br>(`white`)             | `kColorOmniboxResultsBackground`               | `--cr-menu-background-color`          | `@macro/omnibox_suggestion_bg_color` / `?attr/colorSurface` |
| **On-Surface Text**       | `--desktop/sys/surface-colors/on-surface`<br>(`#1f1f1f`)        | `kColorOmniboxResultsText`                     | `--cr-primary-text-color`             | `@macro/default_text_color` / `?attr/colorOnSurface`        |
| **Dimmed Text Color**     | `--desktop/sys/surface-colors/on-surface-subtle`<br>(`#474747`) | `kColorOmniboxResultsTextDimmed`               | `--cr-secondary-text-color`           | `@macro/default_text_color_secondary`                       |
| **Corner Radius**         | `--desktop/corner-radius/16`<br>(`16px`)                        | Matches popup widget corner radii specs        | `border-radius: 12px;` or `16px;`     | `@dimen/omnibox_suggestion_corner_radius`                   |
| **Elevation Drop Shadow** | `--desktop/elevation/4`<br>(Dual drop shadows)                  | Handled dynamically by native window elevation | Handled via CSS shadow/elevation maps | `@dimen/omnibox_suggestion_elevation`                       |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Overlay Layout Pipeline

- **C++ Views**: Built as a separate, floating frameless window
  (`views::Widget`) positioned below the location bar.
- **WebUI**: Embedded directly as a shadow DOM child element
  (`<cr-searchbox-dropdown>`).
- **Clank (Android)**: Built as an Android `RecyclerView`
  (`OmniboxSuggestionsDropdown`) powered by MVC ModelList adapters and
  `PropertyModel` view binders.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Active Highlighting Model

- **C++ Views**: Shifts row background to
  `kColorOmniboxResultsBackgroundHovered` or
  `kColorOmniboxResultsBackgroundSelected`.
- **WebUI**: Sets `selectedMatchIndex` property in `<cr-searchbox-dropdown>`.
- **Clank (Android)**: Uses `BaseSuggestionView` touch states and TalkBack
  accessibility traversal.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Clean List Margins**: Ensure text overflow uses ellipses to prevent long
  URLs from breaking layout rows.
- **Navigability**: Smooth arrow key and touch fling interactions.

______________________________________________________________________
