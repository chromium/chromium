---
name: figma-to-webui
description: Recreate Figma design frames as production-ready Chromium WebUI code following Chrome's Lit-element architecture and styling guidelines. Use when asked to create WebUI code from a Figma frame URL.
metadata:
  author: Chrome Design System Team
---

# Figma to WebUI: Code Generation

## Prerequisites
- **Figma MCP Server**: Must be active and configured in the workspace context to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root directory of the Chromium repository source code (`//src/`).

If prerequisites are not met, STOP execution and inform the user.

---

## 1. Discovery & Context Retrieval

When given a Figma design URL (e.g., `https://www.figma.com/design/:fileKey/:fileName?node-id=:nodeId`):

1.  **Extract Parameters**: Extract the `fileKey` and the `nodeId` (replace hyphens with colons, e.g. `54800-5491` becomes `54800:5491`).
2.  **Retrieve Design Context**: Call `get_design_context` with `fileKey` and `nodeId` to fetch the metadata, layout hierarchy, layer styles, and a screenshot of the node.
3.  **Reference Existing Component Specs**: Look up component spec files for relevant components using the `project-knowledge` skill to map Figma components to Chromium WebUI components.
4.  **Reference Token & Font Mapping**: Read the Chrome Design System token mapping from the `project-knowledge` skill to translate Figma variables to equivalent Chromium CSS variables (e.g., `desktop/sys/surface-colors/surface-2` to `--color-sys-surface2`), and translate Figma typography styles to equivalent Chromium font families, sizes, weights, and line heights.

---

## 2. Design-to-Code Gap Auditing

Before writing any code, first check if the user already did an audit of this Figma design against production coding standards within the current chat session's recent history (past 5 commands). If not, perform the audit. Incorporate the feedback from this audit into the following code implementation.

---

## 3. WebUI Component Implementation

### Guidelines

- **Scope**: Focus strictly on building the UI. Do not add functionality beyond what is specified in
the Figma mockup. If the Figma mockup includes a window frame or top Chrome frame, disregard these.
- **Strictly Follow Element Hierarchy**: Strictly mirror the container and node element hierarchy exported from the Figma design context (`get_design_context`). Ensure outer and inner containers maintain their exact nesting relationships. Exception: if a frame contains only one other frame, these frames can be combined; merge their properties.
- **Pure UI Only (No Event Listeners)**: Implement pure UI templates only. Do NOT add interactive event listeners or handlers (e.g., `@click`, `@selected-changed`, `@value-changed`, `@change`, etc.). Bind properties declaratively to pre-populated mock values without mutation handlers.
- **Output Location**: Save the implementation files inside the  out/<ComponentName>  directory
(relative to the skill directory), unless specified otherwise.
- **Builds**: Do not run any builds.

### File 1: `<component_name>.ts`
The main TypeScript file defining the element class. It must:
1.  Extend `CrLitElement`.
2.  Expose the static `is` getter (returning the kebab-case tag name).
3.  Load the CSS and HTML template wrappers:
    ```typescript
    import { getCss } from './<component_name>.css.js';
    import { getHtml } from './<component_name>.html.js';
    ```
4.  Bind static properties pre-populated with the exact values from the Figma mockup (without event handler methods).

*Example Boilerplate:*
```typescript
// other imports go here...
import { CrLitElement } from '//resources/lit/v3_0/lit.rollup.js';

import { getCss } from './my_component.css.js';
import { getHtml } from './my_component.html.js';

export interface MyComponentElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class MyComponentElement extends CrLitElement {
  static get is() {
    return 'my-component';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      value_: {type: String},
    };
  }

  protected accessor value_: string = 'Default Mock Value';
}

customElements.define(MyComponentElement.is, MyComponentElement);
```

### File 2: `<component_name>.html.ts`
The HTML template containing the Lit markup.
1.  Must wrap the template in `html` literal.
2.  Must use slot elements correctly (e.g. `slot="title"`, `slot="body"`, `slot="footer"`).
3.  Do **NOT** add any extra mock elements that are not in the Figma frame.

*Example Boilerplate:*
```typescript
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {MyComponentElement} from './my_component.js';

export function getHtml(this: MyComponentElement) {
  return html`
    <!-- HTML goes here... -->
  `;
}
```

### File 3: `<component_name>.css`

Stylesheet guidelines:

- **Lit Compilation Metadata Headers**: It must contain the Lit compilation metadata headers importing shared variables so that the build system can wrap it into a Lit stylesheet module.
- **Chromium Stylelint Rules**: It must follow the style lint rules for Chromium WebUI ([//src/ui/webui/resources/tools/stylelint.config_base.mjs](//src/ui/webui/resources/tools/stylelint.config_base.mjs)).
- **Lint feedback loop**: Run Chromium's stylelint script (below) and fix errors. Repeat until there are no more errors.
  ```bash
  python3 ui/webui/resources/tools/stylelint.py --config ui/webui/resources/tools/stylelint.config_base.mjs --in_folder <relative_folder_path> --in_files <file_name>.css --out_file /tmp/stylelint.out
  ```
- **Typography & Font Fidelity**: Carry over all Figma font styles into the WebUI stylesheet.
- **Use WebUI Components As-Is**: When using existing WebUI components, **DO NOT** add custom styling to existing components in order to match the Figma specifications. Use the WebUI components as-is.
- **Clean Flex Layouts (No Hardcoded Flex-Basis)**: For elements in Figma
  - With auto-layout `fill container` (filling the remaining space in a flex row or column), use standard CSS flex shorthand (`flex: 1`) instead of computing or hardcoding static pixel `flex-basis` or fixed `width`. This ensures responsive layouts that cleanly adapt to parent padding, gap, and sibling sizes.
  - With fixed width, use CSS `width` property instead of `flex-basis`.
- **Strictly Use Figma-to-CSS Token Map**: For all Figma variables used in the design, map them to existing Chromium CSS variables using the token mapping catalog. If there is a match, you MUST use it.
- **Token Fallback Chaining**: Always declare Material 3 dynamic tokens using Chromium's shared fallback variables (`var(--color-sys-<token>, var(--cr-fallback-color-<token>))`):
  ```css
  /* Preferred: Dynamic Material 3 token with Chromium fallback */
  background-color: var(--color-sys-surface2, var(--cr-fallback-color-surface2));
  color: var(--color-sys-on-surface, var(--cr-fallback-color-on-surface));
  --cr-button-background-color: var(--color-sys-tonal-container, var(--cr-fallback-color-tonal-container));

  /* Anti-pattern: Hardcoded hex fallbacks */
  background-color: var(--color-sys-surface2, #f3f6fc);
  ```

*Example Boilerplate:*
```css
/* #css_wrapper_metadata_start
 * #type=style-lit
 * #import=//resources/cr_elements/cr_shared_vars.css.js
 * #scheme=relative
 * #css_wrapper_metadata_end */

:host {
  background-color: var(--color-sys-surface2, var(--cr-fallback-color-surface2));
  color: var(--color-sys-on-surface, var(--cr-fallback-color-on-surface));
  display: block;
}

/* Styles go here... */
```

---

## 4. Host Page Integration & Theme Support

Whenever creating or integrating a WebUI component into a host HTML page (e.g. an internal diagnostics page, a feature page, or `webui_gallery.html`):

1. **Import Color Pipeline Stylesheet**: Ensure `<link rel="stylesheet" href="chrome://theme/colors.css?sets=ui,chrome">` is included in the host HTML `<head>` or `<body>`.
   - This connects Chromium's `ThemeSource` (`ui::ColorProvider`) to the DOM, defining dynamic Material 3 `--color-sys-*` variables at runtime for both light and dark mode.
2. **Import Shared Typography & Spacing Stylesheets**: Ensure standard shared stylesheets like `chrome://resources/css/text_defaults_md.css` and `chrome://resources/css/md_colors.css` are imported if text and focus defaults are required.

---

## 5. Report

Create an artifact that summarizes the work performed. Explain rationale for the following:
- Any custom CSS added to WebUI components
- Use of CSS variables and token mappings
- Host HTML integration and theme compatibility
