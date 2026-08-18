# Component Spec: TextArea

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **TextArea** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **TextArea** component is a multi-line, scrollable text field designed for
entering longer text content across C++ Views desktop windows, WebUI web
frontends (`<cr-textarea>`), and Android mobile screens (`EditTextWithLeading` /
`TextInputLayout`). It supports word wrapping, newlines (`Enter`), auto-grow
behavior, supporting/error footers, character counters, read-only/disabled
states, and accessible error announcements.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                        | C++ Views (Desktop)                                                                  | WebUI (Desktop)                                                                                                              | Clank (Android)                                                                                                                                  |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `.base.TextArea(Views)` / `.base.TextArea(WebUI)`                                                                                                                                                                                                                                                                                      | `views::Textarea`                                                                    | `<cr-textarea>`                                                                                                              | `EditTextWithLeading` (multiline) / `TextInputLayout`                                                                                            |
| **Source Files**   | [Figma Link (Views): `3155:885`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=3155-885&t=KdwbLO7J6jWrzj20-11)<br>[Figma Link (WebUI): `39836:4315`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=39836-4315&t=KdwbLO7J6jWrzj20-11) | [ui/views/controls/textarea/textarea.h](//src/ui/views/controls/textarea/textarea.h) | [ui/webui/resources/cr_elements/cr_textarea/cr_textarea.ts](//src/ui/webui/resources/cr_elements/cr_textarea/cr_textarea.ts) | [ui/android/java/src/org/chromium/ui/widget/EditTextWithLeading.java](//src/ui/android/java/src/org/chromium/ui/widget/EditTextWithLeading.java) |

> [!NOTE] `views::Textarea` inherits from `views::Textfield` and overrides
> specific keyboard and mouse events to handle multiline input. In WebUI,
> `<cr-textarea>` shares core styles and CSS variables with `<cr-input>` via
> `cr-input-style-lit`.

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Property / Slot   | C++ Views Class / API                    | WebUI Custom Element Attribute           | Clank (Android) Implementation                      |
| :---------------------- | :--------------------------------------- | :--------------------------------------- | :-------------------------------------------------- |
| **Multiline Container** | `views::Textarea` (`SetMultiline(true)`) | `<textarea>` wrapped in custom element   | `EditTextWithLeading` (`inputType="textMultiLine"`) |
| **Label Text**          | Composed with sibling `views::Label`     | `label` attribute (`<div id="label">`)   | `android:hint` / Floating hint in `TextInputLayout` |
| **Placeholder Text**    | `set_placeholder_text_draw_flags()`      | `placeholder` attribute                  | `app:placeholderText`                               |
| **Invalid State**       | `views::FocusRing::SetInvalid(true)`     | `invalid` boolean attribute              | `app:errorEnabled="true"`                           |
| **Auto-grow**           | Dynamic layout / `SetPreferredSize()`    | `autogrow` boolean attribute             | Android `wrap_content` with line limits             |
| **Read Only**           | `SetReadOnly(true)`                      | `readonly` attribute (`8px 8px` corners) | `android:focusable="false"`                         |
| **Supporting Text**     | Composed with sibling `views::Label`     | `firstFooter` slot / attribute           | `app:helperText`                                    |
| **Character Count**     | N/A *(Custom layout needed)*             | `secondFooter` slot / attribute          | `app:counterEnabled="true"`                         |

______________________________________________________________________

## 3. Component States

| State                  | Figma Property     | C++ Views State / Visuals                                 | WebUI CSS / Custom Elements                                        | Clank (Android)                 |
| :--------------------- | :----------------- | :-------------------------------------------------------- | :----------------------------------------------------------------- | :------------------------------ |
| **Default**            | `state="Default"`  | `ui::kColorTextfieldOutline` (1px solid)                  | `--cr-input-background-color` (1px underline)                      | `@macro/hairline_stroke_color`  |
| **Selected (Focused)** | `state="Selected"` | `views::FocusRing::Install(this)` (2px solid)             | `--cr-input-focus-color` (2px solid underline)                     | 2dp stroke `?attr/colorPrimary` |
| **Error (Invalid)**    | `state="Error"`    | `views::FocusRing::Get(this)->SetInvalid(true)`           | `--cr-input-error-color` (2px red underline & label)               | `app:errorEnabled="true"`       |
| **Disabled**           | `state="Disabled"` | `!GetEnabled()` / `ui::kColorTextfieldBackgroundDisabled` | `<cr-textarea disabled>` (`--color-textfield-background-disabled`) | `android:enabled="false"`       |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute                    | Figma Token / Value                                           | C++ Views Token / Value                         | WebUI Variable / Value                          | Clank (Android) Equivalent                                   |
| :---------------------------------- | :------------------------------------------------------------ | :---------------------------------------------- | :---------------------------------------------- | :----------------------------------------------------------- |
| **Container Background (Default)**  | `var(--desktop/sys/surface-colors/surface-variant)` (#e1e3e1) | `ui::kColorTextfieldBackground`                 | `--cr-input-background-color`                   | `?attr/colorSurfaceContainer`                                |
| **Container Background (Disabled)** | `var(--desktop/sys/state-colors/state-disabled-container)`    | `ui::kColorTextfieldBackgroundDisabled`         | `--color-textfield-background-disabled`         | `@dimen/default_disabled_alpha`                              |
| **Underline / Border (Default)**    | Neutral Outline (`#c7c7c7`)                                   | `ui::kColorTextfieldOutline`                    | `--cr-input-border-bottom` (`1px solid`)        | `@macro/hairline_stroke_color` / `?attr/colorOutline`        |
| **Underline / Border (Focused)**    | Focus Ring (`#0b57d0`)                                        | `ui::kColorFocusableBorderFocused`              | `--cr-input-focus-color` (`2px solid`)          | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| **Underline / Border (Error)**      | Error Color (`#b3261e`)                                       | `ui::kColorTextfieldOutlineInvalid`             | `--cr-input-error-color`                        | `?attr/colorError`                                           |
| **Corner Radius**                   | `var(--desktop/corner-radius/8)` (`8px`)                      | `ShapeSysTokens::kSmall` (`8px`)                | `--cr-input-border-radius` (`8px 8px 0 0`)      | `@dimen/default_rounded_corner_radius` (`8dp`)               |
| **Typography (Label)**              | `desktop/body/five` (`11px`)                                  | `style::STYLE_BODY_5` (`11px`)                  | `#label` CSS (`11px`)                           | `@style/TextAppearance.TextSmall` (`12sp`)                   |
| **Typography (Input Text)**         | `desktop/body/four` (`12px`)                                  | `style::STYLE_BODY_4` (`12px`)                  | `#input` CSS (`12px`)                           | `@style/TextAppearance.TextMedium.Primary` (`14sp`-`16sp`)   |
| **Text Color (Default)**            | `var(--desktop/sys/surface-colors/on-surface)` (#1f1f1f)      | `ui::kColorTextfieldForeground`                 | `--cr-input-color`                              | `@macro/default_text_color` / `?attr/colorOnSurface`         |
| **Text Color (Disabled)**           | `var(--desktop/sys/state-colors/state-disabled)`              | `ui::kColorTextfieldForegroundDisabled`         | `var(--color-textfield-foreground-disabled)`    | `@macro/default_text_color_secondary`                        |
| **Padding (All sides)**             | `var(--desktop/spacing/10)` (`10px`)                          | `views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING` | `--cr-input-padding-top/bottom/start/end: 10px` | `12dp` - `16dp`                                              |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Supporting Text & Counter Slots

- **WebUI**: Helper and character counter texts are rendered via `firstFooter`
  and `secondFooter` attributes/slots built into `<cr-textarea>`.
- **Clank (Android)**: `TextInputLayout` manages `app:helperText` and
  `app:counterMaxLength` built-in.
- **C++ Views**: `views::Textarea` does not provide native supporting text or
  character counter slots; callers must compose a parent box layout containing
  auxiliary `views::Label` widgets.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Input Line-Height

- The Figma visual design specifies `18px` (`desktop/body/four`) line-height for
  primary input text. The WebUI component hardcodes `line-height: 16px` on the
  `#input` text area element in `cr_input_style_lit.css`, resulting in slightly
  tighter line spacing.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use `<cr-textarea>` / `views::Textarea` / `EditTextWithLeading` (multiline)
  when users need to enter multiple lines of text (e.g. feedback comments,
  notes, settings descriptions).
- Utilize `autogrow` on WebUI or `wrap_content` on Android to allow the field to
  expand dynamically with content.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Keyboard Navigation**:
  - Focus input via `Tab`.
  - Pressing `Enter` adds a newline character (`\n`) in `views::Textarea` and
    `<cr-textarea>` rather than submitting a dialog or form.
  - `Up` and `Down` arrow keys navigate lines of text vertically.
- **Accessibility (a11y)**:
  - **WebUI**: The live-region attribute `aria-live` on the footer container is
    set to `assertive` when `invalid` is active and `polite` otherwise.
  - **Clank (Android)**: `TextInputLayout` announces validation errors directly
    to TalkBack.
