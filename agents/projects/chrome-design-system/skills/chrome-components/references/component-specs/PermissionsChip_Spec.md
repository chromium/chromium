# Component Spec: PermissionsChip

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **PermissionsChip** component across
Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The PermissionsChip serves as a highly visible, rounded button used specifically
for prompting users for browser-level permissions (e.g., Location, Camera,
Notifications). It sits within the Omnibox / LocationBar on desktop and mobile.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                   | C++ Views (Desktop)                                                                                                                        | WebUI (Desktop)                                                                                                                | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                  |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `PermissionsChip`                                                                                                                 | `PermissionChipView`                                                                                                                       | `<permission-chip>`                                                                                                            | `ChipView` / `LocationBar` Permission Chip                                                                                                                                                                                                                                                                                                                                                                                       |
| **Source Files**   | [Figma Link: `288:7602`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=288-7602) | [`chrome/browser/ui/views/permissions/chip/permission_chip_view.h`](//src/chrome/browser/ui/views/permissions/chip/permission_chip_view.h) | [`chrome/browser/resources/webui_toolbar/permission_chip.ts`](//src/chrome/browser/resources/webui_toolbar/permission_chip.ts) | [`components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/chips/ChipView.java)<br>[`chrome/android/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java`](//src/chrome/android/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant        | C++ `PermissionChipView` Method              | WebUI `<permission-chip>` Property | Clank (Android) Implementation                  |
| :------------------- | :------------------------------------------- | :--------------------------------- | :---------------------------------------------- |
| **Variant: Primary** | Default Prominent Theme                      | Standard setup                     | `ChipView` with `@macro/chip_bg_selected_color` |
| **Variant: Error**   | Error theme based on `PermissionPromptStyle` | Error specific colors              | Error icon drawable with red tint               |
| **Variant: Neutral** | Quiet theme                                  | Neutral coloring                   | Standard outline `ChipView`                     |
| **Icon Only**        | `AnimateCollapse()`                          | `isFullyCollapsed` / `chipState`   | Location bar security icon                      |
| **Icon + Text**      | `AnimateExpand()`                            | Text visible                       | Expanded permission chip                        |

______________________________________________________________________

## 3. Component States

| Interactive State | Figma             | C++ `PermissionChipView`     | WebUI `<permission-chip>`      | Clank (Android)                         |
| :---------------- | :---------------- | :--------------------------- | :----------------------------- | :-------------------------------------- |
| **Default**       | `state="Default"` | `ButtonState::STATE_NORMAL`  | Standard layout                | Normal chip style                       |
| **Hovered**       | `state="Hovered"` | `ButtonState::STATE_HOVERED` | Native hover/focus logic       | State layer overlay                     |
| **Pressed**       | `state="Pressed"` | `ButtonState::STATE_PRESSED` | Dispatches events to ToolbarUI | `?attr/selectableItemBackground` ripple |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type             | Figma Property                  | C++ Views                                 | WebUI Variable / CSS          | Clank (Android) Equivalent                                   |
| :--------------------- | :------------------------------ | :---------------------------------------- | :---------------------------- | :----------------------------------------------------------- |
| **Primary Background** | Primary                         | `PermissionChipTheme::kNormalVisibility`  | `--cr-primary-colors-primary` | `@macro/default_control_color_active` / `?attr/colorPrimary` |
| **Error Background**   | Error                           | Error states mapping                      | `--cr-error-colors-error`     | `?attr/colorError`                                           |
| **Height**             | `24px`                          | Managed by parent layout / `GetPadding()` | CSS defined                   | `@dimen/location_bar_chip_height` (`28dp`-`32dp`)            |
| **Corner Radius**      | Fully Rounded (999px)           | `GetCornerRadius()`                       | `border-radius: 999px`        | `@dimen/chip_corner_radius` (999dp)                          |
| **Padding**            | `4px` (Icon), `6px 10px` (Text) | `GetPadding()`                            | Explicit padding              | `paddingStart="8dp"`, `paddingEnd="12dp"`                    |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Animation Handling**: In C++, `PermissionChipView` leverages
  `gfx::SlideAnimation` to expand/collapse. In Clank, the
  `LocationBarCoordinator` dynamically shows the chip or opens the
  `PageInfoController` bottom sheet upon tap.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Mobile vs Desktop Location**: On desktop, the chip is integrated inside the
  Omnibox text field container. On Android, permission prompts are typically
  presented via bottom sheets or modal permission dialogs before collapsing into
  the location bar security icon.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Urgency**: Use the Prominent / Primary colored chip only for permissions
  requiring immediate user attention.
- **Collapsing**: Chips should expand on first appearance and gracefully
  collapse to reduce Omnibox clutter.

### 2. Platform Consistency & Accessibility (a11y)

- **C++ Views**: Manages explicit accessibility strings via `AnnounceText` and
  `AnnounceAlert`.
- **Clank (Android)**: Triggers TalkBack alert announcement and launches
  `PageInfo` on tap.
