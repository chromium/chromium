# Component Spec: Text Fields (WebUI)

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Text Fields (WebUI)** component in Figma and Chromium WebUI.

---

## Overview

The **Text Fields (WebUI)** component is a filled text input control with a flat bottom outline (underline), designed to capture user inputs in the Chromium WebUI frontend. It features rounded top corners (8px) and a flat bottom border that animates to a 2px focused highlight upon user selection. It supports optional helper labels, slot-based inline leading/trailing icons, disabled container styles, and validation error treatments.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Component Name** | `Text Fields (WebUI)` | `<cr-input>` |
| **Source Files** | [Figma Link: `280:26417`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26417&m=dev) | [ui/webui/resources/cr_elements/cr_input/cr_input.ts](//src/ui/webui/resources/cr_elements/cr_input/cr_input.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Filled (Underlined) Input** | Default filled/underlined variant | Standard `<cr-input>` default styling |
| **Stroked (Outlined) Input** | N/A *(Not in this frame)* | `<cr-input class="stroked">` |
| **Inline Prefix Icon** | LHS Icon slot containing "sync" | `<div slot="inline-prefix">` with a `<cr-icon>` or custom svg icon |

---

## 3. Component States

| State | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Default (Normal)** | `State=Default` | Idle state, background `--cr-input-background-color` with 1px border-bottom underline |
| **Hovered** | *(Inferred/Common)* | Hovering `#input-container` displays the `#hover-layer` overlay styled via `--cr-input-hover-background-color` |
| **Disabled** | `State=Disabled` | Property: `<cr-input disabled>` reflected to host attribute `[disabled]`, overlay gets full disabled colors |
| **Selected (Focused)**| `State=Selected` | Triggers Lit state `[focused_]` which transitions `#underline` opacity to 1 and width to 100% (2px solid highlight) |
| **Error (Invalid)** | `State=Error` | Attribute: `<cr-input invalid>` reflected to host `[invalid]`, coloring the label, input caret, underline, and icons to `--cr-input-error-color` |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | WebUI (Web Frontend) CSS Custom Properties |
| :--- | :--- | :--- |
| **Default Underline Base** | `--desktop/sys/outline-colors/neutral-outline` (via asset) | `--cr-input-border-bottom` <br> *(Defaults to `1px solid var(--color-textfield-filled-underline, var(--cr-fallback-color-outline))`)* |
| **Focus Underline Color** | `--desktop/sys/state-colors/state-focus-ring` | `--cr-input-focus-color` <br> *(Defaults to `var(--color-textfield-filled-underline-focused, var(--cr-fallback-color-primary))`)* |
| **Error Underline Color** | `--desktop/sys/error-colors/error` (`#b3261e`) | `--cr-input-error-color` <br> *(Defaults to `var(--color-textfield-filled-error, var(--cr-fallback-color-error))`)* |
| **Disabled Underline Color**| `rgba(31,31,31,0.38)` (via asset) | `1px solid currentColor` *(derived via host `[disabled]` opacity)* |
| **Background Color** | `--desktop/sys/surface-colors/surface-variant` (`#e1e3e1`) | `--cr-input-background-color` <br> *(Defaults to `var(--color-textfield-filled-background, var(--cr-fallback-color-surface-variant))`)* |
| **Disabled Container BG** | `--desktop/sys/state-colors/state-disabled-container` (`rgba(31,31,31,0.12)`) | `--color-textfield-background-disabled` / `--cr-fallback-color-disabled-background` |
| **Base On-Surface Text** | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`) | `--cr-input-color` <br> *(Defaults to `var(--cr-primary-text-color)`)* |
| **Disabled Text Color** | `--desktop/sys/state-colors/state-disabled` (`rgba(31,31,31,0.38)`) | `--color-textfield-foreground-disabled` / `--cr-fallback-color-disabled-foreground` |
| **Label Color (Normal)** | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`) | `--cr-input-label-color` <br> *(Defaults to `var(--color-textfield-foreground-label, var(--cr-fallback-color-on-surface-subtle))`)* |
| **Label Color (Error)** | `--desktop/sys/error-colors/error` (`#b3261e`) | `--cr-input-error-color` |
| **Corner Radius** | `--desktop/corner-radius/8` (`8px` top corners) | `--cr-input-border-radius` <br> *(Defaults to `8px 8px 0 0`)* |
| **Inner Padding (Horizontal)**| `--desktop/spacing/10` (`10px`) | `--cr-input-padding-start`, `--cr-input-padding-end` <br> *(Defaults to `10px`)* |
| **Inner Padding (Vertical)** | `--desktop/spacing/9` (`9px`) | `--cr-input-padding-top`, `--cr-input-padding-bottom` <br> *(Defaults to `10px`)* |
| **Font Size (Value)** | `--desktop/font_size/body-four` (`12px`) | `--cr-input-font-size` <br> *(Defaults to `12px`)* |
| **Label Font Size** | `--desktop/font_size/body-five` (`11px`) | `font-size: 11px;` *(Hardcoded in `#label`)* |
| **Label Line Height** | `--desktop/line_height/body-five` (`16px`) | `line-height: 16px;` *(Hardcoded in `#label`)* |

---

## 5. Architectural & Implementation Gaps

### 1. Hardcoded Label Styling
While layout paddings, fonts, and container properties are extensively tokenized via CSS variables, the font properties of `#label` (such as `font-size: 11px;` and `line-height: 16px;`) are hardcoded in `cr_input_style_lit.css`. These map directly to the Figma `--desktop/font_size/body-five` and `--desktop/line_height/body-five` design tokens, but could cause scaling inconsistencies if the parent context font changes.

### 2. Minor Padding Difference
Figma defines the vertical inner padding of the text input container as `9px` (`--desktop/spacing/9`), while the default `<cr-input>` padding defaults to `10px` (`--cr-input-padding-top` / `--cr-input-padding-bottom`). This results in a minor height layout deviation of 2px.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Height Discrepancy
*   **Figma**: The text field height is modeled as exactly `60px` overall, which includes the label height (`24px` including padding/margin) and the container height (`36px`).
*   **WebUI**: The height of `<cr-input>` is dynamic and expands depending on the presence of labels, input lines, error blocks, or suffixes. A default idle `<cr-input>` with a label is typically around `60px` in visual representation, but does not enforce a strict CSS `height: 60px;` unless custom heights are set via CSS classes.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Label Visibility**: Always set the `label` attribute on `<cr-input>` components to describe the expected input, rather than relying solely on placeholders.
*   **Validation**: Use `auto-validate` alongside the `pattern` and `required` attributes to trigger error feedback automatically.
*   **Error Messaging**: Pair `invalid` with a descriptive string in `error-message`. This fills the `#error` block below the input, ensuring screen readers announce the message correctly.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Semantic Roles**: The underlying `<input>` inherits native interactive focus states, supporting keyboard navigation (`Tab` index navigation) and input method engines.
*   **ARIA bindings**: The component automatically links labels and inputs via `aria-labelledby`, ensuring high accessibility compliance across screen readers.

---

## 8. Inheritance Structure

*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── CrLitElement (Lit-element base)
              └── CrInputElement (Reusable cr-input component wrapper)
    ```
