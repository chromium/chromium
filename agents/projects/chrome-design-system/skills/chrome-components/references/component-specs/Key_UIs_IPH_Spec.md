# Component Spec: Key UIs / IPH

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **IPH (In Product Help)** component
across Figma, C++ Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The IPH (In Product Help) bubble is a prominent, contextual UI element used to
educate users about features or changes. It attaches to an anchor view and
supports combinations of a title, body text, an optional leading icon, progress
dots for multi-step tutorials, and call-to-action buttons (a default primary
button and secondary buttons).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                     | C++ Views (Desktop)                                                                                              | WebUI (Desktop)                                                                                                                  | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Key UIs / IPH`                                                                                                                     | `user_education::HelpBubbleView`                                                                                 | `<help-bubble>`                                                                                                                  | `TextBubble` / `UserEducationHelper` / `IphDialogView`                                                                                                                                                                                                                                                                                                                                                                                             |
| **Source Files**   | [Figma Link: `323:15270`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15270) | [components/user_education/views/help_bubble_view.cc](//src/components/user_education/views/help_bubble_view.cc) | [ui/webui/resources/cr_components/help_bubble/help_bubble.ts](//src/ui/webui/resources/cr_components/help_bubble/help_bubble.ts) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java)<br>[chrome/android/java/src/org/chromium/chrome/browser/user_education/UserEducationHelper.java](//src/chrome/android/java/src/org/chromium/chrome/browser/user_education/UserEducationHelper.java) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Figma Variant           | C++ `HelpBubbleParams` Configuration | WebUI `<help-bubble>` Property         | Clank (Android) Implementation                    |
| :---------------------- | :----------------------------------- | :------------------------------------- | :------------------------------------------------ |
| **WITH ICON AND TITLE** | `body_icon` + `title_text`           | `bodyIconName` + `titleText`           | `TextBubble` with title and leading drawable      |
| **WITH TITLE**          | `title_text` (without `body_icon`)   | `titleText` (without `bodyIconName`)   | `TextBubble` with title                           |
| **WITHOUT TITLE**       | Empty `title_text`                   | Empty `titleText`                      | Standard body-only `TextBubble`                   |
| **WITH PROGRESS DOTS**  | `progress` (`std::pair<int, int>`)   | `progress` object (`{current, total}`) | Managed via multi-step IPH coordinator            |
| **Action Buttons**      | `buttons` vector                     | `buttons` array                        | Dismiss button or action button in bubble layout  |
| **Dismiss/Close**       | Handled natively by bubble           | `closeButtonAltText` (always present)  | Touch outside to dismiss or explicit close button |

______________________________________________________________________

## 3. Component States

| Interactive State | Figma                       | C++ `user_education::MdIPHBubbleButton` | WebUI `<cr-button>` within IPH       | Clank (Android)                  |
| :---------------- | :-------------------------- | :-------------------------------------- | :----------------------------------- | :------------------------------- |
| **Default**       | Solid or Outlined rendering | `ButtonState::STATE_NORMAL`             | `.action-button` or `.cancel-button` | Normal state                     |
| **Hovered**       | N/A (Standard CDS hover)    | `ButtonState::STATE_HOVERED`            | `:hover` pseudo-class / ripple       | State layer overlay              |
| **Pressed**       | N/A (Standard CDS press)    | `ButtonState::STATE_PRESSED`            | `:active` pseudo-class / ripple      | `?attr/selectableItemBackground` |
| **Focused**       | N/A (Focus ring spec)       | `views::FocusRing::Get(this)`           | Focus ring via WebUI standard        | Hardware focus outline           |
| **Disabled**      | N/A                         | `ButtonState::STATE_DISABLED`           | `[disabled]` attribute               | `android:enabled="false"`        |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

| Token Type                  | Figma Property                                    | C++ `ui::ColorId` / Constant                      | WebUI Variable / CSS                                     | Clank (Android) Equivalent                                                              |
| :-------------------------- | :------------------------------------------------ | :------------------------------------------------ | :------------------------------------------------------- | :-------------------------------------------------------------------------------------- |
| **Background Color**        | `--desktop/sys/primary-colors/primary` (#0b57d0)  | `kColorFeaturePromoBubbleBackground`              | `--color-feature-promo-bubble-background`                | `@macro/bubble_bg_color` / `?attr/colorSurfaceInverse` / `@macro/default_bg_color_dark` |
| **Foreground / Text Color** | `--desktop/sys/primary-colors/on-primary` (white) | `kColorFeaturePromoBubbleForeground`              | `--color-feature-promo-bubble-foreground`                | `@macro/bubble_text_color` / `?attr/colorOnSurfaceInverse`                              |
| **Default Button Bg**       | `--desktop/sys/primary-colors/on-primary` (white) | `kColorFeaturePromoBubbleDefaultButtonBackground` | `--color-feature-promo-bubble-default-button-background` | `?attr/colorPrimary`                                                                    |
| **Default Button Text**     | `--desktop/sys/primary-colors/primary` (#0b57d0)  | `kColorFeaturePromoBubbleDefaultButtonForeground` | `--color-feature-promo-bubble-default-button-foreground` | `@macro/default_text_color_on_accent1`                                                  |
| **Corner Radius**           | `12px`                                            | Provided by `LayoutProvider`                      | `--help-bubble-border-radius: 12px`                      | `@dimen/text_bubble_corner_radius` (`12dp`)                                             |
| **Inner Padding**           | `20px`                                            | `UseCompactMargins()`                             | `--help-bubble-padding: 20px`                            | `@dimen/text_bubble_padding_horizontal` (`16dp`-`20dp`)                                 |
| **Element Spacing**         | `8px`                                             | Handled by `FlexLayout` gaps                      | `--help-bubble-element-spacing: 8px`                     | `8dp`                                                                                   |
| **Title Font Size**         | `18px`                                            | `ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE`     | Native `h1` sizing                                       | `@dimen/headline_size` (`18sp`)                                                         |
| **Body Font Size**          | `14px`                                            | `ChromeTextContext::CONTEXT_IPH_BUBBLE_BODY`      | Native `p` sizing (`14px`)                               | `@style/TextAppearance.TextMedium.Inverse` (`14sp`)                                     |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

- **Component Decomposition vs. Monolith**: Figma models different layouts as
  separate components. Views, WebUI, and Clank use dynamic properties to
  conditionally render layout slots.
- **Android IPH Architecture**: In Clank,
  [`UserEducationHelper`](//src/chrome/android/java/src/org/chromium/chrome/browser/user_education/UserEducationHelper.java)
  queries the Feature Engagement Tracker backend (`TrackerFactory`) before
  displaying an anchored
  [`TextBubble`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java).

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

- **Focus & Accessibility**: In C++, a prominent focus ring (`views::FocusRing`)
  is manually injected. Clank ensures TalkBack announces the message upon anchor
  visibility.

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Concise Content**: Keep titles short and directly actionable.
- **Progress Tracking**: Always provide multi-step tutorials using progress
  properties so users understand flow length.

### 2. Platform Consistency & Accessibility (a11y)

- **WebUI**: `<help-bubble>` acts as `role="alertdialog"` and uses
  `aria-live="assertive"`.
- **C++ Views**: `HelpBubbleView` overrides
  `SetAccessibleWindowRole(ax::mojom::Role::kAlert)`.
- **Clank (Android)**: `TextBubble` accessibility focus is announced immediately
  upon show.
