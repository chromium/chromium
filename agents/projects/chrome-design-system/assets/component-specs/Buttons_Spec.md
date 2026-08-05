# Component Spec: Buttons

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Buttons** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **Buttons** component is a core interactive control that allows users to trigger actions, execute commands, or submit forms. It supports text labels, optional leading or trailing icons, and multiple levels of visual emphasis.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Buttons` | `views::MdTextButton` / `views::LabelButton` | `<cr-button>` |
| **Source Files** | [Figma Link: `20268:1070`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20268-1070) | [ui/views/controls/button/md_text_button.h](//src/ui/views/controls/button/md_text_button.h)<br>[ui/views/controls/button/label_button.h](//src/ui/views/controls/button/label_button.h) | [ui/webui/resources/cr_elements/cr_button/cr_button.ts](//src/ui/webui/resources/cr_elements/cr_button/cr_button.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Primary Style** | `Variant=Primary` | `ui::ButtonStyle::kProminent` | `<cr-button class="action-button">` |
| **Tonal Style** | `Variant=Tonal` | `ui::ButtonStyle::kTonal` | `<cr-button class="tonal-button">` |
| **Outlined Style** | `Variant=Outlined` | `ui::ButtonStyle::kDefault` | `<cr-button>` *(Default style)* |
| **Text Style** | `Variant=Text` | `ui::ButtonStyle::kText` | `<cr-button>` *(Flat style)* |
| **Leading Icon** | `Icons=Leading` | `LabelButton::SetImageModel()` | Slot: `<slot name="prefix-icon">` |
| **Trailing Icon** | `Icons=Trailing` | `views::MdTextButtonWithDownArrow` or custom `LabelButtonImageContainer` | Slot: `<slot name="suffix-icon">` |

---

## 3. Component States

| State | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Default (Normal)** | `State=Default` | `Button::ButtonState::STATE_NORMAL` | Default state / no modifiers |
| **Hovered** | `State=Hovered` | `Button::ButtonState::STATE_HOVERED` | `:hover` pseudo-class<br>`#hoverBackground` overlay |
| **Pressed (Pushed)** | `State=Pressed` | `Button::ButtonState::STATE_PRESSED` | `:active` pseudo-class<br>`<cr-ripple>` (Ink ripple) |
| **Disabled** | `State=Disabled` | `Button::ButtonState::STATE_DISABLED` | Attribute: `<cr-button disabled>` |
| **Focused** | *(Commonly represented)* | Triggers custom `views::FocusRing` drawing | `:focus` / `:focus-visible` pseudo-classes<br>`.focus-outline-visible` class |

---

## 4. Design Token Comparison (Side-by-Side)

This table tracks how specific design tokens (colors, typography, spacing, and shapes) defined in the Figma design system map directly to C++ Views and WebUI configurations, including cases where hardcoded equivalents or custom fallbacks are utilized.

| Design Attribute | Figma Design Token | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Primary Container Background** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`) | `ui::kColorButtonBackgroundProminent` | `--color-button-background-prominent` |
| **On-Primary Foreground** | `--desktop/sys/primary-colors/on-primary`<br>(`white`) | `ui::kColorButtonForegroundProminent` | `--color-button-foreground-prominent` |
| **Tonal Container Background** | `--desktop/sys/container-colors/tonal-container`<br>(`#d3e3fd`) | `ui::kColorButtonBackgroundTonal` | `--color-button-background-tonal` |
| **On-Tonal Foreground** | `--desktop/sys/container-colors/on-tonal-container`<br>(`#041e49`) | `ui::kColorButtonForegroundTonal` | `--color-button-foreground-tonal` |
| **Border Outline Color** | `--desktop/sys/outline-colors/tonal-outline`<br>(`#a8c7fa`) | `ui::kColorButtonBorder` | `--color-button-border` |
| **Disabled Container BG** | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorButtonBackgroundProminentDisabled` | `--color-button-background-prominent-disabled` |
| **Disabled Foreground** | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`) | `ui::kColorButtonForegroundDisabled` | `--color-button-foreground-disabled` |
| **Hover State Overlay** | `--desktop/sys/state-colors/state-hover-on-prominent` / `state-hover-on-subtle` | `ui::kColorButtonHoverBackgroundText` | `--cr-hover-background-color` / `--cr-hover-on-prominent-background-color` |
| **Font Family** | `--desktop/font/body`<br>(`"Google_Sans_Text:Medium"`) | Native font family resolved by `ui::ResourceBundle` | Default system font family |
| **Font Weight** | `--desktop/font_weight/medium`<br>(`500` / medium) | `MediumWeightForUI()` *(Returns `500`)* | `font-weight: 500;` *(Hardcoded in `cr_button.css`)* |
| **Font Size** | `--desktop/font_size/button`<br>(`13px`) | `gfx::PlatformFont::GetFontSizeDelta(13)` | `font-size: 13px;` *(Derived from base text/button settings)* |
| **Line Height** | `--desktop/line_height/button`<br>(`20px`) | Handled via Typography Provider heights | `line-height: 20px;` *(Default)* / `154%` *(For Prominent style)* |
| **Padding (Horizontal)** | `--desktop/spacing/16`<br>(`16px` outer horizontal padding) | `DistanceMetric::kDistanceButtonHorizontalPadding` | `padding: 8px 16px;` *(Hardcoded in `cr_button.css`)* |
| **Padding (Vertical)** | `--desktop/spacing/8`<br>(`8px` outer vertical padding) | `DistanceMetric::kDistanceButtonVerticalPadding` | `padding: 8px 16px;` *(Hardcoded in `cr_button.css`)* |
| **Gap (Icon-to-Label)** | `--desktop/spacing/8`<br>(`8px`) | Handled internally in `LabelButton::GetImageLabelSpacing()` | `gap: 8px;` *(Hardcoded in `cr_button.css`)* |
| **Corner Radius** | `--desktop/corner-radius/fully-rounded`<br>(`999px`) | `ShapeContextTokens::kButtonRadius` *(Resolves to fully pill-shaped button)* | `border-radius: 100px;` *(Pill style hardcoded in `cr_button.css`)* |

---

## 5. Architectural & Implementation Gaps

### 1. Shape & Corner Radius Gaps
*   **Figma**: Uses `--desktop/corner-radius/fully-rounded` which is set to **`999px`** to enforce a safe, pill-shaped edge.
*   **WebUI**: Implements a hardcoded **`border-radius: 100px;`** inside `cr_button.css`. While it visually achieves the same pill shape as `999px` for standard button heights, it does not use the CSS design token and has a differing hardcoded value.
*   **C++ Views**: Uses `ShapeContextTokens::kButtonRadius` which is resolved dynamically by the layout provider. This usually maps to a smaller, specific radius based on standard Material 3 specs (e.g., `8px` or `12px` depending on context and chrome-specific design rules) rather than a "fully rounded" `999px` or `100px` pill shape.

### 2. Tonal Color Values (Static vs. Dynamic)
*   **Figma**: Has static hardcoded hexadecimal values for tonal containers: background is **`#d3e3fd`** (`tonal-container`) and text is **`#041e49`** (`on-tonal-container`).
*   **C++ Views / WebUI**: Both framework components bind these properties to the dynamic color pipeline (`kColorSysTonalContainer` / `--color-button-background-tonal`). These colors are **dynamically generated** at runtime based on the user's active OS/Chrome theme, meaning they will frequently differ from the static `#d3e3fd` value in Figma.

### 3. Font Size & Inheritance
*   **Figma**: Directly defines and applies `--desktop/font_size/button` set to **`13px`**.
*   **WebUI**: `<cr-button>` does **not** specify an explicit font-size within its `cr_button.css` stylesheet. Instead, it inherits the text size from its surrounding context or layout host, which can lead to rendering differences (e.g., `12px` or `13px`) depending on where the button is placed.
*   **C++ Views**: Explicitly forces `13px` via `gfx::PlatformFont::GetFontSizeDelta(13)`, which is scaled natively according to the operating system's display scale (DPI).

### 4. Text Line Height
*   **Figma**: Standardizes `--desktop/line_height/button` to **`20px`** for all variants.
*   **WebUI**: Uses `line-height: 20px;` as the default fallback in `cr_button.css`. However, when styled as `.action-button` (Primary style) or `.cancel-button`, it overrides this with **`line-height: 154%;`** (which scales relative to the inherited font size).

### 5. Spacing & Padding with Icons
*   **Figma**: Uses symmetric outer padding values regardless of icon presence: `--desktop/spacing/16` (`16px`) for horizontal and `--desktop/spacing/8` (`8px`) for vertical.
*   **WebUI**: Adjusts paddings **asymmetrically** to balance optical weight when slots are populated:
    *   With leading icon: `padding-inline-start` is reduced to **`12px`** (`--icon-block-padding-small`), while `padding-inline-end` remains **`16px`** (`--icon-block-padding-large`).
    *   With trailing icon: `padding-inline-start` remains **`16px`**, while `padding-inline-end` is reduced to **`12px`**.
    These asymmetric shifts do not exist in the static Figma component.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Styling & Variant Mismatches
*   **The "Text" (Flat) Variant in WebUI**:
    *   **Figma** has a dedicated flat `"Text"` variant (no border, transparent background).
    *   **C++ Views** has a corresponding built-in style `ui::ButtonStyle::kText`.
    *   **WebUI's `<cr-button>` does not have a built-in class** (like `.text-button` or `.flat-button`) to automatically remove the border and background. To achieve a text-only button, developers must manually override custom properties inline (e.g., setting `--cr-button-border: none;` or `--cr-button-background-color: transparent;`).

### 2. Feature & Layout Mismatches (Icons)
*   **Simultaneous Leading & Trailing Icons in C++ Views**:
    *   **Figma** has native support for a `"Leading and Trailing"` variant (two icons on opposite sides of the text label).
    *   **WebUI** handles this easily by populating both slots simultaneously (`slot="prefix-icon"` and `slot="suffix-icon"`).
    *   **C++ Views' `views::LabelButton` / `views::MdTextButton` does not natively support both leading and trailing icons out-of-the-box**. It is architected around a single image model (`SetImageModel()`), placing one image adjacent to the text. To support a leading icon and a trailing icon together, developers must build a custom `LabelButtonImageContainer` layout or override the button's internal layout manually.
*   **Dropdown/Trailing Arrows**:
    *   In **Figma**, you simply choose `Icons=Trailing` and draw the arrow.
    *   In **C++ Views**, adding a trailing dropdown arrow requires instantiating a completely different class: **`views::MdTextButtonWithDownArrow`**, rather than simply setting a property on the standard `MdTextButton`.

### 3. State Mismatches
*   **Focus Ring (Accessibility)**:
    *   The **Figma** component variant set does not model a `"Focused"` state.
    *   **C++ Views** and **WebUI** explicitly implement and display highly visible focus rings (`views::FocusRing` and `.focus-outline-visible`) to meet accessibility (a11y) standards.
*   **Default Button Behavior (Window Integration)**:
    *   **C++ Views** buttons can be marked as default (`SetIsDefault(true)`). This dynamically alters the button's theme paint properties and animates the focus border to indicate that pressing "Enter" will trigger it.
    *   **Figma** and **WebUI's `<cr-button>`** do not have a built-in "default" visual state tracker; this is handled programmatically (e.g., via HTML form submit events in the web frame).

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
*   **Visual Hierarchy & Variant Selection**: Use primary actions selectively to guide users. Do not overflow pages with primary buttons.
*   **Action Pairing & Layout**: Cancel buttons are placed on the left, action/submit buttons on the right.
*   **Labeling**: Use strong, active action verbs. Use sentence case in WebUI, and title case in Views.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
*   **WebUI**: `<cr-button>` enforces `role="button"` and `tabindex="0"`. Handles space/enter natively to click.
*   **C++ Views**: Use `GetViewAccessibility().SetName()` to feed accessibility strings. Set accelerators for Cancel (`Esc`) and Default (`Enter`) buttons.

### 3. Icon Usage Guidelines
*   **Leading Icons**: Categorize and add visual weight to recurring layout items.
*   **Trailing Icons**: Reserved for state indicators (e.g. dropdown arrows).

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout/rendering unit)
       └── views::Button (Focusable, clickable base class)
              └── views::LabelButton (Adds text & single-image support)
                     └── views::MdTextButton (Adds Material 3 specification styling)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Native browser base)
       └── LitElement / CrLitElement (Reactive web component base)
              └── CrButtonElement (with CrRippleMixin for ink ripples)
    ```
