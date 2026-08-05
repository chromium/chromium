# Component Spec: TextArea (WebUI-only)

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **TextArea** component across Figma and WebUI (Web Frontend).

---

## Overview

The `<cr-textarea>` is a WebUI custom element styled after Google Material Design text inputs. It supports entering multiple lines of text, with attributes for auto-growing vertically (`autogrow`), setting labels, setting character limits, and indicating invalid inputs. It uses LitElement templates and standard custom CSS variables matching CSS variables from `cr_shared_vars.css`.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Component Name** | `.base.TextArea(WebUI)` | `<cr-textarea>` |
| **Source Files** | [Figma Link: `39836:4315`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=39836-4315&t=KdwbLO7J6jWrzj20-11) | [ui/webui/resources/cr_elements/cr_textarea/cr_textarea.ts](//src/ui/webui/resources/cr_elements/cr_textarea/cr_textarea.ts) |

> [!NOTE]
> `<cr-textarea>` shares core styles and CSS variables with `<cr-input>` by importing and including `cr-input-style-lit` inside its CSS stylesheet.

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Property / Slot | WebUI Custom Element Attribute | Description |
| :--- | :--- | :--- |
| **Label Text** | `label` | Renders a `<div id="label">` above the container showing the label caption. Hidden if not set. |
| **Multiline Text** | `<textarea>` | Uses a standard native `<textarea id="input">` wrapped inside the custom element's shadow root. |
| **Placeholder Text** | `placeholder` | Controls the placeholder attribute on the underlying textarea. |
| **Invalid State** | `invalid` | Boolean attribute indicating a validation error; colors the label, underline, and footers in red. |
| **Auto-grow** | `autogrow` | Boolean attribute enabling vertical growth of the input field via `field-sizing: content` CSS rules. |
| **Disabled** | `disabled` | Boolean attribute that disables the native textarea and changes styles to disabled states. |
| **Read Only** | `readonly` | Boolean attribute that prevents modifications. Rounding corners change to fully-rounded (`8px 8px`). |
| **Supporting Text** | `firstFooter` | Attribute that maps to the text displayed on the lower-left footer slot. |
| **Character Count** | `secondFooter` | Attribute that maps to the text displayed on the lower-right footer slot. |

---

## 3. Component States

| State | Figma Property | WebUI CSS / Custom Elements |
| :--- | :--- | :--- |
| **Default** | `state="Default"` | `<cr-textarea>` (default host)<br>- Background Color: `--cr-input-background-color` (resolves to `--color-textfield-filled-background`, variant `#e1e3e1`) <br>- Underline Base: `--cr-input-border-bottom` (1px solid variant outline) |
| **Selected (Focused)** | `state="Selected"` | `<cr-textarea focused_>` (set when internal textarea gains focus)<br>- Underline: `#underline` transitions to `opacity: 1` and `width: 100%`<br>- Underline Color: `--cr-input-focus-color` (resolves to `--color-textfield-filled-underline-focused`, 2px solid) |
| **Error (Invalid)** | `state="Error"` | `<cr-textarea invalid>` (validation fails)<br>- Element colors: Label, footer text, and bottom underline change to `--cr-input-error-color` (resolves to `--color-textfield-filled-error`, red)<br>- Underline: Active red underline shown at 100% width and 2px thickness |
| **Disabled** | `state="Disabled"` | `<cr-textarea disabled>` (user input deactivated)<br>- Background Color: `--color-textfield-background-disabled`<br>- Text/Label/Underline Color: `--color-textfield-foreground-disabled` |

---

## 4. Design Token Comparison (Side-by-Side)

| Property | Figma Token / Value | WebUI Variable / Value |
| :--- | :--- | :--- |
| **Container Background (Default)** | `var(--desktop/sys/surface-colors/surface-variant)` (#e1e3e1) | `--cr-input-background-color` / `var(--color-textfield-filled-background)` |
| **Container Background (Disabled)** | `var(--desktop/sys/state-colors/state-disabled-container)` | `--color-textfield-background-disabled` / `var(--cr-fallback-color-disabled-background)` |
| **Underline Color (Default)** | Neutral Outline | `--cr-input-border-bottom` / `1px solid var(--color-textfield-filled-underline)` |
| **Underline Color (Focused)** | Focus Ring | `--cr-input-focus-color` / `var(--color-textfield-filled-underline-focused)` |
| **Underline Color (Error/Invalid)** | Error Color | `--cr-input-error-color` / `var(--color-textfield-filled-error)` |
| **Corner Radius** | `var(--desktop/corner-radius/8)` (8px top-left/top-right, 0px bottom) | `--cr-input-border-radius` / `8px 8px 0 0` (changes to `8px 8px` on `[readonly]`) |
| **Typography (Label)** | `desktop/body/five` (Medium weight, 11px size, 16px line-height) | `#label` CSS styles: `font-size: 11px`, `line-height: 16px`, `--cr-input-label-color` |
| **Typography (Input Text)** | `desktop/body/four` (Regular weight, 12px size, 18px line-height) | `#input` CSS styles: `font-size: var(--cr-input-font-size, 12px)`, `line-height: 16px` |
| **Typography (Supporting Text)** | `desktop/body/five` (Regular weight, 11px size, 16px line-height) | `#footerContainer` CSS styles: `font-size: var(--cr-form-field-label-font-size)` / `line-height: var(--cr-form-field-label-line-height)` |
| **Text Color (Default)** | `var(--desktop/sys/surface-colors/on-surface)` (#1f1f1f) | `--cr-input-color` / `var(--cr-primary-text-color)` |
| **Text Color (Disabled)** | `var(--desktop/sys/state-colors/state-disabled)` (rgba(31,31,31,0.38)) | `var(--color-textfield-foreground-disabled)` |
| **Text Color (Error/Invalid)** | `var(--desktop/sys/error-colors/error)` (#b3261e) | `--cr-input-error-color` / `var(--color-textfield-filled-error)` |
| **Padding (All sides)** | `var(--desktop/spacing/10)` (10px) | `--cr-input-padding-top/bottom/start/end: 10px` |

---

## 5. Architectural & Implementation Gaps

*   **Input Line-height**: The Figma visual design specifies `18px` (`desktop/body/four`) line-height for the primary input text area. The WebUI component hardcodes `line-height: 16px` on the `#input` text area element in `cr_input_style_lit.css`, resulting in a slightly tighter line spacing.
*   **Active Underline Transitions**: Figma models static states. The WebUI implementation has dynamic CSS transitions on focus, growing the underline width from `0%` to `100%` and fading in opacity over `120ms` / `180ms`.

---

## 6. Styling, Variants, Features and States Mismatches

*   **Read-Only Corner Radius**: In WebUI, when `readonly` is set, the bottom border underline is completely hidden, and the component corner radius changes from `8px 8px 0 0` to `8px 8px` (all four corners rounded). This variant is not modeled in the Figma CDDS kit.
*   **Horizontal Layout Slots**: The visual specification models a clean text block layout. The underlying WebUI CSS structure supports slots for prefix and suffix elements (`[slot='inline-prefix']`, `[slot='inline-suffix']`), which can influence width alignments and padding properties when used.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- Use `<cr-textarea>` when users need to write multiple lines of text (e.g. feedback comments, settings descriptions).
- Always supply a `label` above the text area for clarity.
- Utilize `autogrow` to allow the field to expand dynamically with content, avoiding cramped scroll zones inside small text boxes.
- Bind the validation state directly to the `invalid` property, and supply the error message through `firstFooter`.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **Keyboard Navigation**:
  - Focus input via `Tab`. Focus transitions to the native inner `<textarea>`.
  - Pressing `Enter` adds a newline character.
- **Accessibility (a11y)**:
  - Aria-label is set automatically using the label string.
  - When disabled, `aria-disabled` is set to `true`.
  - The live-region attribute `aria-live` on the footer container is set to `assertive` when `invalid` is active and `polite` otherwise. This forces screen readers to immediately announce input validation errors.

### 3. Icon Usage Guidelines
- By default, `<cr-textarea>` does not include inline action buttons or icons. If suffix buttons are needed, inject them through standard slots while accounting for spacing variables.

---

## 8. Inheritance Structure

*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Base browser element)
       └── CrLitElement (Lit reactive base class)
              └── CrTextareaElement (Custom element: `<cr-textarea>`)
    ```
