---
name: figma-design-review
description: Critique and review Figma designs to ensure alignment with Chromium codebase components, tokens, and structure. Use when the user asks to critique, review, or audit a design, check alignment with code, or compare a Figma node against component specifications or coding standards.
metadata:
  author: Chrome Design System Team
---

# Figma Design Review

This skill provides a structured workflow for reviewing a Figma design (node or page) and critiquing its alignment with the existing codebase component specifications. It helps bridge the gap between design and implementation by identifying hardcoded values, raw icon usages instead of proper components, and structural mismatches.

## Prerequisites
- **Figma MCP Server**: Must be active and configured in the workspace context to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root directory of the Chromium repository source code (`//src/`).

## Review Workflow

When asked to review a Figma design for code alignment, follow these steps sequentially:

### 1. Fetch the Design Context
Use the `get_design_context` tool on the provided Figma URL or node ID.
- Pay close attention to the generated React/Tailwind code output as it represents the node's structure.
- Look for generic `<div>` wrappers versus named components.
- Check for hardcoded `className` values versus design tokens (e.g., `var(--desktop/sys/surface)`).
- Note any raw image/mask constructions.

### 2. Identify and Read Component Specifications
Identify the main UI components used in the design (e.g., Buttons, Dialogs, Radio Buttons, Text Fields).
- Look up Chorme Design System component specs using the `project-knowledge` skill.
- Read the identified `_Spec.md` files using `read_file` to understand how the components are implemented in the target codebase (WebUI and C++ Views).
- **Crucial Component Mapping Rule**:
  - The design context from Figma may output complex nested `div`s, SVGs, or image masks (`imgMask`, `imgIcon`) instead of clean component tags. This happens when Figma Code Connect is missing, causing the API to decompile the visual layers of a component.
  - **Do not** assume these are "raw custom-drawn shapes" if the component instance name, data name, or ID in the design context (e.g. `data-name="RadioButtons"`, `data-name=".base.Buttons"`) matches an official component defined in the spec files.
  - Cross-reference the Figma component name/ID with Section 1 of the component's `_Spec.md` file to identify the true target components (e.g., `<cr-radio-button>` for WebUI, `views::RadioButton` for C++).
  - Don't report this in the feedback, and don't advise on the use of Figma Code Connect.
- **Crucial Information to Extract from Specs:**
  - Standard tokens used for colors, spacing, outlines, and fonts.
  - Handling of interactive states (hover, focus, disabled).
  - Use of specific variant classes (e.g., `.action-button`, `.tonal-button`).
  - Architectural constraints (e.g., C++ handles shadows natively).

### 3. Analyze and Compare
Cross-reference the Figma structure (from Step 1) with the code specifications (from Step 2). Look for:
- **Raw Elements vs. System Components**
  - Audit: Is the design using raw, custom-drawn frames to define a UI element when an official component should be used instead? (Ensure you distinguish between actual custom-drawn shapes and a decompiled library component due to missing Code Connect, as detailed in Step 2).
  - Resolution: Map custom-drawn elements to the correct system components. For decompiled library components, advise to ignore the visual HTML structure and implement the official code component mapped in the spec.
- **Token Usage**
  - Audit: Are static hex codes, spacing, and radius values being used instead of defined tokens? Are tokens aligning with what the code specs dictate?
  - Resolution: Call out any uses of static values. Recommend Figma variables that could be used instead that semantically map to variables in code. Reference the Chrome Design System token mapping.
- **Manual Overrides**
  - Audit: Is the design applying custom styling when the codebase handles that at the component level?
  - Resolution: Advise to avoid custom styling if it's already handled at the component level.
- **Variant Usage**
  - Audit: Are the correct variants being mapped accurately according to their design intent and usage guidelines?
  - Resolution: Advise how variants map to what's in code.
- **Structure Alignment**
  - Audit: Is the Figma frame adding content inside components that can map to slots in the code implementation?
  - Resolution: Map these to standard slots supported by the parent component. For example, for `<cr-dialog>`, slot custom footers into `slot="footer"`.

### 4. Provide Critique and Advice

Structure your final output clearly for the user. For each identified issue, provide:
1.  **Critique:** Describe what is currently happening in the Figma design and why it is inconsistent or problematic.
2.  **Advice for Alignment:** Provide actionable advice separated by:
    -   **Figma:** What the designer should change in the Figma file.
    -   **Code Impact:** How this aligns (or misaligns) with the target frameworks (e.g., WebUI / `<cr-button>` or C++ Views / `views::RadioButton`).

**Important Output Note:**
Always prioritize actionable advice that brings the Figma structure closer to the code structure. Explicitly state when a Figma component should be swapped, or when a manual style overrides should be ignored by engineers.

### 5. Save the output

Save the output in a directory `out/<ComponentName>_Audit.md` in the same directory as this skill.
