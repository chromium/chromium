# Component Spec: Dialog Modal

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Dialog Modal** (referred to as `Dialog Template` or `Key UIs / Dialog Modal` in Figma) across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The **Dialog Modal** is a foundational overlay component used in Chromium to present critical information, require user decisions, or show structured content in a modal layer above the primary interface. The component supports two layout configurations:
1. **DEFAULT**: A standard dialog containing a title, a large content area, and a bottom button bar.
2. **WITH ILLUSTRATION**: An extended layout featuring a prominent 120px tall illustration/banner at the top of the dialog frame, positioned above the title, content, and buttons.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Key UIs / Dialog Modal` | `views::BubbleDialogDelegateView` <br>(or declarative `views::BubbleDialogModelHost`) | `<cr-dialog>` |
| **Source Files** | [Figma Link: `323:21577`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-21577&m=dev) | [bubble_dialog_delegate_view.h](//src/ui/views/bubble/bubble_dialog_delegate_view.h)<br>[bubble_dialog_model_host.h](//src/ui/views/bubble/bubble_dialog_model_host.h) | [cr_dialog.ts](//src/ui/webui/resources/cr_elements/cr_dialog/cr_dialog.ts)<br>[cr_dialog.html.ts](//src/ui/webui/resources/cr_elements/cr_dialog/cr_dialog.html.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant / Slot | C++ Views Implementation | WebUI Implementation |
| :--- | :--- | :--- |
| **DEFAULT Layout** | Standard `BubbleDialogDelegate` layout. The client view provides the content body via `SetContentsView()`. | Standard `<cr-dialog>` container wrapping slots for title, body, and buttons. |
| **WITH ILLUSTRATION Layout** | Handled via `BubbleFrameView::SetHeaderView()` with a custom banner view, or declaratively using `ui::DialogModel::Builder::SetBannerImage()`. | Placed in `<slot name="header">` inside `<cr-dialog>`. *(Note: Requires visual reordering; see Gaps section).* |
| **Title Slot** | Set via `DialogDelegate::SetTitle()` or `DialogModel::Builder::SetTitle()`. Renders as a `views::Label` inside the dialog frame. | `<slot name="title">` nested within the top-level `.top-container`. |
| **Body Content Slot** | Custom `views::View` supplied as the client contents. | `<slot name="body">` nested within `.body-container.cr-scrollable`. |
| **Button Bar (Left tertiary)** | Set via `DialogDelegate::SetExtraView()` (typically configured as an outlined `views::MdTextButton` using `ButtonStyle::kDefault`). | An outlined `<cr-button>` placed on the left side of `<slot name="button-container">`. |
| **Button Bar (Right secondary)**| Standard `Cancel` dialog button. Configured as `views::MdTextButton` with `ButtonStyle::kTonal`. | A tonal `<cr-button class="tonal-button cancel-button">` in `<slot name="button-container">`. |
| **Button Bar (Right primary)** | Standard `OK` dialog button. Configured as `views::MdTextButton` with `ButtonStyle::kProminent`. | A prominent `<cr-button class="action-button">` in `<slot name="button-container">`. |

---

## 3. Component States

| Interactive State | Figma State | C++ Views Implementation | WebUI Implementation |
| :--- | :--- | :--- | :--- |
| **Default** | State: Default | Standard default state for the dialog and buttons. | Standard element rendering. |
| **Hovered** | State: Hovered | Button hover states are managed via `views::InkDrop` highlights. | CSS `:hover` state. Hovered buttons display the `#hoverBackground` layer. |
| **Pressed** | State: Pressed | Ink drop transitions to the pressed state. | CSS `:active` state. Ripple animations are triggered on click/tap. |
| **Disabled** | State: Disabled | Controlled via `SetEnabled(false)`. Colors resolve to disabled IDs. | Applied via `[disabled]` attribute. Styling overrides are set in `:host([disabled])`. |
| **Focused** | State: Focused | `views::FocusRing` is drawn around the active element. | CSS `:focus` or `:focus-visible` states. Focus ring is styled via `--cr-focus-outline-color`. |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Property | Figma Spec | C++ Views Token / Value | WebUI Variable / Value |
| :--- | :--- | :--- | :--- |
| **Dialog Background** | `--desktop/sys/surface-colors/surface` (#FFFFFF) | `ui::kColorBubbleBackground` | `--cr-dialog-background-color` <br>(resolves to `--color-webui-dialog-background`) |
| **Dialog Corner Radius**| `12px` (`--desktop/corner-radius/12`) | `ShapeContextTokens::kBubbleRadius` (12px) | `--cr-dialog-border-radius` <br>(default `8px`, requires override to `12px`) |
| **Dialog Elevation** | `desktop/elevation/3` | `BubbleBorder::Shadow::DIALOG_SHADOW` | Default `box-shadow` on `<dialog>` element |
| **Dialog Padding** | `20px` (`--desktop/spacing/20`) | `INSETS_DIALOG` (typically 20px) | `20px` (body), `20px` (title), `16px` (buttons) |
| **Title Font Family** | `Google Sans` | `TypographyProvider::Get().GetFont(...)` | `--cr-dialog-font-family` |
| **Title Font Size** | `16px` (`--desktop/font_size/headline-four`) | Context: `CONTEXT_DIALOG_TITLE` (16px) | `--cr-dialog-title-font-size` <br>(default ~15px, requires `16px` override) |
| **Title Line Height** | `24px` (`--desktop/line_height/headline-four`)| Standard label height (24px) | `line-height: 24px` |
| **Illustration Height** | `120px` | Set dynamically on the header image view | Height: `120px` on element in slot `header` |
| **Illustration Background**| `--desktop/sys/container-colors/tonal-container` | `ui::kColorSecondaryContainer` | `--color-button-background-tonal` |
| **Button Spacing** | `8px` | `DISTANCE_RELATED_BUTTON_HORIZONTAL` (8px) | `margin-inline-end: 8px` (on `.cancel-button`) |

---

## 5. Architectural & Implementation Gaps

### 1. WebUI Slot Order Gap
In WebUI, the `<slot name="header">` within `<cr-dialog>` is structured *below* the `.top-container` (which contains the title slot). However, in the Figma design's `WITH ILLUSTRATION` variant, the illustration/header area is positioned *above* the title.
*   **Impact**: Directly using the standard `<slot name="header">` will result in the illustration appearing under the title. WebUI implementations must use custom CSS ordering (e.g., CSS flexbox `order` properties on the `#content-wrapper` or dialog parts) to visually position the header above the title.

### 2. Corner Radius Discrepancies
WebUI's `<cr-dialog>` has a default border radius of `8px`. C++ Views uses a default bubble border radius of `12px` (matching the Figma CDDS spec).
*   **Impact**: WebUI pages implementing this design must explicitly override `--cr-dialog-border-radius: 12px;` to maintain platform uniformity.

### 3. Responsive vs. Fixed Heights
The Figma designs showcase a fixed height of `450px` for both dialog variants. In both production codebases (Views and WebUI), the dialog height is responsive and wraps the content dynamically.
*   **Impact**: Developers should not hardcode the `450px` height in code. The fixed height in Figma is purely for template layout mockups.

---

## 6. Styling, Variants, Features and States Mismatches

### 1. Split Button Bar Layout
Figma structures the button bar with a single "Outlined" button on the left, and a "Tonal" + "Primary" button group on the right.
*   **Views**: Fully supported out-of-the-box. The left button maps to `SetExtraView()`, the right-tonal button maps to the `Cancel` button, and the right-primary button maps to the `OK` button.
*   **WebUI**: The `<slot name="button-container">` is a single flex container. WebUI developers must manually place all three buttons inside this slot and use custom spacing/alignment (such as `margin-right: auto` on the left button) to achieve the split layout.

### 2. Double-Icon Support / Launch Badges
The header area in the Figma mockup includes launch status badges ("FULLY LAUNCHED", "VIEWS", "WEBUI") and icons. These are metadata tags for CDDS documentation and are not part of the runtime component spec. Developers should ignore these status chips when implementing the dialog layout.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Variant Selection**:
  - Use the **DEFAULT** variant for standard workflows, simple settings confirmations, or warning prompts.
  - Use the **WITH ILLUSTRATION** variant for prominent feature promotions, first-run onboarding experiences, or celebratory milestone completions.
- **Button Ordering**:
  - The primary action must always be placed on the far right (styled as Prominent/Primary).
  - The cancellation/secondary action should sit directly to the left of the primary action (styled as Tonal/Cancel).
  - A tertiary or alternative action (e.g., "Learn More") should be placed on the far left (styled as Outlined).

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI Accessibility**:
  - Ensure `<cr-dialog>` has an appropriate `closeText` string mapped to the close button's `aria-label`.
  - The dialog automatically traps focus inside its boundary. The default focus should land on the primary action button.
  - Pressing `Escape` must dismiss the dialog unless `noCancel` is explicitly set to true.
- **C++ Views native focus**:
  - Focus rings are installed automatically on buttons via `FocusRing::Install`.
  - Ensure the dialog defines the correct default button via `DialogDelegate::SetDefaultButton()`.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base layout unit)
       └── views::WidgetDelegate (Window state manager)
              └── views::BubbleDialogDelegateView (Rounded bubble dialog delegate)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser element base)
       └── LitElement / CrLitElement (Web UI host)
              └── CrDialogElement (Reusable cr-dialog component)
    ```
