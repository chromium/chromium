# Component Spec: IconButton

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **IconButton** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **IconButton** component is a compact, circular interactive element
containing a single vector icon. It is designed to trigger specialized or
high-frequency secondary actions within toolbars, cards, tables, or header bars.
It does not display a text label, relying on optical visual clarity and
accessibility labels to communicate intent.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                       | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                                          | Clank (Android)                                                                                                                                                                                                                              |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------ | :--------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `IconButton`                                                                                                                          | `views::ImageButton` (via `CreateIconButton`)                                            | `<cr-icon-button>`                                                                                                                       | `ChromeImageButton` / `@style/OverflowMenuButton` / `@style/ToolbarButton`                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `20268:1205`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20268-1205) | [ui/views/controls/button/image_button.h](//src/ui/views/controls/button/image_button.h) | [ui/webui/resources/cr_elements/cr_icon_button/cr_icon_button.ts](//src/ui/webui/resources/cr_elements/cr_icon_button/cr_icon_button.ts) | [ui/android/java/src/org/chromium/ui/widget/ChromeImageButton.java](//src/ui/android/java/src/org/chromium/ui/widget/ChromeImageButton.java)<br>[chrome/android/java/res/values/styles.xml](//src/chrome/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component         | C++ Views (Desktop)         | WebUI (Web Frontend)                                                                   | Clank (Android)                                                                                |
| :---------------- | :---------------------- | :-------------------------- | :------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------- |
| **Size: 16dp**    | `size=16dp`             | `MaterialIconStyle::kSmall` | CSS: `--cr-icon-button-icon-size: 16px;`<br>`--cr-icon-button-size: 28px;`             | `android:layout_width="28dp"`, `android:layout_height="28dp"`                                  |
| **Size: 20dp**    | `size=20dp` *(Default)* | `MaterialIconStyle::kLarge` | CSS: `--cr-icon-button-icon-size: 20px;`<br>`--cr-icon-button-size: 32px;` *(Default)* | `android:layout_width="48dp"`, `android:layout_height="48dp"` (`@dimen/min_touch_target_size`) |

______________________________________________________________________

## 3. Component States

| State                | Figma Component          | C++ Views (Desktop)                        | WebUI (Web Frontend)                      | Clank (Android)                                                        |
| :------------------- | :----------------------- | :----------------------------------------- | :---------------------------------------- | :--------------------------------------------------------------------- |
| **Default (Normal)** | `state=Default`          | `Button::ButtonState::STATE_NORMAL`        | Default hover-free state                  | `android:enabled="true"`                                               |
| **Hovered**          | `state=Hovered`          | `Button::ButtonState::STATE_HOVERED`       | `:hover:not([disabled])` pseudo-class     | Hover layer overlay (`@dimen/default_hovered_alpha`)                   |
| **Pressed**          | `state=Pressed`          | `Button::ButtonState::STATE_PRESSED`       | `:active` pseudo-class with `<cr-ripple>` | `?attr/selectableItemBackgroundBorderless` ripple                      |
| **Disabled**         | `state=Disabled`         | `Button::ButtonState::STATE_DISABLED`      | Attribute: `<cr-icon-button disabled>`    | Attribute: `android:enabled="false"` (`@dimen/default_disabled_alpha`) |
| **Focused**          | *(Commonly represented)* | Triggers custom `views::FocusRing` drawing | `:focus-visible:focus` pseudo-class       | Hardware / TalkBack focus ring                                         |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute                  | Figma Design Token                                                               | C++ Views (Desktop)                                | WebUI (Web Frontend)                                          | Clank (Android)                                                   |
| :-------------------------------- | :------------------------------------------------------------------------------- | :------------------------------------------------- | :------------------------------------------------------------ | :---------------------------------------------------------------- |
| **Base Foreground (Color)**       | `currentColor` / Derived from slot context                                       | Inherited theme foreground color                   | `color: var(--cr-icon-button-fill-color, currentColor);`      | `@macro/default_icon_color` / `?attr/colorOnSurface`              |
| **Hover Background Circle**       | `--desktop/sys/state-colors/state-hover-on-subtle`<br>(`rgba(31,31,31,0.06)`)    | Handled via InkDropHost controller overlays        | `background-color: var(--cr-hover-background-color);`         | State layer overlay                                               |
| **Active/Pressed Ripple**         | `--desktop/sys/state-colors/state-hover-on-prominent`<br>(`rgba(31,31,31,0.12)`) | Handled via dynamic ink drop color blending        | Resolved via `<cr-ripple>` opacity / `#ink` target            | `?attr/selectableItemBackgroundBorderless`                        |
| **Disabled Icon Opacity**         | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`)           | Handled automatically by Views disabling shader    | `opacity: var(--cr-disabled-opacity);` *(Resolves to `0.38`)* | `@color/default_icon_color_disabled` (`0.38` alpha)               |
| **Corner Radius**                 | `--desktop/corner-radius/fully-rounded`<br>(`999px`)                             | Circular clipping matches button radius            | `border-radius: 50%;` *(Circular mask)*                       | Unbounded circular ripple mask                                    |
| **Standard Size Outer Dimension** | `28px` / `32px` depending on `size`                                              | Bounded by standard layout insets / preferred size | `--cr-icon-button-size: 28px` or `32px`                       | Enforces min `48dp` touch target (`@dimen/min_touch_target_size`) |
| **Standard Size Inner Icon**      | `16px` / `20px` depending on `size`                                              | Resolved by vector icon scale parameter            | `--cr-icon-button-icon-size: 16px;` or `20px`                 | `24dp` (`@dimen/toolbar_button_icon_size`)                        |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Touch Target Enforcement (Mobile vs Desktop)

- **Desktop (Views & WebUI)**: Icon buttons commonly render at `28px`–`32px`
  square frames.
- **Clank (Android)**: Google Material guidelines require a minimum **`48dp`**
  touch target (`@dimen/min_touch_target_size`), even when the inner vector icon
  is only `20dp` or `24dp`.

### 2. Inner/Outer Dimensions and Custom Scaling

- **Figma**: Standardizes strictly on two variants: `16dp` and `20dp`.
- **WebUI**: Allows arbitrary scaling by overriding `--cr-icon-button-size` and
  `--cr-icon-button-icon-size`.
- **C++ Views**: Relies on static sizing parameters derived from Material Design
  standards.
- **Clank (Android)**: Handled via `android:padding` and
  `android:scaleType="centerInside"`.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Right-to-Left (RTL) Dir-Mirroring

- **Figma**: Directional flipping must be configured manually.
- **WebUI**: Automatically flips directional arrow icons along the X-axis unless
  `suppressRtlFlip` is set.
- **C++ Views**: Dynamically mirrors directional icons in RTL layout cycles.
- **Clank (Android)**: Android vector drawables support
  `android:autoMirrored="true"`.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Avoid Over-Cluttering**: Limit icon buttons inside toolbars to 3–4 actions
  max to maintain clean margins and focus.
- **Clear Visual Metaphor**: Icon targets must be immediately recognizable.
- **Sufficient Touch Target**: High-frequency icon buttons should maintain a
  minimum tap target of `48px` on mobile/touch layouts.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Mandatory `aria-label` or `aria-labelledby`.
- **C++ Views**: Set `accessible_name` via
  `GetViewAccessibility().SetName(accessible_name)`.
- **Clank (Android)**: Mandatory `android:contentDescription` or
  `setContentDescription()` with tooltip support via `TooltipCompat`.
