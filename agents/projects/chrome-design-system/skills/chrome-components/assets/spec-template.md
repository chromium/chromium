# {{ComponentName}} Spec ({{PlatformName}})

This document maps the specifications of the **{{ComponentName}}**
({{PlatformName}}) component across Figma and code.

______________________________________________________________________

## Overview

\[In less than 3 sentences, summarize the component's role, interactive
capabilities, and styling variants\]

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                 | C++ Views (Desktop)                                                | WebUI (Desktop)                                                              | Clank (Android)                                                                                                                      |
| :----------------- | :-------------------------------------------------------------- | :----------------------------------------------------------------- | :--------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `{{ComponentName}}`                                             | `views::{{ViewsClassName}}`                                        | `<{{webui-element-tag}}>`                                                    | `{{ClankClassName}}` / `@style/{{ClankStyleName}}`                                                                                   |
| **Source Files**   | [Figma Link: `{{ComponentName}}`](%7B%7BOriginalFigmaURL%7D%7D) | [path/to/views/header.h](%7B%7B//src/path/to/views/header.h%7D%7D) | [path/to/webui/controller.ts](%7B%7B//src/path/to/webui/controller.ts%7D%7D) | [path/to/clank/Class.java](%7B%7B//src/path/to/clank/Class.java%7D%7D)<br>[path/to/styles.xml](%7B%7B//src/path/to/styles.xml%7D%7D) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: \[List each Figma component's variant properties and their
values, e.g. `Variant` (Primary | Tonal | Outlined | Text) × `Icons` (None |
Leading | Trailing) × `State` (Default | Hovered | Pressed | Disabled). If there
are multiple Figma components, use one bullet per component.\]

\[Table mapping Figma variants like State (e.g. Hovered, Disabled), Type (e.g.
Primary, Tonal, Outlined), and icon slots (Leading, Trailing) to equivalent C++
classes/enums, WebUI props/custom classes, and Clank Android styles/classes\]

| Feature / Variant | Figma Component   | C++ Views (Desktop)           | WebUI (Desktop)                     | Clank (Android)       |
| :---------------- | :---------------- | :---------------------------- | :---------------------------------- | :-------------------- |
| **Primary Style** | `Variant=Primary` | `ui::ButtonStyle::kProminent` | `<cr-button class="action-button">` | `@style/FilledButton` |
| ...               | ...               | ...                           | ...                                 | ...                   |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

\[Comprehensive side-by-side mapping table of design tokens covering colors,
container/foreground pairings, border outlines, typography (weights, sizes, line
heights), outer/inner paddings, and corner radii\]

| Design Attribute                 | Figma Design Token                     | C++ Views (Desktop)                   | WebUI (Desktop)                       | Clank (Android)                                              |
| :------------------------------- | :------------------------------------- | :------------------------------------ | :------------------------------------ | :----------------------------------------------------------- |
| **Primary Container Background** | `--desktop/sys/primary-colors/primary` | `ui::kColorButtonBackgroundProminent` | `--color-button-background-prominent` | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| ...                              | ...                                    | ...                                   | ...                                   | ...                                                          |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

\[Detailed sections describing shape/corner-radius discrepancies, static vs.
dynamic theme coloring, font size inheritance differences, line-height
overrides, and responsive or asymmetric spacing shifts across desktop and
Android\]

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

\[Detailed analysis of features or visual treatments that have layout
mismatches, variant discrepancies, or custom behavioral configurations between
Figma and codebases (such as double-icon limitations, focus ring logic, or touch
target sizing)\]

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- [Visual hierarchy and variant selection advice]
- [Action pairing layout rules (primary vs secondary alignment)]
- [Best labeling conventions (strong active verbs)]

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- [WebUI a11y rules: role, tabindex, triggers, and aria attribute bindings]
- [C++ Views native focus, ink drop, and naming/shortcut standards]
- \[Clank Android accessibility: `contentDescription`, min 48dp touch target
  (`@dimen/min_touch_target_size`), and TalkBack navigation\]

### 3. Icon Usage Guidelines

[Guidelines for icon usage, if applicable]
