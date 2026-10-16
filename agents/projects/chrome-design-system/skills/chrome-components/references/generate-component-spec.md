---
name: generate-component-spec
description: Maps a Figma component from the Chrome Design System (CDS) to its platform-specific equivalent in the Chromium codebase. Generates a platform-specific Markdown specification that details variants, interactive states, design tokens, architectural and implementation gaps, and usage guidelines. Use this skill when given a Figma component or frame URL and asked to generate a component spec.
metadata:
  author: Chrome Design System Team
---

# Component Spec Generator

## Prerequisites

- **Figma MCP Server**: Must be active and configured in the workspace context
  to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root
  directory of the Chromium repository source code (`//src/`).

______________________________________________________________________

## Workflow Decision Tree

```
[User provides URL to a Figma component or frame]
           │
           ▼
[Step 1: Extract URL metadata, resolve target Figma component(s), & retrieve design context]
           │
           ▼
[Step 2: Identify the component's platform]
           │
           ▼
[Step 3: Identify equivalent component(s) in code & related Figma components]
           │
           ▼
[Step 4: Analyze design tokens]
           │
           ▼
[Step 5: Identify architectural gaps]
           │
           ▼
[Step 6: Generate the final Markdown Spec File]
```

______________________________________________________________________

## Detailed Step-by-Step Procedure

### Step 1: Extract URL Metadata & Design Context

1. Parse the given Figma URL:
   - Extract the `fileKey` (e.g., `qj3RvxSvSMVdw4tcH8GVMX`) and the `nodeId`
     from the URL parameters.
   - **Important**: Convert any hyphens in the `nodeId` parameter to colons
     (e.g., `20268-1070` becomes `20268:1070`).
2. Resolve the target Figma component(s):
   - Sometimes the provided URL points to a parent frame (such as a section,
     documentation card, or artboard) that isn't actually a Figma component, but
     contains the Figma component(s).
   - Inspect the node structure (via `mcp_figma_get_metadata` or
     `mcp_figma_get_design_context`). **Always use the Figma component
     (`COMPONENT` or `COMPONENT_SET`) rather than a parent frame** whenever
     possible, updating the target `nodeId`(s) and Figma URL(s) to point
     directly to the Figma component(s).
   - **Note on frame-based components**: This isn't always possible—sometimes a
     component in Figma isn't formally defined as a Figma component, but is
     instead constructed out of plain frames (`FRAME`). In that case, use the
     frame that directly represents the component itself (rather than an outer
     layout or documentation wrapper frame).
   - **Multiple Figma components**: If the parent frame contains multiple Figma
     components that are part of the same logical component (or sub-components
     that together make up the component), collect all of them so they can all
     be analyzed and listed in the spec.
3. Retrieve the component design context by calling
   `mcp_figma_get_design_context` with your `fileKey` and resolved `nodeId`(s).
4. Study the returned React markup, CSS variables, typography annotations,
   variants, and element states (such as Default, Hovered, Pressed, Disabled) to
   understand the component's visual properties.

### Step 2: Identify the Component's Platform

The given Figma URL is a component for a specific platform. It should be one of
the following, which also corresponds to an implementation in code:

- Desktop - note that a desktop Figma component may correspond to a WebUI
  implementation, a C++ Views implementation, or both.
- Android (Clank)

### Step 3: Identify Equivalent Component(s) in Code

Look for code implementations depending on the platform. If there are no
matches, tell the user that there are no matches, and do not continue (don't
generate a component spec file).

Keep in mind that **Figma-to-code mappings can be many-to-many**:

- **One Figma component → Multiple code components**: A single Figma component
  may map to several distinct components in code (for example, variants within
  one Figma component implemented as separate C++ Views classes or WebUI
  elements). Identify all corresponding primary code components.
- **Multiple Figma components → One code component (or shared component)**:
  Conversely, several Figma components may map to the same component in code or
  represent different parts/variants of the same component. When this occurs,
  identify and list all of the other Figma components that are part of the same
  component (fetching their metadata/design context if needed).

#### Desktop C++ Views

1. Views controls are located under `ui/views/controls/`. Search in this
   directory (especially under subdirectories like `button/`, `textfield/`,
   `checkbox/`, `combobox/` depending on the component's visual role) using
   `glob` or `grep_search`.
2. Locate the corresponding C++ `.h` header files.
3. Identify how style variations (primary, default, tonal, etc.) are set. Search
   for button or component style definitions (such as `enum class ButtonStyle`
   in `ui/base/ui_base_types.h`) and typography providers (such as
   `ui/views/style/typography_provider.cc`).

#### Desktop WebUI

1. WebUI elements are located under `ui/webui/resources/cr_elements/` and
   `ui/webui/resources/cr_components/`. Search in this directory using `glob` or
   `grep_search`.
2. Identify the custom element's `.ts` controller file (e.g., `cr_button.ts`)
   and any associated HTML templates (`.html.ts`) or CSS files (`.css`).
3. Identify how style variations are activated via CSS classes (such as
   `.action-button` or `.tonal-button` for `<cr-button>`).

#### Android (Clank)

1. Clank Android components are located under:
   - `ui/android/java/src/org/chromium/ui/widget/` (e.g., `ButtonCompat.java`,
     `ChromeImageButton.java`, `AnchoredPopupWindow.java`,
     `EditTextWithLeading.java`).
   - `components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/`
     (e.g., `chips/ChipView.java`, `CheckBoxWithDescription.java`,
     `RadioButtonWithDescription.java`, `MaterialSwitchWithText.java`,
     `textbubble/TextBubble.java`, `listmenu/ListMenu.java`).
   - `components/browser_ui/bottomsheet/android/` (e.g.,
     `BottomSheetController.java`).
   - `components/browser_ui/modaldialog/android/` (e.g.,
     `ModalDialogView.java`).
2. Identify the corresponding Java class files and XML resource styles in
   `components/browser_ui/styles/android/java/res/values/styles.xml` and
   `ui/android/java/res/values/styles.xml` (e.g., `@style/FilledButton`,
   `@style/Widget.BrowserUI.CheckBox`).
3. Identify MVC bindings if applicable (`PropertyModel`, `ViewBinder`).

### Step 4: Map Design Tokens

1. Compare color definitions:
   - Match Figma CSS properties (e.g., `--desktop/sys/primary-colors/primary`)
     to corresponding:
     - C++ `ColorId` entries (e.g., `ui::kColorButtonBackgroundProminent` in
       `ui/color/color_id.h`).
     - WebUI custom properties (e.g., `--color-button-background-prominent`).
     - Clank (Android) resource macros/attributes (e.g.,
       `@macro/default_control_color_active`, `?attr/colorPrimary`, or
       `SemanticColorUtils.getFilledButtonBgColor(context)`).
2. Compare typography:
   - Inspect font weights, font sizes, and line heights. Note any delta
     calculations or hardcoded styles (`@dimen/text_size_*`,
     `@dimen/*_leading`).
3. Compare margins, padding, spacing, and shapes (corner radius). Note whether
   pill-shaped (rounded) elements are hardcoded (e.g., `border-radius: 100px`,
   `@dimen/button_compat_corner_radius`) or resolved dynamically.

### Step 5: Identify Architectural Gaps

Document differences between design intent and system implementations, focusing
on:

- **Variant mismatches**: e.g., features modeled in Figma that have no direct
  single-class toggle in C++ Views, require manual CSS in WebUI, or need custom
  layout overlays in Clank.
- **Layout restrictions**: e.g., content slots, nested elements, or trailing
  arrow support.
- **Interactive state discrepancies**: e.g., missing design specs for active,
  hovered, or focused states, or custom accessibility overrides (like
  `FocusRing` vs Android ripple backgrounds).

### Step 6: Generate the Specification Markdown File

Write a polished, comprehensive Markdown specification file. Strictly follow
rules:

- Name the file `{{ComponentName}}_Spec.md`.
- If the file already exists, update it only with necessary changes rather than
  generating from scratch.
- The document must follow the exact structure specified in
  `../assets/spec-template.md`.
  - Omit table columns that are not relevant to the component's platform.
  - Links to components in code MUST be relative to the repo root (`//src/`),
    NOT the user's filesystem (do not include `file://`).
  - In section 1, under the **Figma Component** column:
    - Always reference the actual Figma component name and link (`nodeId`)
      rather than a parent container frame (unless the component is only defined
      using frames in Figma).
    - If multiple Figma components map to the component in code or are part of
      the same component, list all of those Figma components (and their
      corresponding Figma links) separated by `<br>`.
  - In section 1, under the platform code columns:
    - If the Figma component maps to multiple components in code, list each
      applicable component and its source file separated by `<br>`.
    - When listing source files, strictly limit components in code to the
      primary component class(es)/element(s) and their direct source files
      (e.g., the `.h` header for Views, `.ts` controller for WebUI, and primary
      `.java` widget class for Clank).
    - **Always list only the component name(s)** (e.g., `ButtonCompat`,
      `views::MdTextButton`, `<cr-button>`). Do NOT include style resources,
      theme overlays, or CSS classes (e.g., `@style/FilledButton`,
      `FilledButton.Tonal.ThemeOverlay.*`, `.action-button`) in these columns;
      document those in the variant and token sections instead.
- Make the file viewable as an artifact.
- In section 2, always start with a **"Figma variant axes"** summary listing
  every variant property of each Figma component and its possible values (e.g.,
  `Variant` (Primary | Tonal) × `State` (Default | Hovered)), before the
  variants table. Use exact Figma property/value names, and one bullet per Figma
  component when there are several.
- Ask the user to confirm saving the markdown file to the
  `./component-specs/{{PlatformName}}/` directory and updating the mapping table
  in `./components_{{platformName}}.md`.
  - When adding the row to the mapping table, follow the same rule: list only
    the component name(s) in the platform code columns, without style resources,
    theme overlays, or CSS classes.
