# Component Spec: Key UIs / IPH

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **IPH (In Product Help)** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

The IPH (In Product Help) bubble is a prominent, contextual UI element used to educate users about features or changes. It attaches to an anchor view and supports combinations of a title, body text, an optional leading icon, progress dots for multi-step tutorials, and call-to-action buttons (a default primary button and secondary buttons).

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Key UIs / IPH` | `user_education::HelpBubbleView` | `<help-bubble>` |
| **Source Files** | [Figma Link: `323:15270`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15270) | [components/user_education/views/help_bubble_view.cc](//src/components/user_education/views/help_bubble_view.cc) | [ui/webui/resources/cr_components/help_bubble/help_bubble.ts](//src/ui/webui/resources/cr_components/help_bubble/help_bubble.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

Figma uses distinct layout variants to model combinations of features, whereas both C++ Views and WebUI use dynamic properties to conditionally render layout slots.

| Figma Variant | C++ `HelpBubbleParams` Configuration | WebUI `<help-bubble>` Property |
| :--- | :--- | :--- |
| **WITH ICON AND TITLE** | `body_icon` + `title_text` | `bodyIconName` + `titleText` |
| **WITH TITLE** | `title_text` (without `body_icon`) | `titleText` (without `bodyIconName`) |
| **WITHOUT TITLE** | Empty `title_text` | Empty `titleText` |
| **WITH PROGRESS DOTS** | `progress` (`std::pair<int, int>`) | `progress` object (`{current, total}`) |
| **Action Buttons** | `buttons` vector | `buttons` array |
| **Dismiss/Close** | Handled natively by bubble | `closeButtonAltText` (always present) |

---

## 3. Component States

Interactive states apply primarily to the embedded call-to-action buttons within the IPH bubble.

| Interactive State | Figma | C++ `user_education::MdIPHBubbleButton` | WebUI `<cr-button>` within IPH |
| :--- | :--- | :--- | :--- |
| **Default** | Solid or Outlined rendering | `ButtonState::STATE_NORMAL` | `.action-button` or `.cancel-button` |
| **Hovered** | N/A (Standard CDS hover) | `ButtonState::STATE_HOVERED` (InkDrop applied) | `:hover` pseudo-class / ripple |
| **Pressed** | N/A (Standard CDS press) | `ButtonState::STATE_PRESSED` | `:active` pseudo-class / ripple |
| **Focused** | N/A (Focus ring spec) | `views::FocusRing::Get(this)` | Focus ring via WebUI standard |
| **Disabled** | N/A | `ButtonState::STATE_DISABLED` | `[disabled]` attribute |

---

## 4. Design Token Comparison (Side-by-Side)

| Token Type | Figma Property | C++ `ui::ColorId` / Constant | WebUI Variable / CSS |
| :--- | :--- | :--- | :--- |
| **Background Color** | `--desktop/sys/primary-colors/primary` (#0b57d0) | `kColorFeaturePromoBubbleBackground` | `--color-feature-promo-bubble-background` |
| **Foreground / Text Color** | `--desktop/sys/primary-colors/on-primary` (white) | `kColorFeaturePromoBubbleForeground` | `--color-feature-promo-bubble-foreground` |
| **Default Button Bg** | `--desktop/sys/primary-colors/on-primary` (white) | `kColorFeaturePromoBubbleDefaultButtonBackground` | `--color-feature-promo-bubble-default-button-background` |
| **Default Button Text** | `--desktop/sys/primary-colors/primary` (#0b57d0) | `kColorFeaturePromoBubbleDefaultButtonForeground` | `--color-feature-promo-bubble-default-button-foreground` |
| **Corner Radius** | `12px` | Provided by `LayoutProvider` | `--help-bubble-border-radius: 12px` |
| **Inner Padding** | `20px` | `UseCompactMargins()` | `--help-bubble-padding: 20px` |
| **Element Spacing** | `8px` | Handled by `FlexLayout` gaps | `--help-bubble-element-spacing: 8px` |
| **Title Font Size** | `18px` (`desktop/font_size/headline-three`) | `ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE` | Native `h1` sizing |
| **Body Font Size** | `14px` (`desktop/font_size/body-two`) | `ChromeTextContext::CONTEXT_IPH_BUBBLE_BODY` | Native `p` sizing (`14px`) |

---

## 5. Architectural & Implementation Gaps

* **Component Decomposition vs. Monolith**: Figma models different layouts (with/without icon, with/without title) as separate components for ease of design. The Chromium implementation (both C++ and WebUI) uses a single flexible component that dynamically toggles child nodes.
* **Button Architecture**: In C++, `user_education::MdIPHBubbleButton` is a subclass of `views::MdTextButton` that overrides background painting logic to remove the default MD button border for prominent buttons and manually applies border strokes based on `HelpBubbleDelegate` colors. WebUI delegates standard `<cr-button>` styles.
* **Arrow Positioning**: WebUI uses a robust CSS injection (`--help-bubble-arrow-offset`) and rotated `div` injection for the pointer arrow, whereas C++ uses native `views::BubbleFrameView` and `views::BubbleBorder::Arrow` translations.

---

## 6. Styling, Variants, Features and States Mismatches

* **Typography Mapping**: The C++ layer abstracts font sizes through `ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE` and `ChromeTextContext::CONTEXT_IPH_BUBBLE_BODY`. Figma explicitly names the fonts `Google_Sans` and sizes them at 18px and 14px, which strictly relies on the Chromium text context correctly resolving those sizes globally.
* **Focus & Accessibility**: In C++, a prominent focus ring (`views::FocusRing`) is manually injected matching `GetHelpBubbleForegroundColorId()`. Figma does not explicitly depict this state.
* **Close Button Color**: C++ uses `GetHelpBubbleCloseButtonInkDropColorId()` for the `ClosePromoButton` state, managing a custom ink drop to blend against the prominent blue background.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Concise Content**: Keep titles short and directly actionable. Body text should explain feature value without overcrowding the layout.
- **Progress Tracking**: Always provide multi-step tutorials using the `progress` properties so users understand the length of the flow.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI a11y rules**: `<help-bubble>` acts as `role="alertdialog"` and uses `aria-live="assertive"`, `aria-modal="true"`, and ties labels using `aria-labelledby` and `aria-describedby`.
- **C++ Views native focus**: `HelpBubbleView` overrides `SetAccessibleWindowRole(ax::mojom::Role::kAlert)`. Navigation between panes is bound to `IDC_FOCUS_NEXT_PANE` accelerators.

### 3. Icon Usage Guidelines
- Leading icons should ideally utilize high-contrast white bounds (`#ffffff` on `#0b57d0`).
- The `Close` icon is strictly for dismissal and utilizes the generic system exit icon (`cancel` or `close`).

---

## 8. Inheritance Structure

* **C++ Views (Desktop)**:
  `views::View` → `views::BubbleDialogDelegateView` → `user_education::HelpBubbleView`
  (Internal button uses `views::MdTextButton` → `user_education::MdIPHBubbleButton`)

* **WebUI (Web Frontend)**:
  `HTMLElement` → `LitElement` → `HelpBubbleElement` (`<help-bubble>`)
