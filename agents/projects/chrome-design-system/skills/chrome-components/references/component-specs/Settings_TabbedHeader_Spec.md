# Component Spec: Settings - TabbedHeader

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **TabbedHeader** component (implemented
as `<cr-tabs>` in WebUI and `TabLayout` in Android) across Figma, WebUI (Web
Frontend), and Clank (Android).

______________________________________________________________________

## Overview

The `TabbedHeader` is a horizontal tab selection container used to partition
views and categories on WebUI pages and mobile bottom sheets / tab switchers. It
supports two main modes: "Left Aligned" layout (featuring icons and specific
margins) and "Full Width" layout (for centered, text-only tabs).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                             | C++ Views (Desktop) | WebUI (Desktop)                                                                                              | Clank (Android)                                                                                                                          |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------ | :----------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `TabbedHeader`                                                                                                                                              | **N/A**             | `<cr-tabs>`                                                                                                  | `TabLayout` / `@style/Widget.BrowserUI.TabLayout`                                                                                        |
| **Source Files**   | [Figma Link: `60352:8528`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=60352-8528&t=FMUhNTpxly8KQpO3-11) | **N/A**             | [ui/webui/resources/cr_elements/cr_tabs/cr_tabs.ts](//src/ui/webui/resources/cr_elements/cr_tabs/cr_tabs.ts) | [components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

- **Left Aligned**:
  - Figma: `type="Left Aligned"`
  - WebUI: `<cr-tabs>` with left alignment (`gap: 32px`).
  - Clank: `TabLayout` with `app:tabMode="scrollable"` and
    `app:tabGravity="start"`.
- **Full Width / Fixed**:
  - Figma: `type="Full Width"`
  - WebUI: `<cr-tabs>` with `--cr-tabs-flex: 1`.
  - Clank: `TabLayout` with `app:tabMode="fixed"` and `app:tabGravity="fill"`.
- **Selected / Active**:
  - Active tabs display a prominent indicator bar and primary color.

______________________________________________________________________

## 3. Component States

| State                  | Figma Property    | WebUI CSS / State Property                   | Clank (Android)                     |
| :--------------------- | :---------------- | :------------------------------------------- | :---------------------------------- |
| **Default (Inactive)** | `state="Default"` | `.tab` / unselected styling                  | `tab.isSelected() == false`         |
| **Selected (Active)**  | `state="Active"`  | `.tab.selected` selected color and indicator | Active tab with indicator bar       |
| **Hover**              | —                 | Hover state transitions                      | State layer overlay                 |
| **Focused**            | —                 | `.tab:focus` focus outline                   | Hardware focus / TalkBack selection |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Visual Property              | Figma Design Token                                                         | WebUI CSS / Custom Property                             | Clank (Android) Equivalent                                   |
| :--------------------------- | :------------------------------------------------------------------------- | :------------------------------------------------------ | :----------------------------------------------------------- |
| **Tab Container Height**     | `48px`                                                                     | `var(--cr-tabs-height, 48px)`                           | `@dimen/tab_layout_height` (`48dp`)                          |
| **Tab Font (Left Aligned)**  | `family: "typeface/Headline" (Roboto:Medium)`, Weight: `500`, Size: `14px` | `var(--cr-tabs-font-size, 14px)`, `font-weight: 500`    | `@style/TextAppearance.TextMedium.Secondary` (`14sp`)        |
| **Indicator Height**         | `3px` / `2px`                                                              | `var(--cr-tabs-selection-bar-width, 2px)`               | `app:tabIndicatorHeight="3dp"`                               |
| **Indicator Shape (Radius)** | `3px` / `2px`                                                              | `var(--cr-tabs-selection-bar-radius, 2px)`              | `app:tabIndicatorAnimationMode="linear"`                     |
| **Selected Color**           | `var(--desktop/sys/primary-colors/primary, #0b57d0)`                       | `var(--cr-tabs-selected-color, var(--google-blue-600))` | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| **Unselected Color**         | `var(--desktop/sys/surface-colors/on-surface-subtle, #474747)`             | `var(--cr-secondary-text-color)`                        | `@macro/default_text_color_secondary`                        |
| **Bottom Separator**         | `var(--desktop/sys/surface-colors/surface-variant, #e1e3e1)`, `1px`        | Wrapper border or sibling element                       | `@macro/hairline_stroke_color`                               |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Slide Indicator**: Both `<cr-tabs>` and Android's `TabLayout` implement
  physics-based spring slide indicator animations when switching tab selection.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Tab Modes**: Android Material 3 explicitly defines `fixed` and `scrollable`
  modes on `TabLayout`, directly matching Figma's "Full Width" and "Left
  Aligned" layout designs.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use tabs to switch views within the same logical container level.
- Keep tab labels brief and descriptive.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Ensure `role="tablist"` and `role="tab"` with
  `ArrowLeft`/`ArrowRight` key handlers.
- **Clank (Android)**: `TabLayout` manages TalkBack tab selection announcements
  automatically.
