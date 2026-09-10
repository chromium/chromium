# Component Spec: FilterChips

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **FilterChips** component across Figma,
C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **FilterChips** component allows users to filter content or selections
interactively. It acts as a toggle button displaying an optional icon and text.
In the selected state, it typically displays a checkmark or highlighted
background.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)             | WebUI (Desktop)                                                                                                | Clank (Android)                                                                                                                                                                                                                    |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :------------------------------ | :------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `FilterChips`                                                                                                                       | `views::MdTextButton` / Generic | `<cr-chip>`                                                                                                    | `ChipView`                                                                                                                                                                                                                         |
| **Source Files**   | [Figma Link: `280:27253`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-27253) | N/A                             | [`ui/webui/resources/cr_elements/cr_chip/cr_chip.ts`](//src/ui/webui/resources/cr_elements/cr_chip/cr_chip.ts) | [`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant            | C++ Classes                | WebUI Element/Class            | Clank (Android) Implementation              |
| :----------------------- | :------------------------- | :----------------------------- | :------------------------------------------ |
| **Default (Unselected)** | Generic Button             | `<cr-chip>`                    | `ChipView` (`setSelected(false)`)           |
| **Selected**             | Generic Button (Checked)   | `<cr-chip selected>`           | `ChipView` (`setSelected(true)`)            |
| **Icon Slot**            | Handled natively by button | Handled via inner HTML / icons | `ChipView.setIcon()`, `ChipProperties.ICON` |

______________________________________________________________________

## 3. Component States

| Interactive State | Figma              | C++                           | WebUI                       | Clank (Android)                 |
| :---------------- | :----------------- | :---------------------------- | :-------------------------- | :------------------------------ |
| **Default**       | `state="Default"`  | Standard painting             | Standard layout             | `ChipView` normal state         |
| **Hovered**       | `state="Hovered"`  | `ButtonState::STATE_HOVERED`  | `cr-ripple` or CSS `:hover` | `@macro/chip_state_layer_color` |
| **Disabled**      | `state="Disabled"` | `ButtonState::STATE_DISABLED` | `[disabled]` attribute      | `ChipView.setEnabled(false)`    |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type                | Figma Property        | C++ Views | WebUI Variable / CSS             | Clank (Android) Equivalent                                        |
| :------------------------ | :-------------------- | :-------- | :------------------------------- | :---------------------------------------------------------------- |
| **Unselected Background** | Transparent / Surface | N/A       | `--color-sys-surface`            | `@macro/chip_bg_color` / `?attr/colorSurface`                     |
| **Unselected Outline**    | Tonal Outline         | N/A       | `--color-sys-tonal-outline`      | `@macro/chip_outline_color` / `?attr/colorOutline`                |
| **Selected Background**   | Tonal Container       | N/A       | `--color-sys-tonal-container`    | `@macro/chip_bg_selected_color` / `?attr/colorSecondaryContainer` |
| **Selected Text Color**   | On-Tonal Container    | N/A       | `--color-sys-on-tonal-container` | `?attr/colorOnSecondaryContainer`                                 |
| **Corner Radius**         | `8px`                 | N/A       | `border-radius: 8px`             | `@dimen/bookmark_bar_chip_corner_radius` (`8dp`)                  |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Component Availability**: C++ Views does not have a dedicated `FilterChip`
  UI primitive in `ui/views/controls`. Native implementation requires manually
  styling an `MdTextButton` or creating a custom view.
- **WebUI Integration**: `<cr-chip>` integrates a standard `<button>` internally
  with `CrRippleMixin`.
- **Clank Android Support**: Clank has first-class chip support via
  [`ChipView`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java)
  and
  [`ChipsCoordinator`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipsCoordinator.java),
  using `RecyclerView` adapters for horizontal chip lists.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Icon Swapping**: Figma defines swapping out icons for `check` when selected.
  `ChipView` in Clank supports auto-displaying checkmarks when selected via
  `ChipProperties.SHOW_CHECKMARK`.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Toggle Logic**: Filter chips should be used for boolean filters or
  multi-select arrays, not single-action navigation.
- **Clank Lists**: For horizontal scrolling chip carousels, use
  `ChipsCoordinator` backed by `ModelList`.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: Relies on standard button focus and `aria-label`/`role` projection.
- **Clank (Android)**: `ChipView` handles TalkBack focus and announcement of
  toggle states.
