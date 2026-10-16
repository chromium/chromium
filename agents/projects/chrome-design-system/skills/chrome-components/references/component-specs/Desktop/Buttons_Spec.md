# Buttons Spec (Desktop)

This document maps the specifications of the **Buttons** (Desktop) component
across Figma and code.

______________________________________________________________________

## Overview

**Buttons** are pill-shaped interactive controls on Chrome Desktop used to
trigger actions, run commands, or submit forms. The component offers four
emphasis variants (`Primary`, `Tonal`, `Outlined`, `Text`), optional leading
and/or trailing icon slots, and `Default`, `Hovered`, `Pressed` and `Disabled`
states, implemented by `views::MdTextButton` (C++ Views) and `<cr-button>`
(WebUI).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                       | C++ Views (Desktop)                                                                                                                                                                                                          | WebUI (Desktop)                                                                                                      |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Buttons` (component set, 64 variants)                                                                                                                                | `views::MdTextButton`<br>`views::MdTextButtonWithDownArrow`                                                                                                                                                                  | `<cr-button>`                                                                                                        |
| **Source Files**   | [Figma Link: `Buttons` (`20268:1070`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=20268-1070&t=qMCYxiacE3YGFLSY-11) | [ui/views/controls/button/md_text_button.h](//src/ui/views/controls/button/md_text_button.h)<br>[ui/views/controls/button/md_text_button_with_down_arrow.h](//src/ui/views/controls/button/md_text_button_with_down_arrow.h) | [ui/webui/resources/cr_elements/cr_button/cr_button.ts](//src/ui/webui/resources/cr_elements/cr_button/cr_button.ts) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: `Variant` (Primary | Tonal | Outlined | Text) × `Icons`
(None | Leading | Trailing | Leading and Trailing) × `State` (Default | Hovered
| Pressed | Disabled).

| Feature / Variant              | Figma Component              | C++ Views (Desktop)                                                                                        | WebUI (Desktop)                                                                             |
| :----------------------------- | :--------------------------- | :--------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------ |
| **Primary Style**              | `Variant=Primary`            | `MdTextButton::SetStyle(ui::ButtonStyle::kProminent)`                                                      | `<cr-button class="action-button">`                                                         |
| **Tonal Style**                | `Variant=Tonal`              | `MdTextButton::SetStyle(ui::ButtonStyle::kTonal)`                                                          | `<cr-button class="tonal-button">`                                                          |
| **Outlined Style**             | `Variant=Outlined`           | `MdTextButton::SetStyle(ui::ButtonStyle::kDefault)` *(default)*                                            | `<cr-button>` *(default)*                                                                   |
| **Text Style**                 | `Variant=Text`               | `MdTextButton::SetStyle(ui::ButtonStyle::kText)`                                                           | No built-in class; consumer must set `--cr-button-border: none`                             |
| **No Icons**                   | `Icons=None`                 | Text-only `MdTextButton`                                                                                   | `<cr-button>` with no slotted icons                                                         |
| **Leading Icon**               | `Icons=Leading`              | `MdTextButton::SetImageModel()` (image laid out before label by default)                                   | `<slot name="prefix-icon">` (sets `[has-prefix-icon_]`)                                     |
| **Trailing Icon**              | `Icons=Trailing`             | `views::MdTextButtonWithDownArrow` (drop-down arrow), or `SetHorizontalAlignment(gfx::ALIGN_RIGHT)`        | `<slot name="suffix-icon">` (sets `[has-suffix-icon_]`)                                     |
| **Leading and Trailing Icons** | `Icons=Leading and Trailing` | No built-in support; requires custom `LabelButtonImageContainer` passed to the `MdTextButton` constructor  | `prefix-icon` + `suffix-icon` slots together                                                |
| **Default State**              | `State=Default`              | `Button::STATE_NORMAL`                                                                                     | No state attribute / pseudo-class                                                           |
| **Hovered State**              | `State=Hovered`              | `Button::STATE_HOVERED` (ink drop highlight via `MdTextButton::GetHoverColor()`)                           | `:host(:hover) #hoverBackground`                                                            |
| **Pressed State**              | `State=Pressed`              | `Button::STATE_PRESSED` (`FloodFillInkDropRipple` + `NativeTheme::GetSystemButtonPressedColor()` on bg)    | `<cr-ripple id="ink">` (`--cr-button-ripple-color`)                                         |
| **Disabled State**             | `State=Disabled`             | `Button::STATE_DISABLED` (`View::SetEnabled(false)`)                                                       | `<cr-button disabled>` (`aria-disabled="true"`, `tabindex="-1"`)                            |
| **Focused State**              | *(Not modeled in Figma)*     | `views::FocusRing` (`SetInstallFocusRingOnFocus(true)`) + `kColorButtonBackground{Prominent,Tonal}Focused` | `:host-context(.focus-outline-visible):host(:focus)` → `2px solid --cr-focus-outline-color` |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                        | Figma Design Token                                                            | C++ Views (Desktop)                                                                       | WebUI (Desktop)                                                                                                                                                   |
| :-------------------------------------- | :---------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Primary Container Background**        | `--desktop/sys/primary-colors/primary` (`#0b57d0`)                            | `ui::kColorButtonBackgroundProminent`                                                     | `--color-button-background-prominent` (fallback `--cr-fallback-color-primary`)                                                                                    |
| **On-Primary Foreground**               | `--desktop/sys/primary-colors/on-primary` (`#ffffff`)                         | `ui::kColorButtonForegroundProminent` (`style::STYLE_DIALOG_BUTTON_DEFAULT`)              | `--color-button-foreground-prominent` (fallback `--cr-fallback-color-on-primary`)                                                                                 |
| **Tonal Container Background**          | `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`)                  | `ui::kColorButtonBackgroundTonal`                                                         | `--color-button-background-tonal` (fallback `--cr-fallback-color-secondary-container`)                                                                            |
| **On-Tonal Foreground**                 | `--desktop/sys/container-colors/on-tonal-container` (`#041e49`)               | `ui::kColorButtonForegroundTonal` (`style::STYLE_DIALOG_BUTTON_TONAL`)                    | `--color-button-foreground-tonal` (fallback `--cr-fallback-color-on-tonal-container`)                                                                             |
| **Outlined / Text Foreground**          | `--desktop/sys/primary-colors/primary` (`#0b57d0`)                            | `ui::kColorButtonForeground` (`style::STYLE_PRIMARY`)                                     | `--color-button-foreground` (fallback `--cr-fallback-color-primary`)                                                                                              |
| **Outlined Border**                     | `--desktop/sys/outline-colors/tonal-outline` (`#a8c7fa`, `1px` solid)         | `ui::kColorButtonBorder` (`Painter::CreateRoundRectWith1PxBorderPainter`)                 | `--color-button-border` (fallback `--cr-fallback-color-tonal-outline`), `1px solid`                                                                               |
| **Disabled Container Background**       | `--desktop/sys/state-colors/state-disabled-container` (`rgba(31,31,31,0.12)`) | `ui::kColorButtonBackgroundProminentDisabled` / `ui::kColorButtonBackgroundTonalDisabled` | `--color-button-background-prominent-disabled` / `--color-button-background-tonal-disabled`                                                                       |
| **Disabled Outlined Border**            | `--desktop/sys/state-colors/state-disabled-container` (`rgba(31,31,31,0.12)`) | `ui::kColorButtonBorderDisabled`                                                          | `--color-button-border-disabled` (fallback `--cr-fallback-color-disabled-background`)                                                                             |
| **Disabled Foreground**                 | `--desktop/sys/state-colors/state-disabled` (`rgba(31,31,31,0.38)`)           | `ui::kColorButtonForegroundDisabled` (`style::STYLE_DISABLED`)                            | `--color-button-foreground-disabled` (fallback `--cr-fallback-color-disabled-foreground`)                                                                         |
| **Hover Overlay (Primary)**             | `--desktop/sys/state-colors/state-hover-on-prominent` (`#fdfcfb1a`)           | `ui::kColorSysStateHoverOnProminent` (`GetHoverColor`)                                    | `--cr-hover-on-prominent-background-color` → `--color-sys-state-hover-on-prominent`                                                                               |
| **Hover Overlay (Tonal/Outlined/Text)** | `--desktop/sys/state-colors/state-hover-on-subtle` (`#1f1f1f0f`, 6%)          | `ui::kColorSysStateHoverOnSubtle` (`GetHoverColor`)                                       | `--cr-hover-background-color` → `--color-sys-state-hover` (fallback on-surface 8%) ⚠️                                                                             |
| **Pressed Ripple (Primary & Tonal)**    | `--desktop/sys/state-colors/state-ripple-primary` (`#7cacf852`)               | Ink drop base color = `GetHoverColor()`; bg darkened via `GetSystemButtonPressedColor()`  | Primary: `--cr-active-on-primary-background-color` → `--color-sys-state-ripple-primary`<br>Tonal: `--cr-active-background-color` → `--color-sys-state-pressed` ⚠️ |
| **Pressed Ripple (Outlined & Text)**    | `--desktop/sys/state-colors/state-ripple-neutral-on-subtle` (`#1f1f1f14`)     | Ink drop base color = `ui::kColorSysStateHoverOnSubtle`                                   | `--cr-active-background-color` → `--color-sys-state-pressed` (fallback on-surface 12%)                                                                            |
| **Composite Typography**                | `desktop/special/button`                                                      | `views::style::CONTEXT_BUTTON_MD`                                                         | Local rules in `cr_button.css` + inherited font                                                                                                                   |
| **Font Family**                         | `--desktop/font/body` (`Google Sans Text`)                                    | Resolved by `TypographyProvider`                                                          | Inherited from host page                                                                                                                                          |
| **Font Weight**                         | `--desktop/font_weight/medium` (`500`)                                        | Medium weight via `TypographyProvider` for `CONTEXT_BUTTON_MD`                            | `font-weight: 500`                                                                                                                                                |
| **Font Size**                           | `--desktop/font_size/button` (`13px`)                                         | `CONTEXT_BUTTON_MD` font size (`13px`)                                                    | Inherited (not set in `cr_button.css`)                                                                                                                            |
| **Line Height**                         | `--desktop/line_height/button` (`20px`)                                       | `TypographyProvider::GetLineHeight()`                                                     | `line-height: 20px` (`154%` for `.action-button` / `.cancel-button`)                                                                                              |
| **Button Height**                       | `36px` (`20px` line + `8px` × 2)                                              | `LayoutProvider::GetControlHeightForFont()`                                               | `height: var(--cr-button-height)` (`36px` in `cr_shared_vars.css`)                                                                                                |
| **Horizontal Padding (no icon)**        | `--desktop/spacing/16` (`16px`)                                               | `DISTANCE_BUTTON_HORIZONTAL_PADDING` (`16px`)                                             | `padding: 8px 16px`                                                                                                                                               |
| **Horizontal Padding (icon side)**      | `--desktop/spacing/12` (`12px`)                                               | `kLeftPadding = 12` when `HasImage()` (left side only)                                    | `--icon-block-padding-small: 12px` / `--icon-block-padding-large: 16px`                                                                                           |
| **Vertical Padding**                    | `--desktop/spacing/8` (`8px`)                                                 | `(target_height - label_height) / 2` in `CalculateDefaultPadding()`                       | `padding-block: 8px`                                                                                                                                              |
| **Icon Size**                           | `20px` × `20px` (Google Symbols)                                              | Size passed to `ui::ImageModel::FromVectorIcon()`                                         | `--iron-icon-height/width: 20px`                                                                                                                                  |
| **Icon-to-Label Gap**                   | `--desktop/spacing/8` (`8px`)                                                 | `kImageSpacing = 8` (`SetImageLabelSpacing`)                                              | `gap: 8px` (only when an icon is slotted)                                                                                                                         |
| **Corner Radius**                       | `--desktop/corner-radius/fully-rounded` (`999px`)                             | `ShapeContextTokens::kButtonRadius` (resolved from bounds)                                | `border-radius: 100px` (hardcoded)                                                                                                                                |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Corner Radius (`999px` vs. dynamic vs. `100px`)

- **Figma**: `--desktop/corner-radius/fully-rounded` (`999px`).
- **C++ Views**: `MdTextButton::GetCornerRadii()` queries
  `LayoutProvider::GetCornerRadiusMetric(ShapeContextTokens::kButtonRadius, size())`,
  recomputed on bounds change.
- **WebUI**: Hardcodes `border-radius: 100px` in
  [cr_button.css](//src/ui/webui/resources/cr_elements/cr_button/cr_button.css)
  (`.floating-button` uses `8px`).

### 2. Static Figma Hex Values vs. Dynamic Color Pipeline

Figma encodes light-mode baseline values. Both Views (`ui::ColorProvider`,
`ui::kColorButton*`) and WebUI (`--color-button-*` with `--cr-fallback-color-*`)
resolve colors dynamically, supporting dark mode, custom themes and forced
colors.

### 3. Hover / Pressed Token Drift in WebUI

- Subtle hover in WebUI uses `--color-sys-state-hover` (fallback on-surface 8%),
  whereas Figma and Views use `state-hover-on-subtle` (6%).
- Tonal pressed ripple in WebUI uses `--color-sys-state-pressed`, whereas Figma
  specifies `state-ripple-primary`.
- Views does not use the ripple tokens directly: the ink drop base color is the
  hover color and the background is darkened via
  `NativeTheme::GetSystemButtonPressedColor()`.

### 4. Line Height Override in WebUI

`:host(.action-button)` and `:host(.cancel-button)` override `line-height` to
`154%` (≈`20px` only at `13px`), whereas Figma uses a fixed `20px` everywhere.
WebUI also doesn't set `font-size`, so the `13px` size depends on the host page.

### 5. Asymmetric Icon Padding

- **Figma**: `12px` on any side with an icon, `16px` otherwise
  (`Leading and Trailing` → `12px` / `12px`).
- **WebUI**: Correct for single icons. With both icons, the later
  `:host([has-suffix-icon_])` rule wins, giving `16px` start / `12px` end.
- **C++ Views**: `CalculateDefaultPadding()` always applies `12px` to the
  **left** when `HasImage()`, assuming a leading icon. Trailing-icon layouts
  (`ALIGN_RIGHT`) therefore get `12px` on the wrong side unless
  `SetCustomPadding()` is used.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. `Text` Variant Missing in WebUI

Views supports `ui::ButtonStyle::kText` (transparent stroke). `<cr-button>` has
`.action-button` and `.tonal-button` but no `.text-button`; consumers must set
`--cr-button-border: none`.

### 2. `Icons=Leading and Trailing`

WebUI supports both slots simultaneously. `MdTextButton` defaults to
`SingleImageContainer` (one image); two icons need a custom
`LabelButtonImageContainer`. `MdTextButtonWithDownArrow` covers only the
trailing `arrow_drop_down` case.

### 3. Focus State Not Modeled in Figma

Figma has no `Focused` state. Views installs a `views::FocusRing` (with
`kDrawFocusRingBackgroundOutline` for prominent buttons) and switches to
`kColorButtonBackground{Prominent,Tonal}Focused`. WebUI draws a `2px` outline
with a `2px` offset using `--cr-focus-outline-color` when
`.focus-outline-visible` is set.

### 4. Pressed State Rendering

Figma shows `Pressed` as a hover overlay plus a ripple circle anchored near the
trailing edge. Code draws a ripple from the pointer location (Views
`FloodFillInkDropRipple`, WebUI `<cr-ripple>`), so the static Figma frame is
only an approximation.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Primary** (`kProminent` / `.action-button`): use for the single most
  important action in a dialog, bubble or section.
- **Tonal** (`kTonal` / `.tonal-button`): medium-high emphasis that shouldn't
  compete with a Primary button.
- **Outlined** (`kDefault` / default `<cr-button>`): standard secondary action
  (e.g., "Cancel").
- **Text** (`kText`): low-emphasis or inline actions.
- In dialogs, place the dismissive action before the affirmative action (WebUI
  `.cancel-button` adds `margin-inline-end: 8px`).
- Use short, clear action verbs ("Save", "Continue", "Delete").

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: `<cr-button>` sets `role="button"` and `tabindex="0"`, sets
  `aria-disabled` and `tabindex="-1"` when disabled, clicks on `Enter` keydown
  and `Space` keyup. Icon-only usage needs an `aria-label`.
- **C++ Views**: Use `GetViewAccessibility().SetName()` when the label needs
  more context. Use dialog default/cancel buttons so `Enter`/`Esc` work; focus
  ring and ink drop are installed automatically.

### 3. Icon Usage Guidelines

- **Leading** icons reinforce the action (e.g., `add` on "Create").
- **Trailing** icons are mainly for disclosure (e.g., `arrow_drop_down` for
  buttons that open a menu, `views::MdTextButtonWithDownArrow`).
- Use `Leading and Trailing` only when a button both represents an action and
  opens a menu. Icons are `20px` and inherit the foreground color (Views: the
  `use_text_color_for_icon` constructor argument, `true` by default; WebUI:
  slotted icons inherit the button's `color`).
