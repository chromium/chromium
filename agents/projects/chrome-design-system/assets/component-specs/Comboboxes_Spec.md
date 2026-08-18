# Component Spec: Comboboxes

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Comboboxes** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

Comboboxes allow users to select an option from a dropdown menu. They come in
two primary flavors: a standard dropdown (select) and an editable combobox
allowing users to type a custom value or filter choices.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                    | WebUI (Desktop)                                    | Clank (Android)                                                                                                                                                                                                                              |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------- | :------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Comboboxes`                                                                                                                        | `views::Combobox`                                                                      | `<select class="md-select">`                       | `Spinner` / `AutoCompleteTextView` / `DropdownPopupWindow`                                                                                                                                                                                   |
| **Source Files**   | [Figma Link: `280:26332`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26332) | [`ui/views/controls/combobox/combobox.h`](//src/ui/views/controls/combobox/combobox.h) | `ui/webui/resources/cr_elements/md_select_lit.css` | [`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DropdownPopupWindow.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DropdownPopupWindow.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant         | C++ Classes               | WebUI Element/Class          | Clank (Android) Implementation                    |
| :-------------------- | :------------------------ | :--------------------------- | :------------------------------------------------ |
| **Default**           | `views::Combobox`         | `<select class="md-select">` | `androidx.appcompat.widget.AppCompatSpinner`      |
| **Editable Combobox** | `views::EditableCombobox` | Custom input/dropdown pairs  | `AutoCompleteTextView` with `DropdownPopupWindow` |

______________________________________________________________________

## 3. Component States

| Interactive State | Figma              | C++ `views::Combobox` | WebUI `<select>`       | Clank (Android)           |
| :---------------- | :----------------- | :-------------------- | :--------------------- | :------------------------ |
| **Default**       | `state="Default"`  | Normal drawing        | Normal layout          | Default spinner style     |
| **Hovered**       | `state="Hovered"`  | InkDrop / Hover state | `:hover`               | State layer overlay       |
| **Pressed**       | `state="Pressed"`  | `IsMenuRunning()`     | Active / Dropdown open | Popup open state          |
| **Disabled**      | `state="Disabled"` | `SetEnabled(false)`   | `[disabled]` attribute | `android:enabled="false"` |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type              | Figma Property              | C++ Views                      | WebUI Variable / CSS              | Clank (Android) Equivalent                            |
| :---------------------- | :-------------------------- | :----------------------------- | :-------------------------------- | :---------------------------------------------------- |
| **Corner Radius**       | `8px`                       | Layout Provider                | `border-radius: 8px` / `4px`      | `@dimen/default_rounded_corner_radius` (`8dp`)        |
| **Padding**             | `10px`                      | `GetInsets()`                  | `padding` based on `md-select`    | `@dimen/dropdown_padding`                             |
| **Border Color**        | Neutral Outline             | `ui::kColorComboboxBackground` | `border-color`                    | `@macro/hairline_stroke_color` / `?attr/colorOutline` |
| **Background Color**    | Surface / Neutral Container | `ui::kColorComboboxBackground` | `background-color`                | `?attr/colorSurfaceContainer`                         |
| **Disabled Background** | State Disabled Container    | Inherits from system           | `--md-select-disabled-background` | `@dimen/default_disabled_alpha` overlay               |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **C++ Menu Integration**: `views::Combobox` ties directly into
  `ui::ComboboxModel` and spins up a native menu.
- **WebUI**: A native `<select>` element styled with `.md-select` is most
  common.
- **Clank (Android)**: Clank uses standard Android `Spinner` widgets for modal
  dropdown dialogs, or
  [`DropdownPopupWindow`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DropdownPopupWindow.java)
  when anchoring directly below the input field.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Arrow Customization**: The dropdown arrow in WebUI is often customized using
  background SVG masks, C++ draws it via vector icons, and Clank uses Android
  theme `spinnerDropDownItemStyle` or `app:endIconMode="dropdown_menu"`.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **When to use**: Use when there are more than 4-5 options. For fewer options,
  consider radio buttons or segmented controls.

### 2. Platform Consistency & Accessibility (a11y)

- **Keyboard**: Standard up/down arrow navigation.
- **Clank (Android)**: Supports TalkBack announcement and bottom-sheet picker
  dialogs.
