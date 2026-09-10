# Component Spec: Settings - StepIndicator

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **StepIndicator** component across
Figma, WebUI (Web Frontend), and Clank (Android).

______________________________________________________________________

## Overview

The `StepIndicator` is a horizontal progress indicator composed of small dots
used to represent the active position and total pages in a multi-step user
onboarding, setup guide (e.g. Chrome Privacy Guide), or mobile promo carousel.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                            | C++ Views (Desktop) | WebUI (Desktop)                                                                                                                                                        | Clank (Android)                                                                                                                              |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------ | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `StepIndicator`                                                                                                                                            | **N/A**             | `<step-indicator>`                                                                                                                                                     | `PageIndicatorView` / `DotsPageIndicator`                                                                                                    |
| **Source Files**   | [Figma Link: `25345:1314`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=60352-8594&t=FMUhNTpxly8KQpO3-4) | **N/A**             | [chrome/browser/resources/settings/privacy_page/privacy_guide/step_indicator.ts](//src/chrome/browser/resources/settings/privacy_page/privacy_guide/step_indicator.ts) | [ui/android/java/src/org/chromium/ui/widget/PageIndicatorView.java](//src/ui/android/java/src/org/chromium/ui/widget/PageIndicatorView.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

- **Dot Indicators**:
  - Generates a row of dots whose count matches the `model.total` parameter.
  - If `model.total` is 1 or less, the component renders nothing.
- **Active Step**:
  - Identified by matching the dot's index against `model.active`. The active
    dot is highlighted in primary blue color.
  - All other dots are rendered in neutral grey to denote inactive/unvisited
    steps.

______________________________________________________________________

## 3. Component States

| State            | Figma Property           | WebUI CSS / State Property                          | Clank (Android)                                                 |
| :--------------- | :----------------------- | :-------------------------------------------------- | :-------------------------------------------------------------- |
| **Active Dot**   | `state=true` (Active)    | `span.active` class applies active background color | Active dot drawable / `@macro/default_control_color_active`     |
| **Inactive Dot** | `state=false` (Inactive) | `span` base class applies standard background color | Inactive dot drawable / `@macro/default_control_color_inactive` |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Visual Property            | Figma Design Token                                             | WebUI CSS / Custom Property                           | Clank (Android) Equivalent                                            |
| :------------------------- | :------------------------------------------------------------- | :---------------------------------------------------- | :-------------------------------------------------------------------- |
| **Dot Size (W x H)**       | `8px` x `8px`                                                  | Width: `8px`, Height: `8px`                           | `8dp` x `8dp` (`@dimen/page_indicator_dot_size`)                      |
| **Dot Spacing (Gap)**      | `8px`                                                          | `margin: 0 4px` (resulting in `8px` gap between dots) | `8dp` margin (`@dimen/page_indicator_dot_spacing`)                    |
| **Active Color (Light)**   | `var(--desktop/sys/primary-colors/primary, #0b57d0)`           | `var(--google-blue-600)`                              | `@macro/default_control_color_active` / `?attr/colorPrimary`          |
| **Active Color (Dark)**    | —                                                              | `var(--google-blue-300)`                              | `@macro/default_control_color_active`                                 |
| **Inactive Color (Light)** | `var(--desktop/sys/surface-colors/on-surface-subtle, #474747)` | `var(--google-grey-200)`                              | `@macro/default_control_color_inactive` / `?attr/colorSurfaceVariant` |
| **Inactive Color (Dark)**  | —                                                              | `var(--google-grey-500)`                              | `@macro/default_control_color_inactive`                               |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Frameworks**: WebUI `<step-indicator>` is built using Polymer/Lit with slot
  rendering. In Clank, `PageIndicatorView` draws dot states directly onto a
  custom Android canvas or uses `ViewPager2` page sync listeners.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Outer Margins**:
  - Figma structures the layout using flex `gap-[8px]`.
  - WebUI implements spacing using `margin: 0 4px` on each individual `<span>`.
  - Android paints circular drawables with explicit spacing coordinates
    calculated in `onDraw()`.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use step indicators for wizard-style setups, multi-stage dialogs, or guided
  tours.
- Avoid displaying the indicator if the flow has less than 2 pages.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Includes screen-reader-only text ("Step 1 of 4") via
  `i18n('privacyGuideSteps', active, total)`.
- **Clank (Android)**: `PageIndicatorView` sends
  `AccessibilityEvent.TYPE_VIEW_SELECTED` events on page step change.
