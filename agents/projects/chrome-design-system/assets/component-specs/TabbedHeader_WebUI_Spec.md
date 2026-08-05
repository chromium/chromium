# Component Spec: TabbedHeader

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **TabbedHeader** component (implemented as `<cr-tabs>` in the Chromium codebase) across Figma and WebUI (Web Frontend).

---

## Overview

The `TabbedHeader` is a horizontal tab selection container used to partition views and categories on WebUI pages. It supports two main modes: "Left Aligned" layout (featuring icons and specific margins) and "Full Width" layout (for centered, text-only tabs).

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Component Name** | `TabbedHeader` | `<cr-tabs>` |
| **Source Files** | [Figma Link: `21524:46500`](https://www.figma.com/design/ZRB6863VRSstVNLN6WI3Pt/CDDS-Design-Kit---Settings--Chrome-?node-id=21524-46500&m=dev) | [ui/webui/resources/cr_elements/cr_tabs/cr_tabs.ts](//src/ui/webui/resources/cr_elements/cr_tabs/cr_tabs.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

*   **Left Aligned**:
    *   Figma: `type="Left Aligned"`
    *   WebUI: `<cr-tabs>` with left alignment. Tab contents are aligned to the left of the page with a gap between tabs (e.g. `gap: 32px`).
    *   Features: Includes icons before the text (e.g. `list` and `conversion_path`).
    *   Divider: A full-width `1px` horizontal separator (`var(--desktop/sys/surface-colors/surface-variant, #e1e3e1)`) spans the bottom.
*   **Full Width**:
    *   Figma: `type="Full Width"`
    *   WebUI: `<cr-tabs>` with custom styles setting `--cr-tabs-flex: 1`. Tabs are stretched equally to fill the entire horizontal space.
    *   Features: Text-only labels without icons.
*   **Selected / Active**:
    *   Active tabs display a prominent blue color (`var(--desktop/sys/primary-colors/primary, #0b57d0)`) and a solid bottom indicator bar.
    *   Inactive tabs display a neutral grey color and have no bottom indicator bar.

---

## 3. Component States

| State | Figma Property | WebUI CSS / State Property |
| :--- | :--- | :--- |
| **Default (Inactive)** | `state="Default"` | `.tab` / unselected styling |
| **Selected (Active)** | `state="Active"` | `.tab.selected` selected color and indicator |
| **Hover** | — | Hover state transitions |
| **Focused** | — | `.tab:focus` applies focus outline under `.focus-outline-visible` |

---

## 4. Design Token Comparison (Side-by-Side)

| Visual Property | Figma Design Token | WebUI CSS / Custom Property |
| :--- | :--- | :--- |
| **Tab Container Height** | `48px` | `var(--cr-tabs-height, 48px)` |
| **Tab Font (Left Aligned)** | `family: "typeface/Headline" (Roboto:Medium)`, Weight: `500`, Size: `14px` | `var(--cr-tabs-font-size, 14px)`, `font-weight: 500` |
| **Tab Font (Full Width)** | `family: "typeface/Headline" (Roboto:Medium)`, Weight: `500`, Size: `13px` | `var(--cr-tabs-font-size, 14px)`, `font-weight: 500` |
| **Indicator Height (Left Aligned)** | `3px` | `var(--cr-tabs-selection-bar-width, 2px)` |
| **Indicator Height (Full Width)** | `2px` | `var(--cr-tabs-selection-bar-width, 2px)` |
| **Indicator Shape (Radius)** | `3px` (Left Aligned) / `2px` (Full Width) | `var(--cr-tabs-selection-bar-radius, 2px)` |
| **Selected Color** | `var(--desktop/sys/primary-colors/primary, #0b57d0)` | `var(--cr-tabs-selected-color, var(--google-blue-600))` |
| **Unselected Color** | `var(--desktop/sys/surface-colors/on-surface-subtle, #474747)` | `var(--cr-secondary-text-color)` |
| **Bottom Separator (Divider)** | `var(--desktop/sys/surface-colors/surface-variant, #e1e3e1)`, height `1px` | Typically rendered via parent wrapper border or sibling element |

---

## 5. Architectural & Implementation Gaps

*   **Selection Bar Thickness**:
    *   *Design Intent*: Figma uses a `3px` tall selection indicator for Left Aligned tabs and `2px` for Full Width.
    *   *Implementation*: WebUI utilizes a single `--cr-tabs-selection-bar-width` property defaulting to `2px`. By default, Left Aligned tabs will render with a slightly thinner selection bar (`2px`) than designed, unless customized.
*   **Font Size Inconsistency**:
    *   *Design Intent*: Left Aligned tab text is `14px` (`heading-four`) and Full Width tab text is `13px` (`heading-five`).
    *   *Implementation*: WebUI applies a single `var(--cr-tabs-font-size, 14px)` to all tabs, making the Full Width tab labels slightly larger than the Figma design.
*   **Divider Ownership**:
    *   *Design Intent*: The `TabbedHeader` contains a bottom separator divider internally.
    *   *Implementation*: `<cr-tabs>` does not include a bottom border or divider line in its styling. Consumers must add this separator externally in the parent page layout.

---

## 6. Styling, Variants, Features and States Mismatches

*   **Dynamic Underline Animation**:
    *   WebUI `<cr-tabs>` implements a physics-based slide/expand transition animation when switching selections. This interactive behavior is not depicted in static Figma designs.
*   **Icon Asset Masking**:
    *   WebUI loads tab icons through CSS `-webkit-mask-image` mask properties to allow CSS color tokens to colorize the icon dynamically on selection. This requires specifying corresponding assets in the component's `tabIcons` list.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   Use tabs to switch views within the same logical container level.
*   Avoid mixing icon-carrying tabs with text-only tabs inside the same header block.
*   Keep tab labels brief and descriptive.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   Ensure `<cr-tabs>` has `role="tablist"`, and each tab item has `role="tab"`.
*   Maintain keyboard navigation compatibility: users can navigate through tab list items using `ArrowLeft`, `ArrowRight`, `Home`, and `End` keys.
*   Update `aria-selected` dynamically on tab selection changes.

---

## 8. Inheritance Structure

*   **WebUI (Web Frontend)**:
    ```
    HTMLElement
       └── CrLitElement
             └── CrTabsElement (Manages tab selections, render loops, and keyboard event handlers)
    ```
