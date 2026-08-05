# Component Spec: Text Fields (Views)

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Text Fields (Views)** component in Figma and C++ Views (Desktop).

---

## Overview

The **Text Fields (Views)** component is a text entry box designed to capture user string input in the native desktop interface (C++ Views). It displays a rounded-corner container with a neutral outline in its default state, highlights with an active focus ring when focused, and renders with a disabled container background and dimmed text when disabled.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) |
| :--- | :--- | :--- |
| **Component Name** | `Text Fields (Views)` | `views::Textfield` |
| **Source Files** | [Figma Link: `30230:10704`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=30230-10704&m=dev) | [ui/views/controls/textfield/textfield.h](//src/ui/views/controls/textfield/textfield.h) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) |
| :--- | :--- | :--- |
| **Standard Text Field** | Standard empty text field container | Native `views::Textfield` with default FocusableBorder |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) |
| :--- | :--- | :--- |
| **Default (Normal)** | `State=Default` | Normal state, using `ui::kColorTextfieldOutline` border and default background |
| **Hovered** | *(Inferred/Common)* | Shows ink drop hover highlight overlay using `ui::kColorTextfieldHover` |
| **Disabled** | `State=Disabled` | `SetEnabled(false)`: Renders with `ui::kColorTextfieldBackgroundDisabled` background and `ui::kColorTextfieldOutlineDisabled` border |
| **Selected (Focused)**| `State=Selected` | Triggers `views::FocusRing` drawing, which paints with the default cascading accent color (based on `ui::kColorFocusableBorderFocused`) |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) |
| :--- | :--- | :--- |
| **Default Outline Border** | `--desktop/sys/outline-colors/neutral-outline` (`#c7c7c7`) | `ui::kColorTextfieldOutline` |
| **Disabled Outline Border** | N/A *(No border shown)* | `ui::kColorTextfieldOutlineDisabled` |
| **Focus Ring Color** | `--desktop/sys/state-colors/state-focus-ring` (`#0b57d0`) | `ui::kColorFocusableBorderFocused` (via `GetCascadingAccentColor()`) |
| **Selection Highlight BG** | `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`) | `ui::kColorTextfieldSelectionBackground` |
| **Selection Text Color** | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`) | `ui::kColorTextfieldSelectionForeground` |
| **Disabled Container BG** | `--desktop/sys/state-colors/state-disabled-container` (`rgba(31,31,31,0.12)`) | `ui::kColorTextfieldBackgroundDisabled` |
| **Disabled Text Color** | `--desktop/sys/state-colors/state-disabled` (`rgba(31,31,31,0.38)`) | `ui::kColorTextfieldForegroundDisabled` (via `style::STYLE_DISABLED`) |
| **Base On-Surface Text** | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`) | `ui::kColorTextfieldForeground` (via `style::STYLE_PRIMARY`) |
| **Corner Radius** | `--desktop/corner-radius/8` (`8px`) | `LayoutProvider::Get()->GetCornerRadiusMetric(ShapeContextTokens::kTextfieldRadius)` (maps to `ShapeSysTokens::kSmall` = `8px`) |
| **Inner Padding (Horizontal)**| `--desktop/spacing/10` (`10px`) | `LayoutProvider::Get()->GetDistanceMetric(DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING)` (`10px`) |
| **Inner Padding (Vertical)** | `--desktop/spacing/9` (`9px`) | `LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING)` (`10px`) |
| **Font Size** | `--desktop/font_size/body-four` (`12px`) | `ui::kLabelFontSizeDelta` (`12px` target font size) |
| **Font Weight** | `--desktop/font_weight/regular` (`normal` / `400`) | Standard font weight (regular) |
| **Line Height** | `--desktop/line_height/body-four` (`18px`) | Standard line height |

---

## 5. Architectural & Implementation Gaps

### 1. Hardcoded Padding vs. LayoutProvider
In the C++ Views codebase, padding values are resolved using `LayoutProvider::Get()->GetDistanceMetric()`. For `views::Textfield`, the vertical padding resolves to `10px` (`DISTANCE_CONTROL_VERTICAL_TEXT_PADDING`), whereas the Figma design kit specifies a vertical padding of `9px` (`--desktop/spacing/9`). This causes a minor visual height layout deviation of 2px (1px top, 1px bottom).

### 2. Ink Drop Hover Overlay
In Figma, there is no explicit hover state layer defined in this spec frame. In C++ Views, `views::Textfield` automatically registers an `InkDropHost` overlay that changes color using `ui::kColorTextfieldHover` when the cursor hovers over the element.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Height Discrepancy in Disabled State
*   **Figma**: The disabled state variant has a height of `34px` (compared to the `36px` default). This is modeled by having the container fill inset by `-2.94%` vertically, which hides/omits the borders.
*   **C++ Views**: The textfield retains a constant preferred height/bounds (typically `36px` or derived layout bounds) in both enabled and disabled states. The border is not removed, but rather painted using `ui::kColorTextfieldOutlineDisabled`.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Accessible Naming**: Views text fields should have an accessible name assigned using `GetViewAccessibility().SetName()` or via an associated `views::Label` buddy.
*   **Standard Spacing**: When placing textfields in dialog layouts, use distance metrics provided by `LayoutProvider` (e.g. `DISTANCE_RELATED_CONTROL_VERTICAL` or `DISTANCE_UNRELATED_CONTROL_VERTICAL`) to maintain layout consistency.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **Focus Ring**: Native focus rings are configured using `FocusRing::Install(this)`. Outsets are disabled by default for textfields (`SetOutsetFocusRingDisabled(true)`) to draw the focus ring inside the outline border.
*   **Keyboard Actions**: Full support for common text cursor movement shortcuts (arrow keys, word leaps, home/end, select all) mapping to platform-native cursor/editing controls in `ui::TextInputClient`.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Textfield (Handles click/key input events and selection layers)
    ```
