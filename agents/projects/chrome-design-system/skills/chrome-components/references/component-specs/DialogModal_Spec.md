# Component Spec: Dialog Modal

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Dialog Modal** (referred to as
`Dialog Template` or `Key UIs / Dialog Modal` in Figma) across Figma, C++ Views
(Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Dialog Modal** is a foundational overlay component used in Chromium to
present critical information, require user decisions, or show structured content
in a modal layer above the primary interface. The component supports two layout
configurations:

1. **DEFAULT**: A standard dialog containing a title, a large content area, and
   a bottom button bar.
2. **WITH ILLUSTRATION**: An extended layout featuring a prominent 120px tall
   illustration/banner at the top of the dialog frame, positioned above the
   title, content, and buttons.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                                                                                                    | WebUI (Desktop)                                                                                                                                                      | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Key UIs / Dialog Modal`                                                                                                            | `views::BubbleDialogDelegateView` <br>(or declarative `views::BubbleDialogModelHost`)                                                                                  | `<cr-dialog>`                                                                                                                                                        | `ModalDialogView` / `PromoDialog` / `ActionConfirmationDialog`                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Source Files**   | [Figma Link: `323:21577`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-21577) | [bubble_dialog_delegate_view.h](//src/ui/views/bubble/bubble_dialog_delegate_view.h)<br>[bubble_dialog_model_host.h](//src/ui/views/bubble/bubble_dialog_model_host.h) | [cr_dialog.ts](//src/ui/webui/resources/cr_elements/cr_dialog/cr_dialog.ts)<br>[cr_dialog.html.ts](//src/ui/webui/resources/cr_elements/cr_dialog/cr_dialog.html.ts) | [components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/PromoDialog.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/PromoDialog.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant / Slot             | C++ Views Implementation                                                                        | WebUI Implementation                                                              | Clank (Android) Implementation                                                             |
| :------------------------------- | :---------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------- |
| **DEFAULT Layout**               | Standard `BubbleDialogDelegate` layout with `SetContentsView()`.                                | Standard `<cr-dialog>` container wrapping slots for title, body, and buttons.     | `ModalDialogView` bound via `ModalDialogProperties.CUSTOM_VIEW` or `MESSAGE_PARAGRAPH_1`.  |
| **WITH ILLUSTRATION Layout**     | Handled via `BubbleFrameView::SetHeaderView()` or `ui::DialogModel::Builder::SetBannerImage()`. | Placed in `<slot name="header">` inside `<cr-dialog>`.                            | `PromoDialog` / `PromoDialogLayout` or `ModalDialogProperties.TITLE_ICON`.                 |
| **Title Slot**                   | Set via `DialogDelegate::SetTitle()` / `DialogModel::Builder::SetTitle()`.                      | `<slot name="title">` in `.top-container`.                                        | `ModalDialogProperties.TITLE` rendered with `@style/TextAppearance.AlertDialogTitleStyle`. |
| **Body Content Slot**            | Custom `views::View` supplied as the client contents.                                           | `<slot name="body">` in `.body-container.cr-scrollable`.                          | `ModalDialogProperties.CUSTOM_VIEW` or `MESSAGE_PARAGRAPH_1`.                              |
| **Button Bar (Left tertiary)**   | Set via `DialogDelegate::SetExtraView()` (outlined `views::MdTextButton`).                      | An outlined `<cr-button>` placed on the left of `<slot name="button-container">`. | Managed via `DualControlLayout` / `ModalDialogProperties.NEGATIVE_BUTTON_TEXT`.            |
| **Button Bar (Right secondary)** | Standard `Cancel` button (`views::MdTextButton` with `kTonal`).                                 | `<cr-button class="tonal-button cancel-button">`.                                 | `ModalDialogProperties.NEGATIVE_BUTTON_TEXT` with `@style/AlertDialogButtonStyle`.         |
| **Button Bar (Right primary)**   | Standard `OK` button (`views::MdTextButton` with `kProminent`).                                 | `<cr-button class="action-button">`.                                              | `ModalDialogProperties.POSITIVE_BUTTON_TEXT` with `@style/AlertDialogButtonStyle`.         |

______________________________________________________________________

## 3. Component States

| Interactive State | Figma State     | C++ Views Implementation                        | WebUI Implementation                                  | Clank (Android) Implementation                    |
| :---------------- | :-------------- | :---------------------------------------------- | :---------------------------------------------------- | :------------------------------------------------ |
| **Default**       | State: Default  | Standard default state for dialog and buttons.  | Standard element rendering.                           | Standard dialog view hierarchy.                   |
| **Hovered**       | State: Hovered  | Button hover via `views::InkDrop` highlights.   | CSS `:hover` state with `#hoverBackground`.           | `@dimen/default_hovered_alpha` state layer.       |
| **Pressed**       | State: Pressed  | Ink drop transitions to pressed state.          | CSS `:active` state with `<cr-ripple>`.               | Android Material ripple drawable.                 |
| **Disabled**      | State: Disabled | Controlled via `SetEnabled(false)`.             | Applied via `[disabled]` attribute.                   | `ModalDialogProperties.POSITIVE_BUTTON_DISABLED`. |
| **Focused**       | State: Focused  | `views::FocusRing` drawn around active control. | CSS `:focus-visible` with `--cr-focus-outline-color`. | Android accessibility/keyboard focus highlight.   |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Property              | Figma Spec                                       | C++ Views Token / Value                    | WebUI Variable / Value                          | Clank (Android) Equivalent                                                              |
| :-------------------------- | :----------------------------------------------- | :----------------------------------------- | :---------------------------------------------- | :-------------------------------------------------------------------------------------- |
| **Dialog Background**       | `--desktop/sys/surface-colors/surface` (#FFFFFF) | `ui::kColorBubbleBackground`               | `--cr-dialog-background-color`                  | `SemanticColorUtils.getDialogBgColor(context)` / `@color/dialog_bg_color`               |
| **Dialog Corner Radius**    | `12px` (`--desktop/corner-radius/12`)            | `ShapeContextTokens::kBubbleRadius` (12px) | `--cr-dialog-border-radius` (12px)              | `@dimen/popup_bg_corner_radius_16dp` / 16dp Material 3 dialog radius                    |
| **Dialog Elevation**        | `desktop/elevation/3`                            | `BubbleBorder::Shadow::DIALOG_SHADOW`      | Default `box-shadow` on `<dialog>`              | `@dimen/default_elevation_3` / `MaterialCardView` elevation                             |
| **Dialog Padding**          | `20px` (`--desktop/spacing/20`)                  | `INSETS_DIALOG` (typically 20px)           | `20px` (body), `20px` (title), `16px` (buttons) | `@dimen/dialog_padding_sides` / `@dimen/modal_dialog_control_horizontal_padding_filled` |
| **Title Font Family**       | `Google Sans`                                    | `TypographyProvider::Get().GetFont(...)`   | `--cr-dialog-font-family`                       | `@font/accent_font` / `sans-serif-medium`                                               |
| **Title Font Size**         | `16px` (`--desktop/font_size/headline-four`)     | Context: `CONTEXT_DIALOG_TITLE` (16px)     | `--cr-dialog-title-font-size` (16px)            | `@dimen/headline_size` (`22sp`)                                                         |
| **Title Line Height**       | `24px` (`--desktop/line_height/headline-four`)   | Standard label height (24px)               | `line-height: 24px`                             | `@dimen/headline_size_leading` (`28sp`)                                                 |
| **Illustration Height**     | `120px`                                          | Set dynamically on header image view       | Height: `120px` in slot `header`                | Configured in `PromoDialogLayout`                                                       |
| **Illustration Background** | `--desktop/sys/container-colors/tonal-container` | `ui::kColorSecondaryContainer`             | `--color-button-background-tonal`               | `?attr/colorSecondaryContainer`                                                         |
| **Button Spacing**          | `8px`                                            | `DISTANCE_RELATED_BUTTON_HORIZONTAL` (8px) | `margin-inline-end: 8px`                        | `@dimen/button_bar_stacked_margin` (`8dp`)                                              |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. WebUI Slot Order Gap

In WebUI, `<slot name="header">` within `<cr-dialog>` is structured below
`.top-container`. In the Figma design's `WITH ILLUSTRATION` variant, the
illustration is positioned above the title.

- **Impact**: WebUI implementations must use CSS `order` flexbox properties to
  visually elevate the header above the title.

### 2. Clank Modal Presenter Architecture

- In Clank, dialogs are managed via `ModalDialogManager` using either
  **`AppModalPresenter`** (blocking the whole application) or
  **`TabModalPresenter`** (scoped to an individual browser tab). UI state is
  driven strictly via `PropertyModel` rather than direct view mutation.

### 3. Corner Radius Discrepancies

- WebUI defaults to `8px` (requiring override to `12px`).
- Views uses `12px`.
- Clank follows Android Material 3 dialog guidelines using `16dp` or `24dp`
  corners.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Button Stacking Behavior

- **Desktop (Views & WebUI)**: Buttons remain horizontally aligned across all
  typical viewport dimensions.
- **Clank (Android)**:
  [`DualControlLayout`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DualControlLayout.java)
  automatically converts horizontal action pairs into vertically stacked buttons
  on narrow mobile viewports.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Variant Selection**:
  - Use **DEFAULT** for standard workflows, simple settings confirmations, or
    warning prompts.
  - Use **WITH ILLUSTRATION** / `PromoDialog` for prominent feature promotions
    and milestone completions.
- **Button Ordering**:
  - Primary action on the right / top.
  - Secondary / Cancel action to the left / bottom.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: Trap focus inside modal boundary, map `closeText` to aria-label,
  dismiss on `Escape`.
- **C++ Views**: Install `FocusRing`, set default button with
  `DialogDelegate::SetDefaultButton()`.
- **Clank (Android)**: Provide `ModalDialogProperties.TITLE_DESCRIPTION` for
  screen readers and support back button dismissals.
