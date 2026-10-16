# IPH Spec (Desktop)

This document maps the specifications of the **IPH** (In-Product Help) (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

The **IPH** (In-Product Help, a.k.a. help bubble / feature promo bubble) is an
anchored, `primary`-colored callout with a pointer arrow that teaches users
about a feature. It supports an optional leading icon, an optional title, body
text, an optional progress-dot row (for multi-step tutorials), a close (`X`)
button, and up to two pill buttons (outlined secondary + filled default),
implemented in C++ Views by `user_education::HelpBubbleView` and in WebUI by
`<help-bubble>`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | C++ Views (Desktop)                                                                                            | WebUI (Desktop)                                                                                                                  |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `IPH` (frame-based; no `COMPONENT`/`COMPONENT_SET` — 4 example frames inside the `Key UIs / IPH` documentation frame)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | `user_education::HelpBubbleView`                                                                               | `<help-bubble>`                                                                                                                  |
| **Source Files**   | [Figma Link: `IPH` — With icon and title (`323:15281`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15281)<br>[Figma Link: `IPH` — With title (`323:15307`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15307)<br>[Figma Link: `IPH` — Without title (`323:15333`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15333)<br>[Figma Link: `IPH` — With progress dots (`323:15359`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15359)<br>(Parent doc frame: [`Key UIs / IPH` (`323:15270`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-15270)) | [components/user_education/views/help_bubble_view.h](//src/components/user_education/views/help_bubble_view.h) | [ui/webui/resources/cr_components/help_bubble/help_bubble.ts](//src/ui/webui/resources/cr_components/help_bubble/help_bubble.ts) |

> [!NOTE] The Figma doc header marks **VIEWS** as supported and **WEBUI** as not
> supported (`cancel` chip). However, a WebUI implementation (`<help-bubble>`,
> used by `HelpBubbleMixin` and `components/user_education/webui/`) exists and
> uses the same `--color-feature-promo-bubble-*` tokens, so it is included here
> for completeness.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: *None.* `IPH` is built from plain frames and has no Figma
variant properties. The documentation frame shows four configurations:

- `IPH` (frame) — `WITH ICON AND TITLE` (icon + title + body + close + 2
  buttons) | `WITH TITLE` (title + body + close + 2 buttons) | `WITHOUT TITLE`
  (body + close + 2 buttons) | `WITH PROGRESS DOTS` (dots + body + close, no
  buttons).
- Nested instances: `IPH Content` (`LHS` / `Dots` slots), `Icon Buttons`
  (close), `Buttons` ×2 (outlined + filled).

| Feature / Variant               | Figma Component                                    | C++ Views (Desktop)                                                                                                                                                                                    | WebUI (Desktop)                                                                                           |
| :------------------------------ | :------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------- |
| **Leading body icon**           | `WITH ICON AND TITLE` (`lightbulb` in 24px circle) | `HelpBubbleParams::body_icon` (+ `body_icon_alt_text`) → `icon_view_` (`views::ImageView` with `CreatePillBackground`)                                                                                 | `bodyIconName` / `bodyIconAltText` → `#bodyIcon`                                                          |
| **Title**                       | `WITH ICON AND TITLE`, `WITH TITLE`                | `HelpBubbleParams::title_text` (body label moves to its own row below)                                                                                                                                 | `titleText` → `#title` (body rendered in `#mainBody`)                                                     |
| **No title**                    | `WITHOUT TITLE`, `WITH PROGRESS DOTS`              | `title_text` empty → body label placed in the top row next to the close button                                                                                                                         | `titleText` empty → body rendered in `#topBody`                                                           |
| **Progress dots**               | `WITH PROGRESS DOTS` (`Dots` row)                  | `HelpBubbleParams::progress` (`{current, total}`) → `DotView` ×`total` (8×8, filled for `i < current`); close button moves into the progress row                                                       | `progress` (`{current, total}`) → `#progress` (`role="progressbar"`)                                      |
| **Close button**                | `Icon Buttons` (`close`), always present           | `ClosePromoButton` (`views::ImageButton`), always added; a11y name from `close_button_alt_text` (default `IDS_CLOSE`)                                                                                  | `<cr-icon-button>` with `closeButtonAltText`                                                              |
| **Secondary (outlined) button** | `Buttons` (outlined, `on-primary` border)          | `HelpBubbleButtonParams{is_default=false}` → `MdIPHBubbleButton` (bg = bubble bg, 1px border = `kColorFeaturePromoBubbleButtonBorder`)                                                                 | `buttons[i]` (non-default) → `<cr-button>` with `--cr-button-border-color: var(--help-bubble-foreground)` |
| **Default (filled) button**     | `Buttons` (filled `on-primary`, `primary` label)   | `HelpBubbleButtonParams{is_default=true}` → `MdIPHBubbleButton` (bg = `kColorFeaturePromoBubbleDefaultButtonBackground`); placed trailing (leading on Windows via `PlatformStyle::kIsOkButtonLeading`) | `buttons[i].isDefault` → `<cr-button class="default-button">`                                             |
| **No buttons**                  | `WITH PROGRESS DOTS`                               | `params.buttons` empty → `button_container` hidden; default auto-dismiss timeout `kDefaultTimeoutWithoutButtons` (10s)                                                                                 | `buttons` empty → `#buttons` hidden; timeout handled by `HelpBubbleHandler` (10s)                         |
| **Arrow / pointer**             | `Pointer` (top-left arrow)                         | `HelpBubbleParams::arrow` (`HelpBubbleArrow`, `kNone` hides it) → `views::BubbleBorder::Arrow`                                                                                                         | `position` (`HelpBubbleArrowPosition`) → `#arrow` classes                                                 |
| **Hover / Pressed / Focus**     | *(Not modeled in Figma)*                           | Ink drop (`InkDrop::SetBaseColor`), pressed bg via `GetSystemButtonPressedColor`, `FocusRing` colored with foreground                                                                                  | `color-mix()` hover tint (6%/10%), `--cr-focus-outline-color: var(--help-bubble-foreground)`              |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                        | Figma Design Token                                                                                                        | C++ Views (Desktop)                                                                                                                                                 | WebUI (Desktop)                                                                           |
| :-------------------------------------- | :------------------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :---------------------------------------------------------------------------------------- |
| **Container Background**                | `--desktop/sys/primary-colors/primary` (`#0b57d0`)                                                                        | `kColorFeaturePromoBubbleBackground` → `ui::kColorSysPrimary` ([material_chrome_color_mixer.cc](//src/chrome/browser/ui/color/material_chrome_color_mixer.cc))      | `--color-feature-promo-bubble-background` (fallback `--google-blue-700`)                  |
| **Text / Foreground**                   | `--desktop/sys/primary-colors/on-primary` (`white`)                                                                       | `kColorFeaturePromoBubbleForeground` → `ui::kColorSysOnPrimary`                                                                                                     | `--color-feature-promo-bubble-foreground` (fallback `--google-grey-200`)                  |
| **Icon Circle Background**              | `on-primary` (BG vector)                                                                                                  | `CreatePillBackground(GetHelpBubbleForegroundColorId())` (`on-primary`)                                                                                             | `#bodyIcon { background-color: var(--help-bubble-foreground) }`                           |
| **Icon Glyph Color**                    | `--desktop/sys/primary-colors/primary`                                                                                    | `GetHelpBubbleBackgroundColorId()` (`primary`)                                                                                                                      | `color: var(--help-bubble-background)`                                                    |
| **Close Icon Color**                    | `--desktop/sys/primary-colors/on-primary`                                                                                 | `GetHelpBubbleForegroundColorId()`; ink drop `kColorFeaturePromoBubbleCloseButtonInkDrop` (`AdjustHighlightColorForContrast(fg, bg)`)                               | `--cr-icon-button-fill-color: var(--help-bubble-button-foreground)`                       |
| **Outlined Button Border / Label**      | border + label `--desktop/sys/primary-colors/on-primary`; transparent fill                                                | border `kColorFeaturePromoBubbleButtonBorder` → `kColorFeaturePromoBubbleForeground`; bg `kColorFeaturePromoBubbleBackground`; label foreground                     | `--cr-button-border-color: var(--help-bubble-foreground)`; bg `--help-bubble-background`  |
| **Filled Button Background / Label**    | bg `on-primary`; label `primary`                                                                                          | `kColorFeaturePromoBubbleDefaultButtonBackground` → foreground; `kColorFeaturePromoBubbleDefaultButtonForeground` → background                                      | `--color-feature-promo-bubble-default-button-background` / `-foreground`                  |
| **Title Typography**                    | `desktop/headline/three`: `--desktop/font/headline` (Google Sans Medium), `18px` / `24px`, weight `500`                   | `CONTEXT_IPH_BUBBLE_TITLE`: `18px` (`GetFontSizeDeltaIgnoringUserOrLocaleSettings(18)`), `MEDIUM`; platform UI font; line height = font height (not pinned to 24px) | `#title`: `18px` / `24px`, weight `500`; family inherited                                 |
| **Body Typography**                     | `desktop/body/two (regular)`: `--desktop/font/body` (Google Sans Text), `14px` / `20px`, weight `400`, letter-spacing `0` | `CONTEXT_IPH_BUBBLE_BODY`: `14px` (ignoring user/locale settings), normal weight; line height = font height (not pinned to 20px)                                    | `#topBody`/`#mainBody`: `14px` / `20px`, weight `400`, **`letter-spacing: 0.3px`**        |
| **Button Typography**                   | `desktop/special/button`: `13px` / `20px`, weight `500`                                                                   | `MdTextButton` → `style::CONTEXT_BUTTON_MD` (`13px`, medium)                                                                                                        | `<cr-button>` defaults                                                                    |
| **Container Padding**                   | `--desktop/spacing/20` (`20px`)                                                                                           | `contents_insets = gfx::Insets(20)` (FlexLayout interior margin); frame margins zeroed                                                                              | `--help-bubble-padding: 20px`                                                             |
| **Vertical Gap (rows)**                 | `--desktop/spacing/8` (`8px`)                                                                                             | `DISTANCE_RELATED_CONTROL_VERTICAL` (`8px`)                                                                                                                         | `--help-bubble-element-spacing: 8px` (`#main { margin-top }`)                             |
| **Icon ↔ Text Gap**                     | `--desktop/spacing/8` (`8px`)                                                                                             | `icon_view_` trailing margin `default_spacing` (`8px`)                                                                                                              | `#bodyIcon { margin-inline-end: 8px }`                                                    |
| **Body Indent Under Title (with icon)** | `32px` (24px icon + 8px gap; body shares column with title)                                                               | `indent = 20 + 24 + 8 = 52` from bubble edge (collapsed with 20px interior margin → `32px` from content edge)                                                       | `#middleRowSpacer { margin-inline-start: 32px }`                                          |
| **Body ↔ Button Bar Gap**               | `8px` gap + `8px` `Button Bar` top padding = `16px`                                                                       | `DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT` (`24 − 8 = 16px`)                                                                                                      | `#buttons { margin-top: 16px }`                                                           |
| **Button ↔ Button Gap**                 | `--desktop/spacing/8` (`8px`)                                                                                             | `DISTANCE_RELATED_BUTTON_HORIZONTAL` (`8px`)                                                                                                                        | *Not verified* (no explicit gap found in `help_bubble.css`)                               |
| **Button Padding / Height**             | `16px` horizontal, `8px` vertical → `36px` height                                                                         | `DISTANCE_BUTTON_HORIZONTAL_PADDING` (`16px`); height = `max(line height, font height) + 2 × 10px` (*runtime height not verified*)                                  | `<cr-button>` defaults (*not verified*)                                                   |
| **Button Corner Radius**                | `--desktop/corner-radius/fully-rounded` (`999px`)                                                                         | `ShapeContextTokens::kButtonRadius` → `ShapeSysTokens::kFull` (pill)                                                                                                | `<cr-button>` default radius (*not verified*)                                             |
| **Container Corner Radius**             | `12px` (2 frames use raw `12px`, 2 use `--desktop/corner-radius/12`)                                                      | `DialogDelegate::GetCornerRadius()` → `kDialogRadius` → `kMediumSmall` (`12px`)                                                                                     | `--help-bubble-border-radius: 12px`                                                       |
| **Width**                               | Fixed `340px`                                                                                                             | `kMinWidthDip = 200`, `kMaxWidthDip = 350` (can grow for long button labels)                                                                                        | `min-width: 200px; max-width: 350px`                                                      |
| **Body Icon Size**                      | `24px` circle, `20px` glyph                                                                                               | `kBodyIconBackgroundSize = 24`, `kBodyIconSize = 20`                                                                                                                | `--help-bubble-body-icon-size: 24px`, **`--help-bubble-body-icon-image-size: 18px`**      |
| **Close Button Size**                   | `20px` button, **`20px`** glyph (28px state wrapper)                                                                      | Button ≈ 20px (`kCloseButtonHeight = 20` min cross size); **`kIconSize = 16`**; focus halo `1.25px`                                                                 | `--help-bubble-close-button-size: 20px`, **`--help-bubble-close-button-icon-size: 16px`** |
| **Progress Dots**                       | `8px` row (89px for 4 dots + 3 × 8px gap), `4px` vertical padding                                                         | `DotView` `8×8`, 1px stroke, `8px` spacing, min row height `20px`                                                                                                   | `8px` dots, `1px` border, `8px` spacing, `margin-top: 5px`                                |
| **Arrow Size**                          | `16px` wide × `8.3px` tall                                                                                                | `BubbleBorder::kVisibleArrowRadius = 9` (18px wide), `kVisibleArrowLength = 8`                                                                                      | `--help-bubble-arrow-diameter: 16px` (rotated 11.3px square)                              |
| **Arrow Offset (from bubble edge)**     | `22px` to arrow edge (center at `30px`)                                                                                   | `kVisibleArrowBuffer = 12` + radius 9 → center at ~`21px`                                                                                                           | `--help-bubble-arrow-edge-offset: 22px` (center at `30px`)                                |
| **Elevation / Shadow**                  | `desktop/elevation/3` (key `0 4 8` 15%, ambient `0 1 3` 30%) + `Elevation/OS / Light` (`0 4 8` `#3C404326`)               | `HelpBubbleFactoryViews::GetDefaultBubbleShadow()` → `DIALOG_SHADOW` (Mac: `STANDARD_SHADOW`), default elevation; border stroke disabled                            | `box-shadow: 0 6px 10px 4px rgba(60,64,67,.15), 0 2px 3px rgba(60,64,67,.3)`              |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Frame-based Figma definition

- `IPH` in Figma is not a `COMPONENT`/`COMPONENT_SET`; it is four hand-built
  frames. There are no variant properties for icon/title/progress/buttons, so
  designers can't toggle configurations, and Code Connect mapping is not
  possible. The redundant `primary` fill on the inner `IPH Content` frame and
  the mix of raw `12px` vs. `--desktop/corner-radius/12` radii across frames
  suggest the examples have drifted.

### 2. Typography family and line height (Views)

- Figma specifies Google Sans (title) / Google Sans Text (body) with pinned
  `24px` / `20px` line heights. Views `CONTEXT_IPH_BUBBLE_TITLE` /
  `CONTEXT_IPH_BUBBLE_BODY` only set size and weight on the platform UI font and
  leave line height at the font's natural height (no `STYLE_HEADLINE_3` /
  `STYLE_BODY_3` mapping). Sizes are also deliberately fixed via
  `GetFontSizeDeltaIgnoringUserOrLocaleSettings`, so they don't scale with user
  font settings.

### 3. Elevation is not token-driven

- Figma applies `desktop/elevation/3` *plus* an extra OS-light drop shadow.
  Views uses the generic `BubbleBorder` dialog/standard shadow (platform
  dependent); WebUI hard-codes a GM2-style `box-shadow`. None reference an
  elevation-3 token directly. (Exact Views shadow values were not resolved.)

### 4. Arrow geometry (Views)

- Views' shared `BubbleBorder` arrow is 18px wide and centered ~21px from the
  bubble edge, versus Figma/WebUI's 16px arrow at 30px. This is a framework
  constant shared with all Views bubbles, not IPH-specific.

### 5. Figma lists WebUI as unsupported

- `<help-bubble>` exists and closely matches the design; the Figma doc header
  should probably mark WebUI as supported (or explain why it's excluded).

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Close icon glyph size

- Figma: `20px` `close` glyph in a `20px` button. Views (`kIconSize = 16`) and
  WebUI (`--help-bubble-close-button-icon-size: 16px`) both render a `16px`
  glyph.

### 2. Body icon glyph size (WebUI)

- Figma and Views use a `20px` glyph in a `24px` circle; WebUI uses `18px`.

### 3. Body letter-spacing (WebUI)

- WebUI body text adds `letter-spacing: 0.3px`; Figma uses `0`.

### 4. Fixed vs. flexible width

- Figma shows a fixed `340px` width. Code sizes to content between `200px` and
  `350px` (Views can exceed `350px` when long localized button labels force a
  vertical button stack).

### 5. Missing interactive states in Figma

- No Hovered / Pressed / Focused / Disabled states are modeled for the bubble
  buttons or close button. Code implements ink-drop hover, pressed tint and
  foreground-colored focus rings; WebUI uses a `color-mix()` hover tint of
  6%/10% depending on light/dark mode.

### 6. Behavioral features not represented in Figma

- Auto-dismiss timeout (10s when there are no buttons, none otherwise),
  focus-on-show when buttons are present, platform-dependent default-button
  order (leading on Windows), vertical button stacking for long labels, and
  non-activatable bubbles via `HelpBubbleEventRelay` (max one button).

### 7. Button bar `Spacer`

- Figma's `Button Bar` includes a 3px leading `Spacer` (justify-between). Code
  has no leading slot; buttons are simply end-aligned.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use IPH sparingly to highlight a single feature anchored to the relevant UI
  element; always anchor with an arrow unless the anchor is ambiguous.
- Keep body text short ("Short body text explains feature value or any actions
  needed"); add a title only when it adds meaning.
- Use at most two buttons: one outlined secondary action and one filled default
  action on the trailing side. Bubbles without buttons auto-dismiss.
- Use progress dots only for multi-step tutorials.
- Do not override the `kColorFeaturePromoBubble*` /
  `--color-feature-promo-bubble-*` tokens per surface.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **C++ Views (`HelpBubbleView`)**: Window role is `ax::mojom::Role::kAlert`;
  accessible name = `title_text + ". " + (screenreader_text or body_text)`, plus
  `keyboard_navigation_hint` on first activation. Initial focus goes to the
  first button, or the close button if there are no buttons. Always provide
  `close_button_alt_text` and `body_icon_alt_text`. Progress is *not* yet
  exposed to a11y (`TODO(crbug.com/40176811)`).
- **WebUI (`<help-bubble>`)**: `role="alertdialog"`, `aria-modal="true"`,
  `aria-labelledby="title"`, `aria-describedby="body"`, `aria-live="assertive"`;
  progress uses `role="progressbar"` with `aria-valuenow/min/max`; close button
  uses `closeButtonAltText`.

### 3. Icon Usage Guidelines

- The leading icon is optional and rendered as a `primary` glyph on an
  `on-primary` circular (24px) background; use a simple, recognizable symbol
  (e.g. `lightbulb`) and supply alt text.
- The close (`X`) button is always present.

______________________________________________________________________

> [!NOTE] **Not verified:** runtime-rendered button height and Views shadow
> blur/offset values; WebUI `<cr-button>` gap/padding/radius inside `#buttons`;
> dark-mode and high-contrast token resolution; Mac/Windows-specific layout
> differences beyond default-button order.
