# Component Spec: WebUiHeader

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **WebUiHeader** component (implemented as `<cr-toolbar>` and `<cr-toolbar-search-field>` in the Chromium codebase) across Figma and WebUI (Web Frontend).

---

## Overview

The `WebUiHeader` is a branding, navigation, and search header component used at the top of utility and settings pages in Chromium. It contains the application branding (Chrome channel logo and title), a collapsible responsive search field, and slots for contextual action buttons or navigation controls.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Component Name** | `WebUiHeader` | `<cr-toolbar>` and `<cr-toolbar-search-field>` |
| **Source Files** | [Figma Link: `16261:43506`](https://www.figma.com/design/ZRB6863VRSstVNLN6WI3Pt/CDDS-Design-Kit---Settings--Chrome-?node-id=16261-43506&m=dev) | [ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar.ts)<br>[ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts](//src/ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

The component adapts responsively based on the viewport width and state parameters:

*   **Desktop (Wide)**:
    *   Figma: `breakpoint="Desktop"`
    *   WebUI: `<cr-toolbar>` (when viewport is above `narrow-threshold`).
    *   Layout: Displays the logo and page title on the far left, the search field centered (width: 680px), and right-hand options if any.
*   **Desktop-Small (Narrow)**:
    *   Figma: `breakpoint="Desktop-Small"`
    *   WebUI: `<cr-toolbar narrow>` (activated when viewport width matches `narrow-threshold`).
    *   Layout: Shows the navigation menu drawer button (`#menuButton`) on the left, hides logo/title when search is active, and displays search as a collapsible icon button.
*   **Search Active / Inputting**:
    *   Figma: `state="Focus"`
    *   WebUI: `<cr-toolbar-search-field showing-search>` is true. Hides the "Search settings" placeholder text and displays the clear button (`#clearSearch`) with a cancel icon.

---

## 3. Component States

| State | Figma Property | WebUI CSS / State Property |
| :--- | :--- | :--- |
| **Default** | `state="Default"` | `:host` / standard rendering |
| **Hover (Search Field)** | — | `:host(:hover:not([search-focused_])) #stateBackground` background hover transition |
| **Focused (Search Field)** | `state="Focus"` | `:host([search-focused_])` applies focus outline |
| **Scrolled Header** | `state="Scrolled"` | Sibling `#scrollableShadow` has class `.cr-scrollable-top-shadow` with opacity 1 |
| **Narrow Menu Button Active** | `breakpoint="Desktop-Small"` | Menu button displayed; fires `cr-toolbar-menu-click` upon interaction |

---

## 4. Design Token Comparison (Side-by-Side)

| Visual Property | Figma Design Token | WebUI CSS / Custom Property |
| :--- | :--- | :--- |
| **Header Height** | `56px` | `var(--cr-toolbar-height, 56px)` |
| **Header Title Font** | `family: "typeface/Title" (Roboto:Medium)`, Weight: `500`, Size: `22px` | `font-size: 170%`, `font-weight: var(--cr-toolbar-header-font-weight, 500)` |
| **Search Container Width** | `680px` | `var(--cr-toolbar-field-width, 680px)` |
| **Search Container Height**| `36px` | `height: 36px` |
| **Search Corner Radius** | `100px` | `--cr-toolbar-search-field-border-radius: 100px` |
| **Search Normal BG** | `var(--desktop/sys/base-colors/base-container, #edf2fa)` | `var(--cr-toolbar-search-field-background, var(--color-toolbar-search-field-background, var(--cr-fallback-color-base-container)))` |
| **Search Normal Text** | `family: "typeface/Body" (Roboto:Regular)`, Weight: `400`, Size: `12px`, Line-height: `20px` | `font-size: 12px`, `font-weight: 500`, `line-height: 185%` |
| **Search Focus Outline** | `border-2 border-[var(--desktop/sys/state-colors/state-focus-ring,#0b57d0)]` | `outline: 2px solid var(--cr-focus-outline-color); outline-offset: 2px` |

---

## 5. Architectural & Implementation Gaps

*   **Scrolled Header Elevation / Shadow**:
    *   *Design Intent*: The header itself changes state to `Scrolled` and gains a shadow/elevation effect (`settings elevations/+3`).
    *   *Implementation*: `<cr-toolbar>` stays flat and does not contain shadow styling. Instead, a sibling elements in the parent page layout (e.g. `div#scrollableShadow` in `settings_ui.html`) applies top inner shadow dynamically through CSS container queries anchored to the scrolling panel.
*   **Search Text Weight**:
    *   *Design Intent*: Uses regular weight (`400`) from the `Body/Regular` token.
    *   *Implementation*: Hardcoded to `font-weight: 500` (Medium) in `cr_toolbar_search_field.css`, causing the search text to look slightly bolder.
*   **Search Text Line Height**:
    *   *Design Intent*: Line height matches the body-regular font token (`20px`).
    *   *Implementation*: WebUI sets `line-height: 185%` (~22px) on the input field container, creating a minor height variance.

---

## 6. Styling, Variants, Features and States Mismatches

*   **Focus Ring Bounds**:
    *   Figma shows a custom focus ring offset rectangle (`left-[-2.5px] right-[-2.5px] top-[-2px] h-[40px]`).
    *   WebUI implements this with native CSS `outline` styling and standard `outline-offset: 2px` around the search container bounds, which aligns well with standard focus behavior.
*   **Search Input Narrow Mode Transition**:
    *   In WebUI, clicking search in narrow mode causes the input box to transition to full width and hides branding. This requires dynamic state synchronization (`showing-search_` attribute) in the Lit element class.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   Position the header as sticky at the top of the viewport layout.
*   The page title should use standard, localized title casing (e.g., "Settings").
*   Use the search input field for instant page filtering or to route query parameters to secondary pages.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   Ensure `<cr-toolbar>` has `role="banner"`.
*   Support native keyboard shortcuts: pressing `Escape` clears the input text, closes narrow search mode, and blurs the input element.
*   Ensure all buttons (menu drawer, search clear) are fully keyboard-navigable and have aria labels defined (`aria-label`, `aria-description`).

---

## 8. Inheritance Structure

*   **WebUI (Web Frontend)**:
    ```
    HTMLElement
       └── CrLitElement
             ├── CrToolbarElement (Layout container, titles, narrow breakpoint tracking)
             └── CrSearchFieldMixinLit (Common search state properties and query handlers)
                    └── CrToolbarSearchFieldElement (Input element, focus outlines, clear actions)
    ```
