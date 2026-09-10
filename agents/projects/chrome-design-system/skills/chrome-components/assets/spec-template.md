# Component Spec: {{ComponentName}}

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **{{ComponentName}}** component across
Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

\[Summarize the component's role, interactive capabilities, and styling variants
across desktop and mobile platforms\]

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                          | C++ Views (Desktop)                                                | WebUI (Desktop)                                                              | Clank (Android)                                                                                                                      |
| :----------------- | :------------------------------------------------------- | :----------------------------------------------------------------- | :--------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `{{ComponentName}}`                                      | `views::{{ViewsClassName}}`                                        | `<{{webui-element-tag}}>`                                                    | `{{ClankClassName}}` / `@style/{{ClankStyleName}}`                                                                                   |
| **Source Files**   | [Figma Link: `{{nodeId}}`](%7B%7BOriginalFigmaURL%7D%7D) | [path/to/views/header.h](%7B%7B//src/path/to/views/header.h%7D%7D) | [path/to/webui/controller.ts](%7B%7B//src/path/to/webui/controller.ts%7D%7D) | [path/to/clank/Class.java](%7B%7B//src/path/to/clank/Class.java%7D%7D)<br>[path/to/styles.xml](%7B%7B//src/path/to/styles.xml%7D%7D) |

\[In the Source Files row, include links to all components defined in the
Component Name row. For Views, only include header `.h` files. For WebUI, only
include controller `.ts` files. For Clank, include Java class files and resource
`.xml` files.\]

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

\[Table mapping Figma variants like Primary, Tonal, Outlined, Text, and icon
slots (Leading, Trailing) to C++ classes/enums, WebUI custom classes or
elements, and Clank Android styles/classes\]

| Feature / Variant | Figma Component   | C++ Views (Desktop)           | WebUI (Desktop)                     | Clank (Android)       |
| :---------------- | :---------------- | :---------------------------- | :---------------------------------- | :-------------------- |
| **Primary Style** | `Variant=Primary` | `ui::ButtonStyle::kProminent` | `<cr-button class="action-button">` | `@style/FilledButton` |
| ...               | ...               | ...                           | ...                                 | ...                   |

______________________________________________________________________

## 3. Component States

\[Table mapping interactive states like Default, Hovered, Pressed, Disabled,
Focused to Figma state properties, C++ ButtonStates / views::FocusRing, WebUI
CSS classes / pseudo-classes, and Android state selectors / ripple drawables\]

| State                | Figma Component  | C++ Views (Desktop)                   | WebUI (Desktop)              | Clank (Android)                                             |
| :------------------- | :--------------- | :------------------------------------ | :--------------------------- | :---------------------------------------------------------- |
| **Default (Normal)** | `State=Default`  | `Button::ButtonState::STATE_NORMAL`   | Default state / no modifiers | `android:enabled="true"`                                    |
| **Hovered**          | `State=Hovered`  | `Button::ButtonState::STATE_HOVERED`  | `:hover` pseudo-class        | Hover state layer (`@dimen/default_hovered_alpha`)          |
| **Pressed (Pushed)** | `State=Pressed`  | `Button::ButtonState::STATE_PRESSED`  | `:active` / `<cr-ripple>`    | Ripple state (`@color/filled_button_ripple_color`)          |
| **Disabled**         | `State=Disabled` | `Button::ButtonState::STATE_DISABLED` | `<... disabled>` attribute   | `android:enabled="false"` (`@dimen/default_disabled_alpha`) |
| **Focused**          | `State=Focused`  | `views::FocusRing`                    | `:focus-visible` outline     | `FocusRing` / `?attr/colorPrimary`                          |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

\[Comprehensive side-by-side mapping table of design tokens covering colors,
container/foreground pairings, border outlines, typography (weights, sizes, line
heights), outer/inner paddings, and corner radii\]

| Design Attribute                 | Figma Design Token                     | C++ Views (Desktop)                   | WebUI (Desktop)                       | Clank (Android)                                              |
| :------------------------------- | :------------------------------------- | :------------------------------------ | :------------------------------------ | :----------------------------------------------------------- |
| **Primary Container Background** | `--desktop/sys/primary-colors/primary` | `ui::kColorButtonBackgroundProminent` | `--color-button-background-prominent` | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| ...                              | ...                                    | ...                                   | ...                                   | ...                                                          |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

\[Detailed sections describing shape/corner-radius discrepancies, static vs.
dynamic theme coloring, font size inheritance differences, line-height
overrides, and responsive or asymmetric spacing shifts across desktop and
Android\]

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

\[Detailed analysis of features or visual treatments that have layout
mismatches, variant discrepancies, or custom behavioral configurations between
Figma and codebases (such as double-icon limitations, focus ring logic, or touch
target sizing)\]

______________________________________________________________________

## 7. Usage & UX Guidance

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
