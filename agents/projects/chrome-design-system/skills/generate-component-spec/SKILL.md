---
name: generate-component-spec
description: Maps a Figma component from the Chrome Design System (CDS) to equivalent C++ Views and WebUI components in the Chromium codebase. Generates a comprehensive side-by-side comparative Markdown specification detailing layout variants, interactive states, design tokens, architectural and implementation gaps, and usage guidelines. Use this skill when given a Figma component URL and asked to generate a component spec.
metadata:
  author: Chrome Design System Team
---

# Component Spec Generator

## Prerequisites
- **Figma MCP Server**: Must be active and configured in the workspace context to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root directory of the Chromium repository source code (`//src/`).

---

## Workflow Decision Tree

```
[User provides Figma URL]
           │
           ▼
[Step 1: Extract URL metadata & Retrieve Figma design context]
           │
           ▼
[Step 2: Search C++ Views for controls under ui/views/controls/]
           │
           ▼
[Step 3: Search WebUI under ui/webui/resources/cr_elements/]
           │
           ▼
[Step 4: Analyze design tokens, colors, shapes, and typography]
           │
           ▼
[Step 5: Identify architectural & state-handling gaps]
           │
           ▼
[Step 6: Generate the final Markdown Spec File]
```

---

## Detailed Step-by-Step Procedure

### Step 1: Extract URL Metadata & Design Context
1. Parse the given Figma URL:
   - Extract the `fileKey` (e.g., `qj3RvxSvSMVdw4tcH8GVMX`) and the `nodeId` from the URL parameters.
   - **Important**: Convert any hyphens in the `nodeId` parameter to colons (e.g., `20268-1070` becomes `20268:1070`).
2. Retrieve the component design context by calling `mcp_figma_get_design_context` with your extracted `fileKey` and `nodeId`.
3. Study the returned React markup, CSS variables, typography annotations, variants, and element states (such as Default, Hovered, Pressed, Disabled) to understand the component's visual properties.
4. If the Figma component name specifies a platform (e.g. Views or WebUI), assume this is a single-platform component, and skip the steps below that pertain to other platforms.

### Step 2: Search the C++ Views Directory

1. Views controls are located under `ui/views/controls/`. Search in this directory (especially under subdirectories like `button/`, `textfield/`, `checkbox/`, `combobox/` depending on the component's visual role) using `glob` or `grep_search`. If there are no matches, note that there are no matches.
2. Locate the corresponding C++ `.h` header files.
3. Identify how style variations (primary, default, tonal, etc.) are set. Search for button or component style definitions (such as `enum class ButtonStyle` in `ui/base/ui_base_types.h`) and typography providers (such as `ui/views/style/typography_provider.cc`).

### Step 3: Search the WebUI Directory

1. WebUI elements are located under `ui/webui/resources/cr_elements/` and `ui/webui/resources/cr_components/`. Search in this directory using `glob` or `grep_search`. If there are no matches, note that there are no matches.
2. Identify the custom element's `.ts` controller file (e.g., `cr_button.ts`) and any associated HTML templates (`.html.ts`) or CSS files (`.css`).
3. Identify how style variations are activated via CSS classes (such as `.action-button` or `.tonal-button` for `<cr-button>`).

### Step 4: Map Design Tokens
1. Compare color definitions:
   - Match Figma CSS properties (e.g., `--desktop/sys/primary-colors/primary`) to corresponding C++ `ColorId` entries (e.g., `ui::kColorButtonBackgroundProminent` in `ui/color/color_id.h` or `material_ui_color_mixer.cc`) and WebUI custom properties (e.g., `--color-button-background-prominent`).
2. Compare typography:
   - Inspect font weights, font sizes, and line heights. Note any delta calculations or hardcoded styles.
3. Compare margins, padding, spacing, and shapes (corner radius). Note whether pill-shaped (rounded) elements are hardcoded (e.g., `border-radius: 100px`) or resolved dynamically.

### Step 5: Identify Architectural Gaps
Document differences between design intent and system implementations, focusing on:
- **Variant mismatches**: e.g., features modeled in Figma that have no direct single-class toggle in C++ Views or require manual CSS in WebUI.
- **Layout restrictions**: e.g., content slots, nested elements, or trailing arrow support.
- **Interactive state discrepancies**: e.g., missing design specs for active, hovered, or focused states, or custom accessibility overrides (like `FocusRing`).

### Step 6: Generate the Specification Markdown File
Write a polished, comprehensive Markdown specification file. Strictly follow rules:

- Name the file `{{ComponentName}}_Spec.md`. Use the component name from Figma. If the component name includes the platform, separate it out like this: `{{ComponentName}}_{{Platform}}` (e.g. `TextFields_WebUI`).
- If the file already exists, update it only with necessary changes rather than generating from scratch.
- The document must follow the exact structure specified in `assets/spec-template.md`.
  - Links to components in code MUST be relative to the repo root (`//src/`), NOT the user's filesystem (do not include `file://`).
  - When creating tables, only include information for platforms that apply (don't create any N/A columns).
- Make the file viewable as an artifact.
- Ask the user to confirm saving the markdown file to the `agents/projects/chrome-design-system/assets/component-specs/` directory.
