---
name: project-knowledge
description: Use this skill whenever you need to look up or reference Chrome Design System component specs or design token mappings.
---

# Chrome Design System Project Knowledge

## Read reference files

Start from the `agents/projects/chrome-design-system/` directory and use glob to
find relevant files.

- For token mappings (C++ Views, WebUI, and Clank Android), read
  `assets/tokens/tokens.md`.
- For high-level cross-platform mapping across all platforms (Figma, Views,
  WebUI, Clank), read `assets/component-mapping.md`.
- For detailed component specifications, read `assets/component-specs/*`.
  - Use `list_directory` tool on this path to find the relevant `_Spec.md`
    files.
