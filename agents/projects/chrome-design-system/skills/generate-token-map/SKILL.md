---
name: generate-token-map
description: Maps all Figma design tokens from figma-variables.md to their equivalent C++ (Views) identifiers, CSS (WebUI) variables, and Clank (Android) resource attributes/macros/methods in Chromium, and saves the formatted mapping table to the assets directory of the Chrome Design System project. Use this skill when asked to generate or update the design token mapping table.
metadata:
  author: Chrome Design System Team
---

# Chrome Design System Token Map Generator

This skill maps Chrome Design System (CDS) Figma design variables to their
canonical C++ (Views/UI), CSS (WebUI), and Clank (Android) equivalents in the
Chromium codebase and generates a comprehensive Markdown reference file
(`tokens.md`).

## Prerequisites

- **Chromium Repository**: This skill should be executed from inside the root
  directory of the Chromium repository source code (`//src/`).

______________________________________________________________________

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
[Step 4: Research & map equivalent Clank (Android) macros, attributes, and Java APIs]
           │
           ▼
[Step 5: Write the formatted markdown table to tokens.md]
           │
           ▼
[Step 6: Report summary statistics (total count & unmapped variables)]
```

______________________________________________________________________

## Step-by-Step Procedure

### Step 1: Read and Parse `figma-variables.md`

1. Read
   `agents/projects/chrome-design-system/skills/generate-token-map/assets/figma-variables.md`
   using `view_file`.
2. Extract all Figma design variables (e.g., **143 variables** across **11
   categories**):
   01. `System & Surface Colors`
   02. `Primary, Tertiary & Container Colors`
   03. `State, Ripple & Interaction Colors`
   04. `Outline, Divider & Component Colors`
   05. `Reference & Static Colors`
   06. `Typography (Fonts, Sizes, Weights, Line Heights)`
   07. `Typography (Composite Text Styles)`
   08. `Spacing Tokens`
   09. `Corner Radius Tokens`
   10. `Elevation & Shadows`
   11. `Miscellaneous & Gem Tokens`
3. Identify any variables that cannot be mapped due to empty definitions in
   Figma (e.g., `gem/gradients/angular`, `gem/gradients/tab-strip`).

### Step 2: Map Equivalent C++ Identifiers (Views / UI Layer)

For each Figma variable, identify the matching C++ identifier in Chromium. Note
that C++ names will not be identical to Figma variable names, but will contain
similar words:

- **Color tokens (`desktop/sys/*`, `desktop/ref/*`)**: Search
  `ui/color/color_id.h` and `chrome/browser/ui/color/chrome_color_id.h` for
  `ui::kColorSys*`, `ui::kColorRef*`, `kColor*`, etc.
- **Static & Palette colors**: Search `ui/gfx/color_palette.h` for
  `gfx::kGoogle*`, `SK_ColorWHITE`, `SK_ColorBLACK`, etc.
- **Typography tokens (`desktop/font/*`, `desktop/font_size/*`,
  `desktop/line_height/*`, composite styles)**: Search
  `ui/views/style/typography.h` and `ui/views/style/typography_provider.cc` for
  `views::style::STYLE_*`, `views::style::CONTEXT_*`, `gfx::Font::Weight::*`,
  etc.
- **Spacing tokens (`desktop/spacing/*`) & Corner radius
  (`desktop/corner-radius/*`)**: Search `ui/views/layout/layout_provider.h` and
  `ui/views/layout/layout_provider.cc` for `views::ShapeSysTokens::*`,
  `views::Emphasis::*`, etc.
- **Elevation & Shadows (`desktop/elevation/*`)**: Search
  `ui/gfx/shadow_value.h` and `ui/color/color_id.h` for `ui::kColorShadowValue*`
  and `views::Emphasis::kHigh`.

If no matching identifier can be found, the result should be **(none)**.

### Step 3: Map Equivalent CSS Variables (WebUI Layer)

For each Figma variable, identify the closest matching CSS variable or token in
Chromium WebUI:

- **System colors & state colors**: Search
  `ui/webui/resources/cr_elements/cr_shared_vars.css` and
  `ui/color/color_provider_utils.h` for CSS variables such as `--color-sys-*`,
  `--cr-focus-outline-color`, `--cr-hover-background-color`, etc.
- **Typography & Icon sizes**: Search WebUI shared variables for
  `--cr-primary-font-family`, `--cr-icon-size`, etc.
- **Corner radius & Spacing**: Search `cr_shared_vars.css` for
  `--cr-card-border-radius`, `--cr-button-edge-spacing`, `--cr-section-padding`,
  etc.
- **Elevation & Shadows**: Search `cr_shared_vars.css` for `--cr-elevation-1`,
  `--cr-elevation-2`, `--cr-elevation-3`, etc.

If no matching CSS variable can be found, the result should be **(none)**.

### Step 4: Map Equivalent Clank (Android) Attributes, Macros & Java Methods

For each Figma variable, identify the closest matching resource or resolver in
Clank:

- **Color tokens & surface tokens**: Search
  `components/browser_ui/styles/android/java/res/values/semantic_colors_dynamic.xml`
  for `@macro/*` (e.g. `@macro/default_bg_color`,
  `@macro/default_card_bg_color`), `?attr/color*` (e.g. `?attr/colorSurface`,
  `?attr/colorSecondaryContainer`), and
  `components/browser_ui/styles/android/java/src/org/chromium/components/browser_ui/styles/SemanticColorUtils.java`
  for Java resolvers (`SemanticColorUtils.getDefaultBgColor(context)`,
  `SemanticColorUtils.getColorSurfaceContainer(context)`).
- **Typography & Fonts**: Search `ui/android/java/res/values/dimens.xml` and
  `styles.xml` for `@dimen/text_size_*`, `@dimen/headline_size_*`,
  `@dimen/*_leading`, and `@style/TextAppearance.*`.
- **Spacing & Dimensions**: Search `ui/android/java/res/values/dimens.xml` and
  `components/browser_ui/styles/android/java/res/values/dimens.xml` for
  `@dimen/button_bar_stacked_margin`, `@dimen/modal_dialog_control_*`, etc.
- **Corner Radius**: Search `ui/android/java/res/values/dimens.xml` for
  `@dimen/button_compat_corner_radius`, `@dimen/popup_bg_corner_radius_*`,
  `@dimen/bookmark_bar_chip_corner_radius`,
  `@dimen/default_favicon_corner_radius`.
- **Elevation**: Search `ui/android/java/res/values/dimens.xml` for
  `@dimen/default_elevation_*`.

If no matching Clank token can be found, the result should be **(none)**.

### Step 5: Generate and Write `tokens.md`

1. Structure the output Markdown file with:
   - A clear title:
     `# Chrome Design System Token Mappings (Figma to C++, CSS & Clank Android)`
   - An explanatory introduction.
   - A `> [!WARNING]` callout box at the top explicitly listing any unmappable
     Figma variables (e.g., variables with empty definitions in
     `figma-variables.md`).
   - 11 distinct Markdown sections corresponding to the categories in
     `figma-variables.md`.
   - Standard GitHub-flavored Markdown tables for each category with the
     columns:
     - `Figma Variable Name`
     - `Value / Definition`
     - `C++ Equivalent Identifier`
     - `CSS Equivalent Identifier`
     - `Clank (Android) Equivalent`
2. Save the formatted file to
   `agents/projects/chrome-design-system/assets/tokens/tokens.md` using
   `write_to_file` with `Overwrite: true`.

### Step 6: Summarize Results to the User

After creating `tokens.md`:

- State the **total number of Figma variables** processed (e.g., 143 variables
  across 11 categories).
- Summarize how many variables were successfully mapped to C++ identifiers, CSS
  variables, and Clank Android tokens.
- Highlight any variables that could not be mapped and were called out in the
  warning section.
