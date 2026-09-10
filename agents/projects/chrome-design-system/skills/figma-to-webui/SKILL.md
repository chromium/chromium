---
name: figma-to-webui
description: Recreate Figma design frames as production-level Chromium WebUI code following Chrome's Lit-element architecture and styling guidelines. Use when asked to create WebUI code from a Figma frame URL.
metadata:
  author: Chrome Design System Team
---

# Figma to WebUI: Code Generation

## Prerequisites

- **Figma MCP Server**: Must be active and configured in the workspace context
  to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root
  directory of the Chromium repository source code (`//src/`).

If prerequisites are not met, STOP execution and inform the user.

______________________________________________________________________

## 1. Discovery & Context Retrieval

When given a Figma design URL (e.g.,
`https://www.figma.com/design/:fileKey/:fileName?node-id=:nodeId`):

1. **Read the Figma design**: Extract information from the Figma design using
   the `figma-context` skill.
2. **Reference Existing Component Specs**: Map Figma components in the Figma
   design to Chromium WebUI components using the `chrome-components` skill.
3. **Reference Token & Font Mapping**: Translate Figma variables to equivalent
   Chromium CSS variables using the `chrome-tokens` skill.

______________________________________________________________________

## 2. Design-to-Code Gap Auditing

Before writing any code, first check if the user already did an audit of this
Figma design against production coding standards within the current chat
session's recent history (past 5 commands). If not, perform the audit.
Incorporate the feedback from this audit into the following code implementation.

______________________________________________________________________

## 3. WebUI Component Implementation

Implement the WebUI code using the `webui-code` skill.

### Guidelines

- **Scope**: Focus strictly on building the UI. Do not add functionality beyond
  what is specified in the Figma mockup. If the Figma mockup includes a window
  frame or top Chrome frame, disregard these.
- **HTML**:
  - Strictly mirror the container and node element hierarchy from Figma. Ensure
    outer and inner containers maintain their exact nesting relationships.
    Exception: if a frame contains only one other frame, these frames can be
    combined; merge their properties.
  - Implement pure UI templates only. Do NOT add interactive event listeners or
    handlers (e.g., `@click`, `@change`, etc.). Bind properties declaratively to
    pre-populated mock values without mutation handlers.
  - Do NOT add any extra mock elements that are not in the Figma frame.
- **CSS**:
  - **Typography & Font Fidelity**: Carry over all Figma font styles into the
    WebUI stylesheet.
  - **Use WebUI Components As-Is**: When using existing WebUI components, **DO
    NOT** add custom styling to existing components in order to match the Figma
    specifications. Use the WebUI components as-is.
  - **Clean Flex Layouts (No Hardcoded Flex-Basis)**: For elements in Figma
    - With auto-layout `fill container` (filling the remaining space in a flex
      row or column), use standard CSS flex shorthand (`flex: 1`) instead of
      computing or hardcoding static pixel `flex-basis` or fixed `width`. This
      ensures responsive layouts that cleanly adapt to parent padding, gap, and
      sibling sizes.
    - With fixed width, use CSS `width` property instead of `flex-basis`.
  - **Strictly Use Figma-to-CSS Token Map**: For all Figma variables used in the
    design, map them to existing Chromium CSS variables using the token mapping
    catalog. If there is a match, you MUST use it.
- **Output Location**: Save the implementation files inside the
  `./out/<ComponentName>` directory, unless specified otherwise.
- **Builds**: Do not run any builds.

______________________________________________________________________

## 4. Host Page Integration & Theme Support

Whenever creating or integrating a WebUI component into a host HTML page (e.g.
an internal diagnostics page, a feature page, or `webui_gallery.html`):

1. **Import Color Pipeline Stylesheet**: Ensure
   `<link rel="stylesheet" href="chrome://theme/colors.css?sets=ui,chrome">` is
   included in the host HTML `<head>` or `<body>`.
   - This connects Chromium's `ThemeSource` (`ui::ColorProvider`) to the DOM,
     defining dynamic Material 3 `--color-sys-*` variables at runtime for both
     light and dark mode.
2. **Import Shared Typography & Spacing Stylesheets**: Ensure standard shared
   stylesheets like `chrome://resources/css/text_defaults_md.css` and
   `chrome://resources/css/md_colors.css` are imported if text and focus
   defaults are required.

______________________________________________________________________

## 5. Report

Create an artifact that summarizes the work performed. Explain rationale for the
following:

- Any custom CSS added to WebUI components
- Use of CSS variables and token mappings
- Host HTML integration and theme compatibility
