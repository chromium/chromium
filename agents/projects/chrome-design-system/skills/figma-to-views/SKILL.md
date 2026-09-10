---
name: figma-to-views
description: Recreate Figma design frames as production-ready Chromium C++ Views code following Chrome's native Views architecture, component guidelines, and color/shape token mappings. Use when asked to create C++ Views code from a Figma frame URL.
---

# Figma to Views: Code Generation

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
   design to Chromium C++ Views components using the `chrome-components` skill.
3. **Reference Token & Font Mapping**: Translate Figma variables to equivalent
   Chromium C++ identifiers using the `chrome-tokens` skill.

______________________________________________________________________

## 2. Design-to-Code Gap Auditing

Before writing any code, first check if the user already did an audit of this
Figma design against production coding standards within the current chat
session's recent history (past 5 commands). If not, perform the audit.
Incorporate the feedback from this audit into the following code implementation.

______________________________________________________________________

## 3. Views Component Implementation

Implement the Views UI code using the `views-code` skill.

### Guidelines

- **Surface & Background Colors**: Map Figma variables strictly to their C++
  `ui::ColorId` equivalents as documented in Chrome Design System token
  mappings.
- **Scope**: Focus strictly on building the UI. Do not add functionality beyond
  what is specified in the Figma mockup. If the Figma mockup includes a window
  frame or top Chrome frame, disregard these.
- **Output Location**: Save the implementation files inside the
  `./out/<ComponentName>` directory (relative to the skill directory), unless
  specified otherwise.
- **Builds**: Do not run any builds.
