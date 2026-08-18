# Component Spec: Settings - Tooltip

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Tooltip** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

A **Tooltip** is a small, brief pop-up bubble that displays helpful,
supplementary textual information when a user hovers over, focuses, or
long-presses an anchored UI element. It contains static text only, handles no
active inputs itself, and disappears automatically after a delay or upon mouse
exit/blur of the parent element.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                             | C++ Views (Desktop)                                                              | WebUI (Desktop)                                                                                                          | Clank (Android)                                                                                                                                                                                                                                |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Tooltip`                                                                                                                                                   | `views::corewm::TooltipViewAura`                                                 | `<cr-tooltip>`                                                                                                           | `TextBubble` / `TooltipCompat`                                                                                                                                                                                                                 |
| **Source Files**   | [Figma Link: `60352:8418`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=60352-8418&t=FMUhNTpxly8KQpO3-11) | [ui/views/corewm/tooltip_view_aura.h](//src/ui/views/corewm/tooltip_view_aura.h) | [ui/webui/resources/cr_elements/cr_tooltip/cr_tooltip.ts](//src/ui/webui/resources/cr_elements/cr_tooltip/cr_tooltip.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant   | Figma                                           | C++ Views (Desktop)                                                        | WebUI (Web Frontend)                                     | Clank (Android)                                                              |
| :------------------ | :---------------------------------------------- | :------------------------------------------------------------------------- | :------------------------------------------------------- | :--------------------------------------------------------------------------- |
| **Standard Bubble** | Default appearance with dark transparent scrim. | Renders as native widget `views::corewm::TooltipViewAura` with 1px border. | `<cr-tooltip>` wrapping an internal `#tooltip` div.      | `TextBubble` anchored via `AnchoredPopupWindow` with optional arrow pointer. |
| **Text Wrapping**   | Word-wrapped based on max-width constraint.     | Wrapping via `gfx::RenderText` (`SetMultiline(true)`).                     | Block-level text flow and CSS width constraints.         | Managed via `TextView` wrapping with `app:bubbleMaxWidth`.                   |
| **Positioning**     | Top, bottom, left, right                        | `TooltipAura::GetTooltipBounds` relative to cursor.                        | `position` attribute (`top`, `bottom`, `left`, `right`). | `AnchoredPopupWindow` positioned above/below anchor view.                    |

______________________________________________________________________

## 3. Component States

| State                    | Figma State Property | C++ views::View / Widget              | WebUI CSS classes / pseudo-classes  | Clank (Android)                          |
| :----------------------- | :------------------- | :------------------------------------ | :---------------------------------- | :--------------------------------------- |
| **Hidden**               | Not visible          | Widget is hidden/destroyed            | `hidden` attribute is set           | `TextBubble.dismiss()` / popup dismissed |
| **Showing (Transition)** | N/A                  | Instantly visible                     | `.fade-in-animation` class applied  | Android alpha / popup fade-in animator   |
| **Visible**              | Default state        | Widget is visible (`widget_->Show()`) | Fully visible (opacity `0.9`)       | `TextBubble.show()`                      |
| **Hiding (Transition)**  | N/A                  | Instantly hidden / Widget destroyed   | `.fade-out-animation` class applied | Android popup fade-out animator          |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Property / Token     | Figma Design                                                                         | C++ Views (Desktop)                     | WebUI (Web Frontend)                       | Clank (Android) Equivalent                                                                       |
| :------------------- | :----------------------------------------------------------------------------------- | :-------------------------------------- | :----------------------------------------- | :----------------------------------------------------------------------------------------------- |
| **Background Color** | `var(--desktop/sys/state-colors/state-scrim)` (`rgba(0,0,0,0.6)`)                    | `ui::kColorTooltipBackground`           | `var(--paper-tooltip-background, #616161)` | `@macro/bubble_bg_color` / `?attr/colorSurfaceInverse` / `@macro/default_bg_color_dark`          |
| **Text Color**       | `var(--desktop/sys/static-colors/white)` (`#ffffff`)                                 | `ui::kColorTooltipForeground`           | `var(--paper-tooltip-text-color, white)`   | `@macro/bubble_text_color` / `?attr/colorOnSurfaceInverse`                                       |
| **Border / Stroke**  | None                                                                                 | 1px Solid `ui::kColorTooltipForeground` | None                                       | None (elevation shadow)                                                                          |
| **Corner Radius**    | `2px`                                                                                | `0px` (`6px` on Ash)                    | `2px` (`border-radius: 2px`)               | `@dimen/text_bubble_corner_radius` (`8dp` / `12dp`)                                              |
| **Padding**          | Top/Bottom: `10px`<br>Left/Right: `8px`                                              | `TLBR(4, 8, 5, 8)` insets               | `8px` (all sides)                          | `@dimen/text_bubble_padding_horizontal` (`16dp`), `@dimen/text_bubble_padding_vertical` (`12dp`) |
| **Typography**       | Font Family: `typeface/body`<br>Weight: `500`<br>Size: `13px`<br>Line Height: `20px` | System default (`11-12px`)              | System default (`10px`)                    | `@style/TextAppearance.TextMedium.Inverse` (`14sp`)                                              |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

1. **Touch vs. Cursor Trigger Model**:
   - On Desktop (Views and WebUI), tooltips are transient hover overlays
     triggered by mouse hover.
   - On Android (Clank), hover is rarely available;
     [`TextBubble`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java)
     is used as an explicit user education / contextual pointer bubble dismissed
     by outside taps or timers, while
     [`TooltipCompat`](https://developer.android.com/reference/androidx/appcompat/widget/TooltipCompat)
     serves long-press actions on icon buttons.
2. **Arrow Pointers**:
   - Figma and desktop Views do not include arrow nibs for standard tooltips.
   - Clank's `TextBubble` draws an anchored directional arrow pointing towards
     the anchor view.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Display Delays & Animations**:
  - WebUI `<cr-tooltip>` has a built-in fade-in animation delay of `500ms`.
  - C++ Views shows the widget after `TooltipController` timeout without fade.
  - Clank's `TextBubble` provides programmatic dismiss duration controls
    (`setDuration()`, auto-dismiss on scroll).

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Supplementary Info Only**: Tooltips should only provide auxiliary context,
  never critical navigation or error text.
- **Brief Text**: Keep text concise (1-2 short sentences max).

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: `<cr-tooltip>` is announced as `role="tooltip"`.
- **C++ Views**: `views::corewm::TooltipViewAura` sets
  `ax::mojom::Role::kTooltip`.
- **Clank (Android)**: `TextBubble` announces contents to TalkBack upon show.
