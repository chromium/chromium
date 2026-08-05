---
name: generate-token-map
description: Maps all Figma design tokens from figma-variables.md to their equivalent C++ (Views) identifiers and CSS (WebUI) variables in Chromium, and saves the formatted mapping table to the assets directory of the Chrome Design System project. Use this skill when asked to generate or update the design token mapping table.
metadata:
  author: Chrome Design System Team
---

# Chrome Design System Token Map Generator

This skill maps Chrome Design System (CDS) Figma design variables to their canonical C++ (Views/UI) and CSS (WebUI) equivalents in the Chromium codebase and generates a comprehensive Markdown reference file (`tokens.md`).

## Prerequisites
- **Chromium Repository**: This skill should be executed from inside the root directory of the Chromium repository source code (`//src/`).

---

## Workflow Decision Tree

```
[Read figma-variables.md]
           │
           ▼
[Step 1: Extract all Figma design variables across all categories]
           │
           ▼
[Step 2: Research & map equivalent C++ (Views) identifiers]
           │
           ▼
[Step 3: Research & map equivalent CSS (WebUI) variables]
           │
           ▼
[Step 4: Write the formatted markdown table to tokens.md]
           │
           ▼
[Step 5: Report summary statistics (total count & unmapped variables)]
```

---

## Step-by-Step Procedure

### Step 1: Read and Parse `figma-variables.md`
1. Read `agents/projects/chrome-design-system/skills/generate-token-map/assets/figma-variables.md` using `view_file`.
2. Extract all Figma design variables (e.g., **143 variables** across **11 categories**):
   1. `System & Surface Colors`
   2. `Primary, Tertiary & Container Colors`
   3. `State, Ripple & Interaction Colors`
   4. `Outline, Divider & Component Colors`
   5. `Reference & Static Colors`
   6. `Typography (Fonts, Sizes, Weights, Line Heights)`
   7. `Typography (Composite Text Styles)`
   8. `Spacing Tokens`
   9. `Corner Radius Tokens`
   10. `Elevation & Shadows`
   11. `Miscellaneous & Gem Tokens`
3. Identify any variables that cannot be mapped due to empty definitions in Figma (e.g., `gem/gradients/angular`, `gem/gradients/tab-strip`).

### Step 2: Map Equivalent C++ Identifiers (Views / UI Layer)
For each Figma variable, identify the matching C++ identifier in Chromium. Note that C++ names will not be identical to Figma variable names, but will contain similar words:
- **Color tokens (`desktop/sys/*`, `desktop/ref/*`)**: Search `ui/color/color_id.h` and `chrome/browser/ui/color/chrome_color_id.h` for `ui::kColorSys*`, `ui::kColorRef*`, `kColor*`, etc.
- **Static & Palette colors**: Search `ui/gfx/color_palette.h` for `gfx::kGoogle*`, `SK_ColorWHITE`, `SK_ColorBLACK`, etc.
- **Typography tokens (`desktop/font/*`, `desktop/font_size/*`, `desktop/line_height/*`, composite styles)**: Search `ui/views/style/typography.h` and `ui/views/style/typography_provider.cc` for `views::style::STYLE_*`, `views::style::CONTEXT_*`, `gfx::Font::Weight::*`, etc.
- **Spacing tokens (`desktop/spacing/*`) & Corner radius (`desktop/corner-radius/*`)**: Search `ui/views/layout/layout_provider.h` and `ui/views/layout/layout_provider.cc` for `views::ShapeSysTokens::*`, `views::Emphasis::*`, etc.
- **Elevation & Shadows (`desktop/elevation/*`)**: Search `ui/gfx/shadow_value.h` and `ui/color/color_id.h` for `ui::kColorShadowValue*` and `views::Emphasis::kHigh`.

If no matching identifier can be found, the result should be **(none)**.

### Step 3: Map Equivalent CSS Variables (WebUI Layer)
For each Figma variable, identify the closest matching CSS variable or token in Chromium WebUI:
- **System colors & state colors**: Search `ui/webui/resources/cr_elements/cr_shared_vars.css` and `ui/color/color_provider_utils.h` for CSS variables such as `--color-sys-*`, `--cr-focus-outline-color`, `--cr-hover-background-color`, etc.
- **Typography & Icon sizes**: Search WebUI shared variables for `--cr-primary-font-family`, `--cr-icon-size`, etc.
- **Corner radius & Spacing**: Search `cr_shared_vars.css` for `--cr-card-border-radius`, `--cr-button-edge-spacing`, `--cr-section-padding`, etc.
- **Elevation & Shadows**: Search `cr_shared_vars.css` for `--cr-elevation-1`, `--cr-elevation-2`, `--cr-elevation-3`, etc.

If no matching CSS variable can be found, the result should be **(none)**.

### Step 4: Generate and Write `tokens.md`
1. Structure the output Markdown file with:
   - A clear title: `# Chrome Design System Token Mappings (Figma to C++ & CSS)`
   - An explanatory introduction.
   - A `> [!WARNING]` callout box at the top explicitly listing any unmappable Figma variables (e.g., variables with empty definitions in `figma-variables.md`).
   - 11 distinct Markdown sections corresponding to the categories in `figma-variables.md`.
   - Standard GitHub-flavored Markdown tables for each category with the columns:
     - `Figma Variable Name`
     - `Value / Definition`
     - `C++ Equivalent Identifier`
     - `CSS Equivalent Variable`
2. Save the formatted file to `agents/projects/chrome-design-system/assets/tokens.md` using `write_to_file` with `Overwrite: true`.

### Step 5: Summarize Results to the User
After creating `tokens.md`:
- State the **total number of Figma variables** processed (e.g., 143 variables across 11 categories).
- Summarize how many variables were successfully mapped to C++ identifiers and/or CSS variables.
- Highlight any variables that could not be mapped and were called out in the warning section.
