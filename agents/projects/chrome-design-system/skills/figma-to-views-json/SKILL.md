---
name: figma-to-views-json
description: Recreate Figma design frames as prototype-ready Chromium Views JSON script following Chrome's native Views architecture, component guidelines, and color/shape token mappings. Use when asked to create C++ Views Prototype JSON script from a Figma frame URL.
---

# Figma to Views JSON: Code Generation

## Prerequisites

- **Figma MCP Server**: Must be active and configured in the workspace context
  to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root
  directory of the Chromium repository source code (`//src/`).

______________________________________________________________________

## 1. Discovery & Context Retrieval

When given a Figma design URL (e.g.,
`https://www.figma.com/design/:fileKey/:fileName?node-id=:nodeId`):

1. **Extract Parameters**: Extract the `fileKey` and the `nodeId` (replace
   hyphens with colons, e.g., `128-1951` becomes `128:1951`).
2. **Retrieve Design Context**: Call `get_design_context` with `fileKey` and
   `nodeId` to fetch the metadata, layout hierarchy, layer styles, and text
   annotations of the frame.
3. **Reference Existing Component Specs**: Look up component spec files for
   relevant components using the `project-knowledge` skill to map Figma
   components to Chromium C++ Views components.
4. **Reference Token Mapping**: Consult the Chrome Design System token mapping
   using the `project-knowledge` skill to translate the variables from the Figma
   design (e.g., `desktop/sys/base-colors/base`) to equivalent C++ identifiers
   (`ui::kColorSysBase`).

______________________________________________________________________

## 2. Design-to-Code Gap Auditing

Before writing any code, first check if the user already did an audit of this
Figma design against production coding standards within the current chat
session's recent history (past 5 commands). If not, perform the audit.
Incorporate the feedback from this audit into the following code implementation.

______________________________________________________________________

## 3. Views Component Implementation

### Implementation Rules

- **JSON Schema rules**: Read `//ui/views/examples/json_view_builder_schema.ms`
  for the rules about which components/properties are available for use.
- **Surface & Background Colors**: Map Figma variables strictly to their C++
  `ui::ColorId` equivalents as documented in Chrome Design System token
  mappings.
- **Corner Radius Tokens**: Do not hardcode arbitrary radius numbers when design
  system tokens apply. Use CornerRadius as defined in the schema.
- **Responsive Layout & Flex Spacing**: Use `BoxLayout`, `BoxLayoutView`,
  `FlexLayout`, `FlexLayoutView`, `TableLayout` or `TableLayoutView` for
  structured horizontal and vertical stacking, or fully structured tables.
  - Apply consistent spacing and padding using standard `InsetsMetric:` and
    `SetBetweenChildSpacing(...)` and `DistanceMetric:` values for measurements.
  - Use `layout_flex` pseudo-property on components in horizontal or vertical
    layouts to distribute space proportionally.
- **Scope**: Focus strictly on building the UI. Do not add functionality beyond
  what is specified in the Figma mockup. If the Figma mockup includes a window
  frame or top Chrome frame, disregard these.
- **Output Location**: Save the implementation files inside the
  out/<ComponentName> directory (relative to the skill directory), unless
  specified otherwise.
- **Builds**: For testing, ensure the views_examples target is built in the
  user's preferred `out/<out>` directory. Ask the user if multiple are found.
  - **MacOS**: When building under MacOS, the views_examples target builds the
    `Views Examples` app bundle within the `out/<out>` directory. To run from
    the command-line, the executable needs to be referenced within the bundle.
- **Test**: Launch the built views_examples application using the
  `--enable-examples="Views Canvas"` switch and the `--views-canvas-json-file`
  switch with the quoted path to the JSON file generated in the previous step
  above. Be sure to launch the views_examples app onto the same GUI output
  screen as the one the user is logged in with.

### File 1: `<component_name>.json`
