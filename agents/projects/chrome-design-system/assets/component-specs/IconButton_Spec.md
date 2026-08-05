# Component Spec: IconButton

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **IconButton** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **IconButton** component is a compact, circular interactive element containing a single vector icon. It is designed to trigger specialized or high-frequency secondary actions within toolbars, cards, tables, or header bars. It does not display a text label, relying on optical visual clarity and accessibility labels to communicate intent.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `IconButton` | `views::ImageButton` (via `CreateIconButton`) | `<cr-icon-button>` |
| **Source Files** | [Figma Link: `20268:1205`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20268-1205&m=dev) | [ui/views/controls/button/image_button.h](//src/ui/views/controls/button/image_button.h) | [ui/webui/resources/cr_elements/cr_icon_button/cr_icon_button.ts](//src/ui/webui/resources/cr_elements/cr_icon_button/cr_icon_button.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Size: 16dp** | `size=16dp` | `MaterialIconStyle::kSmall` | CSS: `--cr-icon-button-icon-size: 16px;`<br>`--cr-icon-button-size: 28px;` |
| **Size: 20dp** | `size=20dp` *(Default)* | `MaterialIconStyle::kLarge` | CSS: `--cr-icon-button-icon-size: 20px;`<br>`--cr-icon-button-size: 32px;` *(Default)* |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `state=Default` | `Button::ButtonState::STATE_NORMAL` | Default hover-free state |
| **Hovered** | `state=Hovered` | `Button::ButtonState::STATE_HOVERED` | `:hover:not([disabled])` pseudo-class |
| **Pressed** | `state=Pressed` | `Button::ButtonState::STATE_PRESSED` | `:active` pseudo-class with `<cr-ripple>` |
| **Disabled** | `state=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-icon-button disabled>` |
| **Focused** | *(Commonly represented)* | Triggers custom `views::FocusRing` drawing | `:focus-visible:focus` pseudo-class |

---

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Base Foreground (Color)** | `currentColor` / Derived from slot context | Inherited theme foreground color | `color: var(--cr-icon-button-fill-color, currentColor);` |
| **Hover Background Circle** | `--desktop/sys/state-colors/state-hover-on-subtle`<br>(`rgba(31,31,31,0.06)`) | Handled via InkDropHost controller overlays | `background-color: var(--cr-hover-background-color);` |
| **Active/Pressed Ripple** | `--desktop/sys/state-colors/state-hover-on-prominent`<br>(`rgba(31,31,31,0.12)`) | Handled via dynamic ink drop color blending | Resolved via `<cr-ripple>` opacity / `#ink` target |
| **Disabled Icon Opacity** | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`) | Handled automatically by Views disabling shader | `opacity: var(--cr-disabled-opacity);` *(Resolves to `0.38`)* |
| **Corner Radius** | `--desktop/corner-radius/fully-rounded`<br>(`999px`) | Circular clipping matches button radius | `border-radius: 50%;` *(Circular mask)* |
| **Standard Size Outer Dimension**| `28px` / `32px` depending on `size` | Bounded by standard layout insets / preferred size | `--cr-icon-button-size: 28px` or `32px` |
| **Standard Size Inner Icon** | `16px` / `20px` depending on `size` | Resolved by vector icon scale parameter | `--cr-icon-button-icon-size: 16px;` or `20px` |

---

## 5. Architectural & Implementation Gaps

### 1. Shape and Circular Boundary Masking
*   **Figma**: Uses `--desktop/corner-radius/fully-rounded` (`999px`) on a square frame to enforce a circle.
*   **WebUI**: Employs CSS `border-radius: 50%` in its core stylesheet. While visually identical, the implementation uses raw percentage calculations instead of relying on the system-defined fully-rounded px variable.
*   **C++ Views**: Handles boundaries and ink drop masks through dynamic shape geometry bounding rings (calculated during rendering), which fits native host platform requirements instead of static CSS boundaries.

### 2. Inner/Outer Dimensions and Custom Scaling
*   **Figma**: Standardizes strictly on two variants: `16dp` and `20dp`.
*   **WebUI**: Displays high adaptability. Although `32px` and `28px` are the primary styles, developers can scale the icon button arbitrarily to match settings panes or narrow sidebars by manually overriding the `--cr-icon-button-size` and `--cr-icon-button-icon-size` custom properties directly in the layout context.
*   **C++ Views**: Relies on static sizing parameters derived from the Material Design standards. Custom dimensions require passing a custom `gfx::Insets` struct during instantiation.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Right-to-Left (RTL) Dir-Mirroring
*   **Figma**: Does not explicitly model an RTL visual variation. Directional flipping must be configured manually by mirroring the nested icon asset.
*   **WebUI**: Incorporates built-in mirroring in its stylesheet. Directional arrow icons are automatically flipped along the X-axis (`transform: scaleX(-1)`) in RTL locales. To prevent this, developers must explicitly pass the `suppressRtlFlip` boolean attribute.
*   **C++ Views**: Handles horizontal flipping dynamically within the paint cycle when layout alignment is set to mirroring.

### 2. Focus Ring Rendering and Aesthetics
*   **Figma**: Lacks a dedicated focused variant in its state design set.
*   **WebUI**: Employs an inset box shadow `box-shadow: inset 0 0 0 2px var(--cr-focus-outline-color)` which draws the outline *inside* the button bounds.
*   **C++ Views**: Draws an external focus ring outside the circular boundaries of the icon button, ensuring optimal compliance with native host OS accessibility visual parameters.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Avoid Over-Cluttering**: Limit icon buttons inside toolbars to 3–4 actions max to maintain clean margins and focus.
*   **Clear Visual Metaphor**: Icon targets must be immediately recognizable. Avoid using obscure or highly customized icons where a standard material alternative is available (e.g., standard "trash" for Delete).
*   **Sufficient Touch Target**: High-frequency icon buttons should maintain a minimum tap target of `48px` to guarantee physical interactability on touchscreen layouts.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

#### **WebUI**
*   **Mandatory Accessibility Labels**: Since `<cr-icon-button>` contains no visible text, it **MUST** have an explicit `aria-label` or `aria-labelledby` property.
*   **Keyboard Operation**: Standard HTML tab selection is enforced. Focus is visible via `:focus-visible` styling, and clicking is triggered by hitting **Space** or **Enter**.

#### **C++ Views**
*   **Screen Reader Labels**: It is mandatory to provide an `accessible_name` string during button instantiation. Under the hood, Views registers this via `GetViewAccessibility().SetName(accessible_name)`.
*   **Focus Integrity**: Ensure `SetFocusBehavior(FocusBehavior::ALWAYS)` is retained to prevent the button from being skipped during keyboard tab cycles.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::Button (Focus, click handlers)
              └── views::ImageButton (Renders state-dependent image models)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── LitElement / CrLitElement (Reactive UI component)
              └── CrIconButtonElement (with CrRippleMixin for active ripples)
    ```
