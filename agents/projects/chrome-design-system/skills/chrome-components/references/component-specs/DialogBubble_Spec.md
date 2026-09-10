# Component Spec: Dialog Bubble

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Dialog Bubble** component (referred to
as `Bubble Template` or `Key UIs / Dialog Bubble` in Figma) across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Dialog Bubble** is an anchored, non-modal or modal floating container used
in Chromium desktop to present context-sensitive information, interactive
subpages, confirmation prompts, or rich promotional banners. Unlike centered
modal dialogs that block the entire window or web contents, a dialog bubble is
anchored directly to a UI element—such as a toolbar button, an Omnibox page
action icon, or a screen coordinate—and typically dismisses when the user clicks
outside.

In the Chrome Design System (CDS), the **Dialog Bubble** is specified across
three fixed responsive widths and three primary layout configurations:

1. **STANDARD (DEFAULT) LAYOUT**: Features a header with a bubble title and an
   optional trailing close button, a flexible content area, and a bottom button
   bar supporting an outlined tertiary button on the left and tonal/primary
   action buttons on the right.
2. **NAVIGABLE / SUBTITLED LAYOUT (WITH BACK ARROW & DIVIDER)**: Adds a leading
   back navigation button (`arrow_back`), a subtitle/subhead below the title,
   and a full-width horizontal 1px separator line (divider) separating the
   header from the content area.
3. **WITH ILLUSTRATION LAYOUT (PROMO / BANNER)**: Adds a 120px tall full-bleed
   illustration banner at the very top of the bubble frame with the close button
   overlaid in its top-right corner, followed by the bubble title, content area,
   and button bar.

### Width Sizes

- **Small (320dp)**: Standard compact bubble (`kSmallDialogWidth = 320px` in
  Views).
- **Medium (448dp)**: Medium bubble for structured content or lists
  (`kMediumDialogWidth = 448px` in Views).
- **Large (512dp)**: Wide bubble for complex wizards or rich previews
  (`kLargeDialogWidth = 512px` in Views).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                         | C++ Views (Desktop)                                                                                                                                                                                                                                                                                                                                                    | WebUI (Desktop) | Clank (Android) |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------- | :-------------- |
| **Component Name** | `Key UIs / Dialog Bubble`                                                                                                                               | `views::BubbleDialogDelegateView`<br>`views::BubbleDialogModelHost`<br>`views::BubbleFrameView`<br>`views::BubbleBorder`                                                                                                                                                                                                                                               | **N/A**         | **N/A**         |
| **Source Files**   | [Figma Link: `18300:625`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=18300-625&t=H2HK5dc8ofgXglfl-11) | [ui/views/bubble/bubble_dialog_delegate_view.h](//src/ui/views/bubble/bubble_dialog_delegate_view.h)<br>[ui/views/bubble/bubble_dialog_model_host.h](//src/ui/views/bubble/bubble_dialog_model_host.h)<br>[ui/views/bubble/bubble_frame_view.h](//src/ui/views/bubble/bubble_frame_view.h)<br>[ui/views/bubble/bubble_border.h](//src/ui/views/bubble/bubble_border.h) | **N/A**         | **N/A**         |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant                | Figma Component                                                                                         | C++ Views (Desktop) Implementation                                                                                                                                                                                        |
| :------------------------------- | :------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Standard (Default) Layout**    | Standard bubble frame with title, close button, content, and button bar                                 | `views::BubbleDialogDelegateView` with `SetTitle()` and `BubbleFrameView::CreateCloseButton()`.                                                                                                                           |
| **Navigable / Subtitled Layout** | Bubble frame with back navigation button, title, subhead, close button, and horizontal divider          | Custom header view via `BubbleFrameView::SetTitleView()` or `chrome/browser/ui/views/controls/subpage_view.h`, pairing `vector_icons::kArrowBackIcon`, `BubbleDialogDelegateView::SetSubtitle()`, and `views::Separator`. |
| **With Illustration Layout**     | Bubble frame with 120px tall illustration header, overlaid close button, title, content, and button bar | Implemented via `BubbleFrameView::SetHeaderView()` or `ui::DialogModel::Builder::SetBannerImage()`. Close button is overlaid over the header view at top-right.                                                           |
| **Small Size (320dp)**           | `Subheader: SMALL (320DP)`, width: `320px`                                                              | `views::LayoutProvider::kSmallDialogWidth` (320px) / `views::DISTANCE_BUBBLE_PREFERRED_WIDTH`. Snapped via `ChromeLayoutProvider::GetSnappedDialogWidth()`.                                                               |
| **Medium Size (448dp)**          | `Subheader: MEDIUM (448DP)`, width: `448px`                                                             | `views::LayoutProvider::kMediumDialogWidth` (448px). Snapped via `ChromeLayoutProvider::GetSnappedDialogWidth()`.                                                                                                         |
| **Large Size (512dp)**           | `Subheader: LARGE (512DP)`, width: `512px`                                                              | `views::LayoutProvider::kLargeDialogWidth` (512px). Snapped via `ChromeLayoutProvider::GetSnappedDialogWidth()`.                                                                                                          |
| **Title Slot**                   | Bubble title (`desktop/headline/four`, 16px medium)                                                     | `views::Label` configured with `views::style::CONTEXT_DIALOG_TITLE` and `views::style::STYLE_PRIMARY`. Set via `DialogDelegate::SetTitle()` or `DialogModel::Builder::SetTitle()`.                                        |
| **Subhead Slot**                 | "Subhead if necessary" (`desktop/body/four`, 12px regular)                                              | Configured with `views::style::CONTEXT_DIALOG_BODY_TEXT_SMALL` and `views::style::STYLE_SECONDARY`. Set via `BubbleDialogDelegate::SetSubtitle()` or `DialogModel::Builder::SetSubtitle()`.                               |
| **Back Arrow Navigation**        | `Icon Buttons` -> `arrow_back` (20px icon)                                                              | `views::CreateVectorImageButtonWithNativeTheme` with `vector_icons::kArrowBackIcon` (20px) and `views::InstallCircleHighlightPathGenerator`.                                                                              |
| **Close Button**                 | `Icon Buttons` -> `close` (20px icon)                                                                   | `views::BubbleFrameView::CreateCloseButton()`. Positioned at the trailing end of the title row, or overlaid in top-right corner of the illustration header.                                                               |
| **Divider Line**                 | 1px horizontal line below header                                                                        | `views::Separator` using `ui::kColorSeparator`.                                                                                                                                                                           |
| **Content Area Slot**            | 128px content placeholder (`neutral-container`)                                                         | Set via `BubbleDialogDelegateView::SetContentsView()` or `DialogModel::AddCustomField()`.                                                                                                                                 |
| **Left Button (Tertiary)**       | Outlined Button (`Variant=Outlined`)                                                                    | Outlined `views::MdTextButton` added via `DialogDelegate::SetExtraView()` or `DialogModel::AddExtraButton()`.                                                                                                             |
| **Right Secondary Button**       | Tonal Button (`Variant=Tonal`)                                                                          | Cancel button: `views::MdTextButton` styled with `ui::ButtonStyle::kTonal`.                                                                                                                                               |
| **Right Primary Button**         | Primary Button (`Variant=Primary`)                                                                      | Accept/OK button: `views::MdTextButton` styled with `ui::ButtonStyle::kProminent`.                                                                                                                                        |
| **Anchor & Arrow**               | Border frame, elevation shadow                                                                          | `views::BubbleBorder::Arrow` (e.g. `TOP_LEFT`, `TOP_RIGHT`, `BOTTOM_LEFT`, or `FLOAT` / `NONE` for unanchored bubbles). Set via `BubbleDialogDelegateView::SetArrow()`.                                                   |

______________________________________________________________________

## 3. Component States

| State                       | Figma Component / Property | C++ Views (Desktop) Implementation                                                                                                                     |
| :-------------------------- | :------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Default (Normal)**        | `State=Default`            | Standard normal state for bubble widget, frame, and buttons (`views::Button::ButtonState::STATE_NORMAL`).                                              |
| **Hovered**                 | `State=Hovered`            | Action buttons and icon buttons show hover state layer via `views::InkDrop` (`views::Button::ButtonState::STATE_HOVERED`).                             |
| **Pressed (Pushed)**        | `State=Pressed`            | Action buttons and icon buttons show active ripple via `views::InkDrop` (`views::Button::ButtonState::STATE_PRESSED`).                                 |
| **Disabled**                | `State=Disabled`           | Individual buttons disabled via `views::View::SetEnabled(false)` (`views::Button::ButtonState::STATE_DISABLED`).                                       |
| **Focused**                 | `State=Focused`            | Keyboard navigation renders `views::FocusRing` around the focused button, close icon, or interactive element using `ui::kColorFocusableBorderFocused`. |
| **Deactivated / Dismissed** | Outside click              | Managed by `views::BubbleDialogDelegate::set_close_on_deactivate(true)`. Automatically dismisses the bubble widget when focus leaves the window.       |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Design Attribute                    | Figma Design Token                                                                                             | C++ Views (Desktop) Token / Value                                                                 |
| :---------------------------------- | :------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------ |
| **Bubble Container Background**     | `--desktop/sys/surface-colors/surface` (#ffffff)                                                               | `ui::kColorBubbleBackground`                                                                      |
| **Bubble Corner Radius**            | `12px` (`--desktop/corner-radius/12`)                                                                          | `views::ShapeContextTokens::kDialogRadius` -> `views::ShapeSysTokens::kMediumSmall` (12px)        |
| **Bubble Elevation**                | `desktop/elevation/3`<br>Key: `0px 4px 8px 3px rgba(0,0,0,0.15)`<br>Ambient: `0px 1px 3px 0px rgba(0,0,0,0.3)` | `views::BubbleBorder::Shadow::STANDARD_SHADOW` or `views::BubbleBorder::Shadow::DIALOG_SHADOW`    |
| **Header Top & Side Insets**        | `20px` (`--desktop/spacing/20`)                                                                                | `views::LayoutProvider::GetInsetsMetric(INSETS_DIALOG_TITLE)` (20px top, 20px horizontal)         |
| **Header Bottom Spacing**           | `8px` (`--desktop/spacing/8`)                                                                                  | `views::LayoutProvider::GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL)` (8px)               |
| **Content Area Side Insets**        | `20px` (`--desktop/spacing/20`)                                                                                | `views::LayoutProvider::GetInsetsMetric(INSETS_DIALOG)` (20px horizontal)                         |
| **Content Area Bottom Insets**      | `8px` (`--desktop/spacing/8`)                                                                                  | `8px` padding above button bar                                                                    |
| **Button Bar Insets**               | Top: `8px`, Horizontal: `20px`, Bottom: `20px`                                                                 | `views::LayoutProvider::GetInsetsMetric(INSETS_DIALOG_BUTTON_ROW)` (20px horizontal, 20px bottom) |
| **Button Horizontal Spacing**       | `8px` (`--desktop/spacing/8`)                                                                                  | `views::LayoutProvider::GetDistanceMetric(DISTANCE_RELATED_BUTTON_HORIZONTAL)` (8px)              |
| **Title Font Family**               | `Google Sans` (`--desktop/font/headline`)                                                                      | `views::TypographyProvider::Get().GetFont(views::style::CONTEXT_DIALOG_TITLE, ...)`               |
| **Title Font Size**                 | `16px` (`--desktop/font_size/headline-four`)                                                                   | `views::style::CONTEXT_DIALOG_TITLE` (16px)                                                       |
| **Title Line Height**               | `24px` (`--desktop/line_height/headline-four`)                                                                 | `24px` line height                                                                                |
| **Title Font Weight**               | Medium (500) (`--desktop/font_weight/medium`)                                                                  | `gfx::Font::Weight::MEDIUM`                                                                       |
| **Title Color**                     | `--desktop/sys/surface-colors/on-surface` (#1f1f1f)                                                            | `ui::kColorDialogForeground` / `ui::kColorLabelForeground`                                        |
| **Subhead Font Family**             | `Google Sans Text` (`--desktop/font/body`)                                                                     | `views::TypographyProvider::Get().GetFont(views::style::CONTEXT_DIALOG_BODY_TEXT_SMALL, ...)`     |
| **Subhead Font Size**               | `12px` (`--desktop/font_size/body-four`)                                                                       | `views::style::CONTEXT_DIALOG_BODY_TEXT_SMALL` (12px)                                             |
| **Subhead Line Height**             | `18px` (`--desktop/line_height/body-four`)                                                                     | `18px` line height                                                                                |
| **Subhead Font Weight**             | Regular (400) (`--desktop/font_weight/regular`)                                                                | `gfx::Font::Weight::NORMAL`                                                                       |
| **Subhead Color**                   | `--desktop/sys/surface-colors/on-surface-subtle` (#474747)                                                     | `ui::kColorLabelForegroundSecondary`                                                              |
| **Header Icon Size (Back / Close)** | `20px` x `20px`                                                                                                | `views::LayoutProvider::GetDistanceMetric(DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE)` (20px)        |
| **Header Icon Color**               | `--desktop/sys/surface-colors/on-surface-subtle` (#474747)                                                     | `ui::kColorIconSecondary`                                                                         |
| **Illustration Banner Height**      | `120px`                                                                                                        | `120px` (preferred height for bubble banner image)                                                |
| **Illustration Banner Background**  | `--desktop/sys/container-colors/tonal-container` (#d3e3fd)                                                     | `ui::kColorSecondaryContainer` / `ui::kColorSysTonalContainer`                                    |
| **Divider Line**                    | `1px` solid line                                                                                               | `ui::kColorSeparator` (rendered via `views::Separator`)                                           |
| **Neutral Container (Content BG)**  | `--desktop/sys/container-colors/neutral-container` (#f2f2f2)                                                   | `ui::kColorNeutralContainer` / `ui::kColorBubbleFooterBackground`                                 |
| **Primary Button Container**        | `--desktop/sys/primary-colors/primary` (#0b57d0)                                                               | `ui::kColorButtonBackgroundProminent`                                                             |
| **Primary Button Foreground**       | `--desktop/sys/primary-colors/on-primary` (#ffffff)                                                            | `ui::kColorButtonForegroundProminent`                                                             |
| **Tonal Button Container**          | `--desktop/sys/container-colors/tonal-container` (#d3e3fd)                                                     | `ui::kColorButtonBackgroundTonal`                                                                 |
| **Tonal Button Foreground**         | `--desktop/sys/container-colors/on-tonal-container` (#041e49)                                                  | `ui::kColorButtonForegroundTonal`                                                                 |
| **Outlined Button Border**          | `--desktop/sys/outline-colors/tonal-outline` (#a8c7fa)                                                         | `ui::kColorButtonBorder`                                                                          |
| **Outlined Button Foreground**      | `--desktop/sys/primary-colors/primary` (#0b57d0)                                                               | `ui::kColorButtonForeground`                                                                      |
| **Button Corner Radius**            | `999px` / Fully rounded                                                                                        | `views::ShapeContextTokens::kButtonRadius` -> `views::ShapeSysTokens::kFull` (pill shape)         |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Platform Support Boundary

- **Desktop Only**: The `Key UIs / Dialog Bubble` is strictly a Desktop Views
  component pattern. In the CDS kit, it is explicitly badged as `VIEWS`
  (`task_alt`) and `WEBUI` (`cancel`).
- **WebUI Integration**: WebUI applications inside Chrome do not have an
  anchored `<cr-bubble>` custom element. When WebUI content needs to be rendered
  within an anchored bubble (such as the Extensions menu, Tab Search, or Side
  Panel drop-downs), Chromium wraps the WebUI page inside a C++ Views
  `WebUIBubbleDialogView` / `WebUIBubbleManager`.
- **Clank (Android)**: Mobile Android does not support floating anchored dialog
  bubbles. Instead, mobile flows use bottom sheets (`BottomSheetController`),
  full/tab modal dialogs (`ModalDialogView`), or small floating help tips
  (`TextBubble`).

### 2. Dialog Width Snapping Logic

- In C++ Views, bubble and dialog widths are enforced programmatically by
  `ChromeLayoutProvider::GetSnappedDialogWidth(int min_width)`:
  - If `min_width <= 320`, returns `kSmallDialogWidth` (320px).
  - If `min_width <= 448`, returns `kMediumDialogWidth` (448px).
  - If `min_width <= 512`, returns `kLargeDialogWidth` (512px).
  - If `min_width > 512`, snaps to the nearest multiple of 16px.
- This directly aligns with the three width headers in the Figma spec:
  `SMALL (320DP)`, `MEDIUM (448DP)`, and `LARGE (512DP)`.

### 3. Bubble Arrow Visibility

- Figma depicts all dialog bubbles as standalone rounded cards without pointing
  arrow carousels.
- In Views, `BubbleBorder` supports directional pointing arrows
  (`views::BubbleBorder::Arrow`) anchored to the source view. Modern Chrome
  bubbles predominantly set `SetDisplayVisibleArrow(false)` or
  `BubbleBorder::Arrow::FLOAT` to achieve the floating elevation look shown in
  Figma, relying on proximity to the anchor view rather than a caret.

### 4. Close Button Stacking with Illustration Banner

- In the `WITH ILLUSTRATION` layout variant, the close button floats over the
  top-right corner of the 120px illustration banner.
- In Views, `BubbleFrameView::SetHeaderView()` explicitly supports this layout:
  the header view is placed at index 0, and the close button is positioned at
  the top-right corner with a higher z-order to ensure it receives hit events
  and paints above the illustration image.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Navigation Header & Subpage Patterns

- The Figma component includes an `arrow_back` icon on the left, a title and
  subhead in the center, and a `close` icon on the right, followed by a 1px
  separator line.
- In Views, standard `views::BubbleDialogDelegateView` does not have a built-in
  `SetLeadingBackButton()` method on `BubbleFrameView`. Instead, multi-step
  subpage navigation in bubbles is implemented using `SubpageView`
  (`chrome/browser/ui/views/controls/subpage_view.h`) or by creating a custom
  title view via `BubbleFrameView::SetTitleView()`, embedding an `ImageButton`
  with `kArrowBackIcon` and installing `InstallCircleHighlightPathGenerator`.

### 2. Tri-Button Bar Layout

- The Figma design shows three buttons:
  - **Left (LHS)**: Outlined button (e.g. secondary action, "Learn more", or
    settings).
  - **Right (RHS)**: Tonal button (Cancel / Dismiss) and Primary button (Confirm
    / Action).
- Standard `views::DialogDelegate` natively supports only two primary dialog
  buttons (`DialogButton::kOk` and `DialogButton::kCancel`). To achieve the
  third left-aligned button, developers must call
  `DialogDelegate::SetExtraView()` or use `DialogModel::AddExtraButton()`.

### 3. Icon Touch Targets vs. Figma Dimensions

- In Figma, the close and back icon buttons are specified as `20x20px`.
- In Chromium Views, minimum interactive target guidelines require at least
  `24x24px` to `28x28px` hit areas. `BubbleFrameView::CreateCloseButton()` adds
  internal padding around the 20px vector icon to satisfy target accessibility
  standards.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Anchor Context**: Always anchor dialog bubbles to the triggering control
  (e.g., toolbar action button, Omnibox page action chip, or contextual menu
  item).
- **Dismissal on Deactivation**: By default, bubbles should close when the user
  clicks outside or focuses another application
  (`set_close_on_deactivate(true)`). Only modal or persistent setup flows should
  disable auto-dismissal.
- **Variant Selection**:
  - Use the **Standard (Default)** variant for simple notifications, quick
    confirmations, or standalone single-topic interactions.
  - Use the **Navigable / Subtitled** variant when the bubble represents a
    drill-down subpage (e.g., Page Info subpage, Extensions details) or requires
    an explanatory subhead.
  - Use the **With Illustration** variant for feature onboarding, first-run
    experiences (FRE), promo dialogs, or key product updates.
- **Width Guidelines**:
  - **Small (320dp)**: Default for simple alerts, confirmation prompts, or
    compact lists.
  - **Medium (448dp)**: Use for subpage navigation, permission prompts, or
    forms.
  - **Large (512dp)**: Reserved for rich multi-step setup flows or complex
    graphical illustrations.
- **Action Hierarchy**:
  - Rightmost button should always be the primary affirmative action (Prominent
    button).
  - Negative/cancellation button should use the Tonal style directly to the left
    of the primary action.
  - Tertiary or auxiliary actions (such as "Learn more" or settings links)
    belong on the left side of the button bar using the Outlined button style.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Initial Focus**: Call `SetInitiallyFocusedView()` to direct keyboard focus
  to the most relevant input control or to the primary action button.
- **Escape Key Dismissal**: Pressing `Escape` must dismiss the bubble
  immediately (`views::DialogDelegate` default).
- **Tab Navigation**: Tab traversal must cycle cleanly through:
  1. Leading back button (if present).
  2. Content interactive elements (links, inputs, checkboxes).
  3. Trailing close button.
  4. Button bar (LHS extra button -> RHS tonal button -> RHS primary button).
- **Accessible Names & Roles**:
  - Assign `ax::mojom::Role::kDialog` to the bubble widget.
  - Title and subtitle must be wired to `ax::mojom::StringAttribute::kName` and
    `kDescription` so assistive technologies announce context upon opening.
- **Contrast Ratios**: All text tokens
  (`--desktop/sys/surface-colors/on-surface` and
  `--desktop/sys/surface-colors/on-surface-subtle`) provide WCAG AA 4.5:1
  contrast against the surface background.

### 3. Icon Usage Guidelines

- **Back Navigation**: Use `vector_icons::kArrowBackIcon` with a circular ink
  drop highlight (`views::InstallCircleHighlightPathGenerator`).
- **Dismissal / Close**: Use `BubbleFrameView::CreateCloseButton()` which
  renders `vector_icons::kCloseIcon` styled with `ui::kColorIconSecondary`.
- **Header Icon Dimensions**: Vector icons in bubble headers should measure
  `20x20px` (`DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE`).
