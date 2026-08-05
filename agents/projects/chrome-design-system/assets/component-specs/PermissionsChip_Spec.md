# Component Spec: PermissionsChip

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **PermissionsChip** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The PermissionsChip serves as a highly visible, rounded button used specifically for prompting users for browser-level permissions (e.g., Location, Camera, Notifications). It sits within the Omnibox on desktop and is used in the WebUI settings.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `PermissionsChip` | `PermissionChipView` | `<permission-chip>` |
| **Source Files** | [Figma Link: `288:7602`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=288-7602) | [`chrome/browser/ui/views/permissions/chip/permission_chip_view.h`](//src/chrome/browser/ui/views/permissions/chip/permission_chip_view.h) | [`chrome/browser/resources/webui_toolbar/permission_chip.ts`](//src/chrome/browser/resources/webui_toolbar/permission_chip.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant | C++ `PermissionChipView` Method | WebUI `<permission-chip>` Property |
| :--- | :--- | :--- |
| **Variant: Primary** | Default Prominent Theme | Standard setup |
| **Variant: Error** | Error theme based on `PermissionPromptStyle` | Error specific colors |
| **Variant: Neutral** | Quiet theme | Neutral coloring |
| **Icon Only** | `AnimateCollapse()` | `isFullyCollapsed` / `chipState` |
| **Icon + Text** | `AnimateExpand()` | Text visible |

---

## 3. Component States

| Interactive State | Figma | C++ `PermissionChipView` | WebUI `<permission-chip>` |
| :--- | :--- | :--- | :--- |
| **Default** | `state="Default"` | `ButtonState::STATE_NORMAL` | Standard layout |
| **Hovered** | `state="Hovered"` | `ButtonState::STATE_HOVERED` | Native hover/focus logic |
| **Pressed** | `state="Pressed"` | `ButtonState::STATE_PRESSED` | Dispatches events to ToolbarUI |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ Views | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Primary Background** | Primary | `PermissionChipTheme::kNormalVisibility` | `--cr-primary-colors-primary` |
| **Error Background** | Error | Error states mapping | `--cr-error-colors-error` |
| **Height** | `24px` | Managed by parent layout / `GetPadding()` | CSS defined |
| **Corner Radius** | Fully Rounded (999px) | `GetCornerRadius()` | `border-radius: 999px` |
| **Padding** | `4px` (Icon), `6px 10px` (Text) | `GetPadding()` | Explicit padding |

---

## 5. Architectural & Implementation Gaps

* **Animation Handling**: In C++, `PermissionChipView` leverages `gfx::SlideAnimation` to smoothly grow/shrink between the Icon Only and Icon+Text states. In WebUI, `<permission-chip>` listens to `transitionend` events heavily synchronized with the backend (`BrowserProxyImpl`) to signal animation completion.

---

## 6. Styling, Variants, Features and States Mismatches

* **Theme Enforcement**: The C++ implementation manages colors dynamically by overriding `GetBackgroundColor()` and `GetForegroundColor()` based on its `PermissionChipTheme`. The WebUI uses direct bindings to `PermissionChipState` to alter classes.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Urgency**: Use the `Important` / Primary colored chip only for permissions requiring immediate attention. Fallback to Neutral for standard or quiet permission requests.
- **Collapsing**: Chips should expand on first appearance and gracefully collapse to an `iconOnly` state to reduce Omnibox clutter.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **C++ Views**: Manages explicit accessibility strings via `AnnounceText` and `AnnounceAlert`.
- **WebUI**: Integrates closely with `TrackedElementManager` for targeted focus/highlight changes.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    `views::View` → `views::Button` → `views::LabelButton` → `views::MdTextButton` → `PermissionChipView`
*   **WebUI (Web Frontend)**:
    `HTMLElement` → `LitElement` → `PermissionChipElement` (`<permission-chip>`)