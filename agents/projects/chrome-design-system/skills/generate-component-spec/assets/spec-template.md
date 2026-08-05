# Component Spec: {{ComponentName}}

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **{{ComponentName}}** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

[Summarize the component's role, interactive capabilities, and styling variants]

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `{{ComponentName}}` | `views::{{ViewsClassName}}` | `<{{webui-element-tag}}>` |
| **Source Files** | [Figma Link: `{{nodeId}}`]({{OriginalFigmaURL}}) | [path/to/views/header.h]({{//src/path/to/views/header.h}}) | [path/to/webui/controller.ts]({{//src/path/to/webui/controller.ts}}) |

[In the Source Files row, include links to all components defined in the Component Name row. For Views, only include header `.h` files. For WebUI, only include controller `.ts` files (disregard HTML or CSS).]

---

## 2. Styling, Variants & Features (Layout & Style)

[Table mapping Figma variants like Primary, Tonal, Outlined, Text, and icon slots (Leading, Trailing) to C++ classes/enums and WebUI custom classes or elements]

---

## 3. Component States

[Table mapping interactive states like Default, Hovered, Pressed, Disabled, Focused to Figma state properties, C++ ButtonStates / views::FocusRing, and WebUI CSS classes / pseudo-classes]

---

## 4. Design Token Comparison (Side-by-Side)

[Comprehensive side-by-side mapping table of design tokens covering colors, container/foreground pairings, border outlines, typography (weights, sizes, line heights), outer/inner paddings, and corner radii]

---

## 5. Architectural & Implementation Gaps

[Detailed sections describing shape/corner-radius discrepancies, static vs. dynamic theme coloring, font size inheritance differences, line-height overrides, and responsive or asymmetric spacing shifts]

---

## 6. Styling, Variants, Features and States Mismatches

[Detailed analysis of features or visual treatments that have layout mismatches, variant discrepancies, or custom behavioral configurations between Figma and codebases (such as double-icon limitations or focus ring logic)]

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- [Visual hierarchy and variant selection advice]
- [Action pairing layout rules (primary vs secondary alignment)]
- [Best labeling conventions (strong active verbs)]

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- [WebUI a11y rules: role, tabindex, triggers, and aria attribute bindings]
- [C++ Views native focus, ink drop, and naming/shortcut standards]

### 3. Icon Usage Guidelines
[Guidelines for icon usage, if applicable]

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    [Provide diagram detailing class hierarchy. Example:]
    ```
    views::View (Base layout unit)
       └── views::Textfield (Handles click/key input events and selection layers)
    ```
*   **WebUI (Web Frontend)**:
    [Provide diagram/list detailing inheritance. Example:]
    ```
    HTMLElement (Browser element base)
       └── LitElement / CrLitElement (Web UI host)
              └── CrInputElement (Reusable cr-input component wrapper)
    ```
