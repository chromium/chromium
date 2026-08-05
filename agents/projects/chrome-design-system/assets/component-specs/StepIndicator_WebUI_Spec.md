# Component Spec: StepIndicator

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **StepIndicator** component (implemented as `<step-indicator>` in the Settings WebUI codebase) across Figma and WebUI (Web Frontend).

---

## Overview

The `StepIndicator` is a horizontal progress indicator composed of small dots used to represent the active position and total pages in a multi-step user onboarding or setup guide (e.g. Chrome Privacy Guide).

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | WebUI (Web Frontend) |
| :--- | :--- | :--- |
| **Component Name** | `StepIndicator` | `<step-indicator>` |
| **Source Files** | [Figma Link: `25345:1314`](https://www.figma.com/design/ZRB6863VRSstVNLN6WI3Pt/CDDS-Design-Kit---Settings--Chrome-?node-id=25345-1314&m=dev) | [chrome/browser/resources/settings/privacy_page/privacy_guide/step_indicator.ts](//src/chrome/browser/resources/settings/privacy_page/privacy_guide/step_indicator.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

*   **Dot Indicators**:
    *   Generates a row of dots whose count matches the `model.total` parameter.
    *   If `model.total` is 1 or less, the component renders nothing.
*   **Active Step**:
    *   Identified by matching the dot's index against `model.active`. The active dot is highlighted in primary blue color.
    *   All other dots are rendered in neutral grey to denote inactive/unvisited steps.

---

## 3. Component States

| State | Figma Property | WebUI CSS / State Property |
| :--- | :--- | :--- |
| **Active Dot** | `state=true` (Active) | `span.active` class applies active background color |
| **Inactive Dot** | `state=false` (Inactive) | `span` base class applies standard background color |

---

## 4. Design Token Comparison (Side-by-Side)

| Visual Property | Figma Design Token | WebUI CSS / Custom Property |
| :--- | :--- | :--- |
| **Dot Size (W x H)** | `8px` x `8px` | Width: `8px`, Height: `8px` |
| **Dot Spacing (Gap)** | `8px` | `margin: 0 4px` (resulting in `8px` gap between dots) |
| **Active Color (Light)** | `var(--desktop/sys/primary-colors/primary, #0b57d0)` | `var(--google-blue-600)` |
| **Active Color (Dark)** | — | `var(--google-blue-300)` |
| **Inactive Color (Light)**| `var(--desktop/sys/surface-colors/on-surface-subtle, #474747)` | `var(--google-grey-200)` |
| **Inactive Color (Dark)** | — | `var(--google-grey-500)` |

---

## 5. Architectural & Implementation Gaps

*   **Framework Version**:
    *   `<step-indicator>` is built using Polymer (`PolymerElement`), whereas newer shared components are built using Lit (`CrLitElement`).
*   **Color Token Reference**:
    *   *Figma Intent*: Uses modern system tokens (`--desktop/sys/primary-colors/primary`).
    *   *Implementation*: WebUI references specific legacy palette color tokens (`var(--google-blue-600)` and `var(--google-grey-200)`).

---

## 6. Styling, Variants, Features and States Mismatches

*   **Outer Margins**:
    *   Figma structures the layout using a flex container with `gap-[8px]` spacing. The start and end of the indicator row have zero extra margin.
    *   WebUI implements spacing using `margin: 0 4px` on each individual `<span>` dot element. While this achieves the correct `8px` spacing between dots, it adds an extra `4px` margin at the outer left and right bounds of the host container.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   Use step indicators for wizard-style setups, multi-stage dialogs, or guided tours.
*   Avoid displaying the indicator if the flow has less than 2 pages.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   The component includes a screen-reader-only element (`class="screen-reader-only"`) that dynamically formats the progress text (e.g. "Step 1 of 4") for screen readers via `i18n('privacyGuideSteps', active, total)`.

---

## 8. Inheritance Structure

*   **WebUI (Web Frontend)**:
    ```
    HTMLElement
       └── PolymerElement
             └── I18nMixin(PolymerElement)
                    └── StepIndicator (Manages dots calculation and a11y announcement templates)
    ```
