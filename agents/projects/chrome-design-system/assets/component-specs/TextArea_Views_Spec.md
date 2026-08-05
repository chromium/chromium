# Component Spec: TextArea (Views-only)

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **TextArea** component across Figma and C++ Views (Desktop).

---

## Overview

The `TextArea` component is a multi-line, scrollable text field designed for entering longer text content. It inherits the core functionality of `views::Textfield` (e.g., focus handling, borders, typography styles, cursor control, text selection, copy/paste, and accessibility) and extends it to support multi-line layout flags, word wrapping, scrolling, and cursor movement (up/down lines). It supports four visual states: Default, Selected/Focused, Error/Invalid, and Disabled.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) |
| :--- | :--- | :--- |
| **Component Name** | `.base.TextArea(Views)` | `views::Textarea` |
| **Source Files** | [Figma Link: `3155:885`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=3155-885&t=KdwbLO7J6jWrzj20-11) | [ui/views/controls/textarea/textarea.h](//src/ui/views/controls/textarea/textarea.h) |

> [!NOTE]
> `views::Textarea` inherits from `views::Textfield` and overrides specific keyboard and mouse events to handle multiline input, while delegating core painting, styling, and color resolution to `views::Textfield`.

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Property / Slot | C++ Views Class / API | Description |
| :--- | :--- | :--- |
| **Multiline Container** | `views::Textarea` | Inherits from `views::Textfield`; calls `GetRenderText()->SetMultiline(true)` and `GetRenderText()->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS)`. |
| **Input Type** | `SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA)` | Configures the system input method to treat the control as a multiline text area. |
| **Placeholder Text** | `set_placeholder_text_draw_flags()` | Set with `gfx::Canvas::MULTI_LINE` to ensure placeholder text wraps when it exceeds the width. |
| **Container Border** | `views::FocusableBorder` | Set as the view's border. Standard corner radius is set via `SetCornerRadius()`. |
| **Outer Padding** | `views::FocusableBorder::SetInsets()` | Sets the spacing between the border and the inner text area. Uses layout metrics for consistency. |

---

## 3. Component States

| State | Figma Property | C++ Views State / Visuals |
| :--- | :--- | :--- |
| **Default** | `state="Default"` | `!GetInvalid() && GetEnabled() && !HasFocus()`<br>- Border Color: `ui::kColorTextfieldOutline` (`kColorSysNeutralOutline`, 1px solid)<br>- Text Color: `ui::kColorTextfieldForeground` (`kColorSysOnSurface`) |
| **Selected (Focused)** | `state="Selected"` | `HasFocus()` / Standard outset focus ring active<br>- Focus Ring: Installed via `views::FocusRing::Install(this)`<br>- Ring Color: `ui::kColorFocusableBorderFocused` (`kColorSysStateFocusRing`, 2px solid) |
| **Error (Invalid)** | `state="Error"` | `GetInvalid() == true` / Invalid border & focus ring styling<br>- Border Color: `ui::kColorTextfieldOutlineInvalid` (`kColorSysError`, 2px solid)<br>- Focus Ring: Focus ring styling maps to invalid-style via `views::FocusRing::Get(this)->SetInvalid(true)` |
| **Disabled** | `state="Disabled"` | `!GetEnabled()` / Background container filled<br>- Border Color: `ui::kColorTextfieldOutlineDisabled` (`SK_ColorTRANSPARENT`) <br>- BG Color: `ui::kColorTextfieldBackgroundDisabled` (`kColorSysStateDisabledContainer`) <br>- Text Color: `ui::kColorTextfieldForegroundDisabled` (`kColorSysStateDisabled`) |

---

## 4. Design Token Comparison (Side-by-Side)

| Property | Figma Token / Value | C++ Views Token / Value |
| :--- | :--- | :--- |
| **Container Background (Default)** | Surface / Transparent | `ui::kColorTextfieldBackground` / `ui::kColorSysSurface` |
| **Container Background (Disabled)** | `var(--desktop/sys/state-colors/state-disabled-container)` (rgba(31,31,31,0.12)) | `ui::kColorTextfieldBackgroundDisabled` / `ui::kColorSysStateDisabledContainer` |
| **Border Color (Default)** | `var(--desktop/sys/outline-colors/neutral-outline)` (#c7c7c7) | `ui::kColorTextfieldOutline` / `ui::kColorSysNeutralOutline` |
| **Border Color (Focused)** | `var(--desktop/sys/state-colors/state-focus-ring)` (#0b57d0) | `ui::kColorFocusableBorderFocused` / `ui::kColorSysStateFocusRing` |
| **Border Color (Error/Invalid)** | `var(--desktop/sys/error-colors/error)` (#b3261e) | `ui::kColorTextfieldOutlineInvalid` / `ui::kColorSysError` (High-contrast blend) |
| **Border Width (Default)** | 1px | 1px (line thickness of `views::FocusableBorder`) |
| **Border Width (Focused/Error)** | 2px | 2px (thickness of `views::FocusRing`) |
| **Corner Radius** | `var(--desktop/corner-radius/8)` (8px) | `ShapeContextTokens::kTextfieldRadius` -> `ShapeSysTokens::kSmall` (8px) |
| **Typography (Input Text)** | `desktop/body/four` (12px, regular, 18px line height) | `style::CONTEXT_TEXTFIELD` + `style::STYLE_PRIMARY` -> `style::STYLE_BODY_4` (12px, Weight::NORMAL, 18px line-height) |
| **Typography (Supporting Text)** | `desktop/body/five` (11px, regular, 16px line height) | `style::CONTEXT_TEXTFIELD_SUPPORTING_TEXT` -> `style::STYLE_BODY_5` (11px, Weight::NORMAL, 16px line-height) |
| **Text Color (Default)** | `var(--desktop/sys/surface-colors/on-surface)` (#1f1f1f) | `ui::kColorTextfieldForeground` / `ui::kColorSysOnSurface` |
| **Text Color (Disabled)** | `var(--desktop/sys/state-colors/state-disabled)` (rgba(31,31,31,0.38)) | `ui::kColorTextfieldForegroundDisabled` / `ui::kColorSysStateDisabled` |
| **Text Color (Error/Invalid)** | `var(--desktop/sys/error-colors/error)` (#b3261e) | `ui::kColorTextfieldForegroundPlaceholderInvalid` / `ui::kColorSysError` |
| **Padding (Top/Bottom)** | `var(--desktop/spacing/10)` (10px) | `views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING` (10px) |
| **Padding (Left/Right)** | `var(--desktop/spacing/10)` (10px) | `views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING` (10px) |

---

## 5. Architectural & Implementation Gaps

*   **Supporting/Error Text Slot**: The Figma design treats "Supporting text" as a built-in subview/state property of the `TextArea` component. In C++ Views, `views::Textarea` (and `views::Textfield`) does not provide a native slot or label for rendering supporting text. Developers must compose a parent layout (e.g., a vertical `views::BoxLayout`) containing both the `views::Textarea` and a separate `views::Label` configured with context `style::CONTEXT_TEXTFIELD_SUPPORTING_TEXT` to display error messages or notes.
*   **Static vs. Dynamic Component Height**: The Figma component lists a fixed height of `84px` for the text area block. C++ `views::Textarea` dynamically calculates its preferred size or fills its layout container. To achieve a fixed height, developers must explicitly call `SetPreferredSize()` or use layouts that constrain height.

---

## 6. Styling, Variants, Features and States Mismatches

*   **Disabled Border outline**: When disabled, Figma removes the border line entirely and shows a gray-filled container. In C++ Views, `views::Textfield` achieves this by setting the border color to `SK_ColorTRANSPARENT` while drawing the disabled background color underneath.
*   **Focus Ring invalid state updates**: When the text field content is invalid, the focus ring is styled in red. Developers must ensure that whenever `SetInvalid()` is called, they also invoke `FocusRing::Get(this)->SetInvalid(invalid_)` to keep the focus ring visual representation in sync with the input's validity state.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- Use `views::Textarea` when multi-line text input is expected (e.g., user feedback description, notes). Use `views::Textfield` for single-line inputs.
- Keep placeholder text short and descriptive to instruct users on what to enter.
- Always provide clear supporting or error text below the text area when input is invalid or has validation requirements.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **Keyboard Navigation**:
  - `Tab` moves focus to/from the text area.
  - `Enter` inserts a newline character (`\n`) in `views::Textarea` (captured via `PreHandleKeyPressed`) rather than submitting the dialog or form.
  - `Up` and `Down` arrow keys navigate lines of text vertically (handled via `DoExecuteTextEditCommand`).
- **Accessibility (a11y)**:
  - Accessibility name and description should be set via `views::ViewAccessibility`.
  - The accessibility role is automatically managed as `ax::mojom::Role::kTextField` (or `kTextFieldMultiline`) by the views class tree.

### 3. Icon Usage Guidelines
- The standard `views::Textarea` does not include leading/trailing icons. If icons are required (e.g., copy to clipboard or character counter), they should be placed outside the text area or composed in a wrapper view layout.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout & painting unit)
       └── views::Textfield (Manages single-line text rendering, IME input, selection, focus rings, borders, and colors)
              └── views::Textarea (Enables multi-line display, wraps words, handles Enter key for newlines, and handles vertical cursor navigation)
    ```
