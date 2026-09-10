# Component Spec: Search Bar

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Search Bar** component (referred to as
`.base.SearchBar` or `Search Bar` in Figma) across Figma and WebUI (Desktop).

______________________________________________________________________

## Overview

The **Search Bar** is a compact, pill-shaped input component used across
Chromium Desktop WebUI interfaces—including Settings subpages, modal dialogs,
side panels, and utility pages—to enable real-time incremental filtering and
search queries.

The component encapsulates a leading magnifying glass search icon, an input
field with placeholder support, and a conditional trailing clear button that
appears when query text is entered.

In the Chrome Design System (CDS), the **Search Bar** is defined with two
primary functional states:

1. **DEFAULT STATE (`State=Default`)**: The initial empty state displaying a
   leading 20px search icon and subtle placeholder text ("Search"). The trailing
   clear button is hidden.
2. **ENTERED TEXT STATE (`State=Entered Text`)**: The active state containing
   user input rendered in primary text color, displaying an interactive trailing
   clear icon button (`cancel`) for one-click query reset.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                         | C++ Views (Desktop) | WebUI (Desktop)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | Clank (Android) |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :------------------ | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------- |
| **Component Name** | `Search Bar`                                                                                                                                            | **N/A**             | `<cr-search-field>`<br>(and `<cr-toolbar-search-field>`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     | **N/A**         |
| **Source Files**   | [Figma Link: `280:26687`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=280-26687&t=H2HK5dc8ofgXglfl-11) | **N/A**             | [ui/webui/resources/cr_elements/cr_search_field/cr_search_field.ts](//src/ui/webui/resources/cr_elements/cr_search_field/cr_search_field.ts)<br>[ui/webui/resources/cr_elements/cr_search_field/cr_search_field.html.ts](//src/ui/webui/resources/cr_elements/cr_search_field/cr_search_field.html.ts)<br>[ui/webui/resources/cr_elements/cr_search_field/cr_search_field.css](//src/ui/webui/resources/cr_elements/cr_search_field/cr_search_field.css)<br>[ui/webui/resources/cr_elements/cr_search_field/cr_search_field_mixin_lit.ts](//src/ui/webui/resources/cr_elements/cr_search_field/cr_search_field_mixin_lit.ts) | **N/A**         |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant                | Figma Component                                  | WebUI (Desktop) Implementation                                                                                                           |
| :------------------------------- | :----------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **Search Pill Container**        | Fully rounded pill container (h: 36px, r: 999px) | Host or `#searchInput` container inside `<cr-search-field>` styled with `--cr-input-border-radius: 999px` (or 100px) and `height: 36px`. |
| **Leading Search Icon**          | 20px vector icon (`search`)                      | `<cr-icon id="searchIconInline" slot="inline-prefix" icon="cr:search">` or `<cr-icon id="searchIcon" icon="cr:search">` with size 20px.  |
| **Input Text Field**             | Text field (12px medium, line-height: 18px)      | Inner `<cr-input id="searchInput" type="search">` wrapping standard HTML `<input type="search">` with `spellcheck="false"`.              |
| **Trailing Clear Button**        | 20px vector icon button (`cancel`)               | `<cr-icon-button id="clearSearch" class="icon-cancel" slot="suffix" ?hidden="${!this.hasSearchText}">` invoking `onClearSearchClick_()`. |
| **Incremental Query Debouncing** | Modeled as static `Default` and `Entered Text`   | Handled automatically by `CrSearchFieldMixinLit`: schedules search event dynamically (0ms for empty to 200–500ms based on length).       |
| **Width & Responsiveness**       | Default width: `320px`                           | Configured via `--cr-search-field-input-width` (default 160px, overridable to `100%` or `320px`).                                        |

______________________________________________________________________

## 3. Component States

| State                            | Figma Property       | WebUI CSS / State Property                                                                                                               |
| :------------------------------- | :------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **Default (Empty)**              | `State=Default`      | `:host:not([has-search-text])`: Clear button is hidden (`?hidden`), placeholder prompt is visible.                                       |
| **Entered Text (Active Search)** | `State=Entered Text` | `:host([has-search-text])`: Clear button is rendered (`#clearSearch`), placeholder hidden, value displayed in `--cr-primary-text-color`. |
| **Hovered**                      | —                    | `:host(:hover) #searchInput`: Subtle background highlight layer.                                                                         |
| **Focused**                      | —                    | `:host(:focus-within)`: Renders outline `2px solid var(--cr-focus-outline-color)` with `outline-offset: 2px`.                            |
| **Disabled**                     | —                    | `:host([disabled])`: Applies `opacity: var(--cr-disabled-opacity)` and disables pointer events.                                          |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                                                   | WebUI CSS / Custom Property                                                            |
| :------------------------------- | :----------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------- |
| **Search Pill Height**           | `36px`                                                                               | `height: 36px` / `--cr-input-min-height: 36px`                                         |
| **Corner Radius**                | `999px` (`--desktop/corner-radius/fully-rounded`)                                    | `border-radius: 999px` / `--cr-toolbar-search-field-border-radius: 100px`              |
| **Pill Background**              | `--desktop/sys/base-colors/base-container-elevated` (#ffffff)                        | `var(--color-toolbar-search-field-background, white)` / `--cr-search-field-background` |
| **Outer Wrapper Background**     | `--desktop/sys/base-colors/base-container` (#edf2fa)                                 | `var(--cr-fallback-color-base-container)` / `--color-side-panel-header-background`     |
| **Outer Container Padding**      | `8px` (`--desktop/spacing/8`)                                                        | `padding: 8px`                                                                         |
| **Inner Pill Padding**           | Horizontal: `10px` (`--desktop/spacing/10`), Vertical: `4px` (`--desktop/spacing/4`) | `--cr-input-padding-start: 10px;` `--cr-input-padding-end: 10px;`                      |
| **Search Icon Size**             | `20px` x `20px`                                                                      | `--cr-icon-size: 20px;`                                                                |
| **Search Icon Color**            | `--desktop/sys/surface-colors/on-surface-subtle` (#474747)                           | `--cr-search-field-search-icon-fill: var(--cr-secondary-text-color)`                   |
| **Clear Icon Size**              | `20px` x `20px`                                                                      | `--cr-search-field-clear-icon-size: 20px;`                                             |
| **Clear Button Hit Target Size** | `20px`–`24px`                                                                        | `--cr-search-field-clear-button-size: 24px;`                                           |
| **Clear Icon Color**             | `--desktop/sys/surface-colors/on-surface-subtle` (#474747)                           | `--cr-search-field-clear-icon-fill: var(--cr-secondary-text-color)`                    |
| **Font Family**                  | `Google Sans Text` (`--desktop/font/body`, `"Google_Sans_Text:Medium"`)              | `font-family: inherit` (Roboto / Google Sans)                                          |
| **Font Size**                    | `12px` (`--desktop/font_size/body-four`)                                             | `font-size: 12px;` (or `92.3076923%`)                                                  |
| **Line Height**                  | `18px` (`--desktop/line_height/body-four`)                                           | `line-height: 18px;`                                                                   |
| **Font Weight**                  | Medium (500) (`--desktop/font_weight/medium`)                                        | `font-weight: 500;`                                                                    |
| **Placeholder Color**            | `--desktop/sys/surface-colors/on-surface-subtle` (#474747)                           | `--cr-search-field-placeholder-color: var(--cr-secondary-text-color)`                  |
| **Entered Text Color**           | `--desktop/sys/surface-colors/on-surface` (#1f1f1f)                                  | `color: var(--cr-primary-text-color)`                                                  |
| **Focus Outline**                | —                                                                                    | `outline: 2px solid var(--cr-focus-outline-color); outline-offset: 2px;`               |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Element Composition & Nesting

- In Figma, the component is rendered as a standalone pill layer with vector
  icons and text labels.
- In WebUI, `<cr-search-field>` is composed of multiple nested elements: a
  `<cr-input>` (which wraps a native `<input type="search">`), an inline
  `<cr-icon>` prefix, and an absolutely positioned `<cr-icon-button>` suffix for
  clearing.

### 2. Underline vs. Fully-Rounded Pill Styling

- By default, `<cr-search-field>` historically defaults to an underline styling
  (`--cr-search-field-input-border-bottom: 1px solid var(--cr-secondary-text-color)`).
- To match the Chrome Design System specification, consumers must apply pill
  styling variables:
  ```css
  cr-search-field {
    --cr-search-field-input-border-bottom: none;
    --cr-search-field-search-icon-display: none;
    --cr-search-field-search-icon-inline-display: block;
    --cr-input-border-radius: 999px;
    --cr-input-background-color: var(--color-toolbar-search-field-background, white);
  }
  ```
- Alternatively, `<cr-toolbar-search-field>` already has native 100px pill
  styling and background elevation built-in for top-level toolbars.

### 3. Asynchronous Debounce Behavior

- Figma depicts discrete visual states (`Default` and `Entered Text`).
- In WebUI, `CrSearchFieldMixinLit` introduces asynchronous debounce logic to
  avoid triggering expensive search filter queries on every keystroke:
  - 0ms for clearing/empty
  - 500ms for single character
  - 400ms for 2 characters
  - 300ms for 3 characters
  - 200ms for 4+ characters

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Icon Assets

- Figma specifies the Material Symbols `cancel` glyph (circle with 'X') for the
  clear button.
- In WebUI, the clear button uses `<cr-icon-button class="icon-cancel">` which
  maps to `cr:cancel`.

### 2. Clear Button Hit Area

- Figma specifies a 20x20px clear icon.
- WebUI implements `--cr-search-field-clear-button-size: 24px` with internal
  icon centering to ensure an adequate interactive click target that satisfies
  accessibility requirements.

### 3. Clear Button Focus Recovery

- When clicking the clear button in WebUI, focus must return immediately to the
  search input (`this.$.searchInput.focus()`) to permit continuous typing
  without extra clicks.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Concise Placeholders**: Use clear, succinct placeholder prompts such as
  "Search", "Search settings", or "Filter extensions".
- **Instant Results**: Bind to the `search-changed` event to update filtered
  collections incrementally without requiring an Enter keypress.
- **One-Click Reset**: Provide the clear button as soon as search text is
  present so users can reset their query instantly.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Input Attributes**: Ensure the inner input element has `type="search"`,
  `spellcheck="false"`, and appropriate `aria-label`.
- **Clear Button Label**: Supply `clearLabel="$i18n{clearSearch}"` so screen
  readers announce an accessible name for the clear button.
- **Escape Key Handling**: Pressing `Escape` while focused inside the input
  field should clear the query text.
- **Focus Indicators**: Always maintain a visible focus ring
  (`--cr-focus-outline-color`) when focused via keyboard navigation.

### 3. Icon Usage Guidelines

- **Leading Search Icon**: Use `cr:search` as a decorative affordance with
  secondary fill color (`--cr-secondary-text-color`).
- **Trailing Clear Icon**: Use `cr:cancel` within `<cr-icon-button>` for the
  dismissal action.
