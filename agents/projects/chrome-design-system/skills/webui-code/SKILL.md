---
name: webui-code
description: Use this skill when generating or reviewing WebUI code. This skill contains knowledge on WebUI code following Chrome's Lit-element architecture and styling guidelines.
metadata:
  author: Chrome Design System Team
---

# WebUI: Coding Guidelines

## General WebUI Guidelines

- Adhere to the official Chromium WebUI documentation. Read and follow the
  guidelines in these files:
  - For core architecture, Lit implementation details, and migration guides:
    [docs/webui/webui_using_lit.md](//src/docs/webui/webui_using_lit.md)
  - For specific style rules, naming conventions, template guidelines, and
    safety standards:
    [docs/webui/webui_lit_style_guide.md](//src/docs/webui/webui_lit_style_guide.md)
- Adhering to the Chrome Design System. Reference component specs for relevant
  components using the `chrome-components` skill. Ensure code follows the
  guidelines for each component.
- As much as possible, prefer leverage existing WebUI components rather than
  creating new, bespoke components. Components are located under
  `ui/webui/resources/cr_elements/` and `ui/webui/resources/cr_components/`.

## Component Implementation

When implementing a new component, create these files.

### File 1: `<component_name>.ts`

The main TypeScript file defining the element class. It must:

1. Extend `CrLitElement`.
2. Expose the static `is` getter (returning the kebab-case tag name).
3. Load the CSS and HTML template wrappers:
   ```typescript
   import { getCss } from './<component_name>.css.js';
   import { getHtml } from './<component_name>.html.js';
   ```

Example Boilerplate: [component_name.ts](./assets/component_name.ts)

### File 2: `<component_name>.html.ts`

The HTML template containing the Lit markup.

1. Must wrap the template in `html` literal.
2. Must use slot elements correctly (e.g. `slot="title"`, `slot="body"`,
   `slot="footer"`).

Example Boilerplate: [component_name.html.ts](./assets/component_name.html.ts)

### File 3: `<component_name>.css`

Stylesheet guidelines:

- **Lit Compilation Metadata Headers**: It must contain the Lit compilation
  metadata headers importing shared variables so that the build system can wrap
  it into a Lit stylesheet module.
- **Chromium Stylelint Rules**: It must follow the style lint rules for Chromium
  WebUI
  ([//src/ui/webui/resources/tools/stylelint.config_base.mjs](//src/ui/webui/resources/tools/stylelint.config_base.mjs)).
- **Lint feedback loop**: Run Chromium's stylelint script (below) and fix
  errors. Repeat until there are no more errors.
  ```bash
  python3 ui/webui/resources/tools/stylelint.py --config ui/webui/resources/tools/stylelint.config_base.mjs --in_folder <relative_folder_path> --in_files <file_name>.css --out_file /tmp/stylelint.out
  ```
- **Token Fallback Chaining**: Always declare Material 3 dynamic tokens using
  Chromium's shared fallback variables
  (`var(--color-sys-<token>, var(--cr-fallback-color-<token>))`):
  ```css
  /* Preferred: Dynamic Material 3 token with Chromium fallback */
  background-color: var(--color-sys-surface2, var(--cr-fallback-color-surface2));

  /* Anti-pattern: Hardcoded hex fallbacks */
  background-color: var(--color-sys-surface2, #f3f6fc);
  ```

Example Boilerplate: [component_name.css](./assets/component_name.css)
