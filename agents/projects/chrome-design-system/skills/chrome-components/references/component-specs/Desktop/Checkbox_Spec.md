# Checkbox Spec (Desktop)

This document maps the specifications of the **Checkbox** (Desktop) component
across Figma and code.

______________________________________________________________________

## Overview

The **Checkbox** is a binary selection control on Chrome Desktop that lets users
toggle an option between checked (`Selected?=Yes`) and unchecked
(`Selected?=No`), either standalone or paired with a text label. It supports
`Default`, `Hovered`, `Pressed` and `Disabled` states (plus keyboard focus in
code), implemented by `views::Checkbox` (C++ Views) and `<cr-checkbox>` (WebUI).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                        | C++ Views (Desktop)                                                              | WebUI (Desktop)                                                                                                              |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Checkbox` (component set, 8 variants)                                                                                                                 | `views::Checkbox`                                                                | `<cr-checkbox>`                                                                                                              |
| **Source Files**   | [Figma Link: `Checkbox` (`280:22475`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-22475&m=dev) | [ui/views/controls/button/checkbox.h](//src/ui/views/controls/button/checkbox.h) | [ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts](//src/ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: `Selected?` (Yes | No) × `Theme` (Light) × `State` (Default
| Hovered | Pressed | Disabled).

| Feature / Variant         | Figma Component                      | C++ Views (Desktop)                                                                                                | WebUI (Desktop)                                                                            |
| :------------------------ | :----------------------------------- | :----------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------- |
| **Checked**               | `Selected?=Yes`                      | `Checkbox::SetChecked(true)` (`IconState::CHECKED`)                                                                | `<cr-checkbox checked>` (`aria-checked="true"`)                                            |
| **Unchecked**             | `Selected?=No`                       | `Checkbox::SetChecked(false)` *(default)*                                                                          | `<cr-checkbox>` *(default, `aria-checked="false"`)*                                        |
| **Default State**         | `State=Default`                      | `Button::STATE_NORMAL` (`IconState::ENABLED`)                                                                      | No state attribute / pseudo-class                                                          |
| **Hovered State**         | `State=Hovered`                      | `Button::STATE_HOVERED` (circular `InkDropHighlight` via `CircleLayerDelegate` on `image_container_view()`)        | `#checkbox:hover #hover-layer` (`32px` circular overlay)                                   |
| **Pressed State**         | `State=Pressed`                      | `Button::STATE_PRESSED` (`CreateSquareRipple()` on `image_container_view()` and on the `Checkbox` host)            | `<cr-ripple id="ink">` shown on `pointerdown` (`CrRippleMixin`)                            |
| **Disabled State**        | `State=Disabled`                     | `Button::STATE_DISABLED` (`View::SetEnabled(false)`)                                                               | `<cr-checkbox disabled>` (`aria-disabled="true"`, `tabindex="-1"`, `pointer-events: none`) |
| **Focused State**         | *(Not modeled in Figma)*             | `views::FocusRing` (`SetInstallFocusRingOnFocus(true)` + `FocusRingHighlightPathGenerator` around the icon bounds) | `#checkbox:focus-visible` (`2px solid --cr-focus-outline-color`, `outline-offset: 2px`)    |
| **Light Theme**           | `Theme=Light`                        | Resolved dynamically via `ui::ColorProvider`                                                                       | Resolved dynamically via `--color-*` variables / `@media (prefers-color-scheme: dark)`     |
| **Dark Theme**            | *(Not modeled in Figma)*             | Resolved dynamically via `ui::ColorProvider`                                                                       | Resolved dynamically via `--color-*` variables / `@media (prefers-color-scheme: dark)`     |
| **Trailing Label**        | *(Not modeled in Figma)*             | `Checkbox(const std::u16string& label, ...)` (from `views::LabelButton`, `gfx::ALIGN_LEFT`)                        | Default `<slot>` inside `#labelContainer`                                                  |
| **Leading Label**         | *(Not modeled in Figma)*             | No built-in support                                                                                                | `<cr-checkbox class="label-first">`                                                        |
| **Standalone (No Label)** | Standalone `16px` box (all variants) | `Checkbox(std::u16string())` (empty label)                                                                         | `<cr-checkbox class="no-label">`                                                           |
| **Multi-line Label**      | *(Not modeled in Figma)*             | `Checkbox::SetMultiLine(true)`                                                                                     | Default (`#labelContainer { white-space: normal; }`)                                       |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                                            | C++ Views (Desktop)                                                                                            | WebUI (Desktop)                                                                                                                           |
| :------------------------------- | :---------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------- |
| **Checked Container Background** | `--desktop/sys/primary-colors/primary` (`#0b57d0`)                            | `ui::kColorCheckboxContainer` (→ `kColorSysPrimary`)                                                           | `--cr-checkbox-checked-box-color` → `--cr-checked-color` → `--color-checkbox-foreground-checked` (fallback `--cr-fallback-color-primary`) |
| **Checkmark Color**              | `--desktop/sys/surface-colors/surface` (`#ffffff`)                            | `ui::kColorCheckboxCheck` (→ `kColorSysOnPrimary`; overridable via `SetCheckedIconImageColor()`)               | `--cr-checkbox-mark-color` → `--color-checkbox-check` (fallback `--cr-fallback-color-on-primary`)                                         |
| **Unchecked Outline**            | `--desktop/sys/outline-colors/outline` (`#747775`)                            | `ui::kColorCheckboxOutline` (→ `kColorSysOutline`)                                                             | `--cr-checkbox-unchecked-box-color` → `--color-checkbox-foreground-unchecked` (fallback `--cr-fallback-color-outline`)                    |
| **Disabled Checked Container**   | `--desktop/sys/state-colors/state-disabled-container` (`#1f1f1f1f`)           | `ui::kColorCheckboxContainerDisabled` (→ `kColorSysStateDisabledContainer`)                                    | `--color-checkbox-container-disabled` (fallback `--cr-fallback-color-disabled-background`)                                                |
| **Disabled Checkmark**           | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`)                     | `ui::kColorCheckboxCheckDisabled` (→ `kColorSysStateDisabled`)                                                 | `--color-checkbox-check-disabled` (fallback `--cr-fallback-color-disabled-foreground`)                                                    |
| **Disabled Unchecked Outline**   | `--desktop/sys/state-colors/state-disabled-container` (`#1f1f1f1f`)           | `ui::kColorCheckboxOutlineDisabled` (→ `kColorSysStateDisabledContainer`)                                      | `--color-checkbox-outline-disabled` (fallback `--cr-fallback-color-disabled-background`)                                                  |
| **Hover Overlay**                | `--desktop/sys/state-colors/state-hover-on-subtle` (`#1f1f1f0f`, 6%)          | `InkDropHighlight` with base color `ui::kColorCheckboxForegroundUnchecked` (→ `kColorSecondaryForeground`)     | `--cr-hover-background-color` → `--color-sys-state-hover` (fallback on-surface @ 8%)                                                      |
| **Pressed Overlay**              | `--desktop/sys/state-colors/state-ripple-neutral-on-subtle` (`#1f1f1f14`, 8%) | Square ripple with base color `kColorCheckboxForegroundUnchecked` (icon) / `GetIconImageColor(ENABLED)` (host) | `--cr-active-background-color` → `--color-sys-state-pressed` (ripple opacity `.15` unchecked / `.2` checked; `.4` in dark mode)           |
| **Focus Ring**                   | *(Not modeled in Figma)*                                                      | `views::FocusRing` default color (`kColorFocusableBorderFocused`)                                              | `--cr-focus-outline-color` → `--color-sys-state-focus-ring` (fallback `--cr-fallback-color-primary`)                                      |
| **Label Text**                   | *(Not modeled in Figma)*                                                      | `LabelButton` label (`style::CONTEXT_BUTTON`)                                                                  | `--cr-checkbox-label-color` (fallback `--cr-primary-text-color`)                                                                          |
| **Box Size**                     | `16px` × `16px`                                                               | `kCheckboxIconDipSize = 16`                                                                                    | `--cr-checkbox-size: 16px`                                                                                                                |
| **Outline Width**                | `2px` (`border-2`)                                                            | Baked into `kCheckboxCustomIcon` vector                                                                        | `--cr-checkbox-border-size: 2px`                                                                                                          |
| **Box Corner Radius**            | `--desktop/corner-radius/2` (`2px`)                                           | `kCheckboxIconCornerRadius = 2`                                                                                | `border-radius: 2px` (hardcoded in `cr_checkbox.css`)                                                                                     |
| **Checkmark Size**               | `12px` × `12px` (`Check` frame)                                               | `16dp` vector canvas (`kCheckIcon` / `kCheckboxCheckCr2023OldIcon`)                                            | `12px` × `12px` inline `<svg id="checkmark">`                                                                                             |
| **Hover / Pressed Halo Size**    | `32px` × `32px` (`State` layer, `-8px` offset)                                | Highlight radius `InkDropHost::GetLargeSize(24×24).width() / 2` (≈`16dp`); ripple `24dp` small size            | `--cr-checkbox-ripple-size: 32px`; `#hover-layer` `32px`                                                                                  |
| **Halo Shape**                   | `--desktop/corner-radius/fully-rounded` (`999px`)                             | Circle (hover) / rounded square (pressed)                                                                      | `border-radius: 50%` (hover) / `.circle` ripple                                                                                           |
| **Outer Padding / Label Gap**    | *(Not modeled in Figma)*                                                      | `INSETS_CHECKBOX_RADIO_BUTTON` (`VH(5, 6)`); gap `DISTANCE_RELATED_LABEL_HORIZONTAL` (`12dp`)                  | `--cr-checkbox-label-padding-start: 20px` (`--cr-checkbox-label-padding-end` for `.label-first`)                                          |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Checkmark token: `surface` vs. `on-primary`

- **Figma** binds the checkmark to `--desktop/sys/surface-colors/surface`.
- **Code** uses `ui::kColorCheckboxCheck` → `kColorSysOnPrimary`
  ([material_ui_color_mixer.cc](//src/ui/color/material_ui_color_mixer.cc)) and
  `--color-checkbox-check` / `--cr-fallback-color-on-primary`. Both resolve to
  `#ffffff` in light mode, but `on-primary` is the semantically correct pairing
  for a `primary` container and diverges in dark mode.

### 2. Hover / pressed state-layer tokens differ from Figma

- **Figma**: hover `state-hover-on-subtle` (6%), pressed
  `state-ripple-neutral-on-subtle` (8%).
- **WebUI** uses the generic `--cr-hover-background-color`
  (`--color-sys-state-hover`, fallback 8%) and `--cr-active-background-color`
  (`--color-sys-state-pressed`) scaled by ripple opacity (`.15`/`.2` light, `.4`
  dark). The `--cr-hover-on-subtle-background-color` and
  `--cr-active-neutral-on-subtle-background-color` variables that match Figma
  exist in
  [cr_shared_vars.css](//src/ui/webui/resources/cr_elements/cr_shared_vars.css)
  but are not used by `<cr-checkbox>`.
- **C++ Views** derives highlight/ripple color from
  `kColorCheckboxForegroundUnchecked` (→ `kColorSecondaryForeground` →
  `kColorSysOnSurfaceSubtle`) with ink-drop default opacities, rather than the
  `kColorSysStateHoverOnSubtle` / `kColorSysStateRippleNeutralOnSubtle` tokens.

### 3. Legacy vs. Material ColorIds between Views and WebUI

- **C++ Views** ([checkbox.cc](//src/ui/views/controls/button/checkbox.cc)) uses
  `kColorCheckboxContainer` / `kColorCheckboxOutline` for enabled states.
- **WebUI**
  ([cr_checkbox.css](//src/ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.css))
  uses legacy `--color-checkbox-foreground-checked` (→ `kColorButtonForeground`
  → `kColorSysPrimary`) and `--color-checkbox-foreground-unchecked` (→
  `kColorSecondaryForeground` → `kColorSysOnSurfaceSubtle`, **not**
  `kColorSysOutline`) for enabled states, so the WebUI unchecked outline can
  differ from Figma's `outline` token. Disabled states use the same Material IDs
  on both platforms.

### 4. Checkmark geometry & animation

- **Figma**: rotated check inside a `12px` frame centered in the box.
- **WebUI**: `12px` inline SVG, animated `scale(0)` → `scale(1)` over
  `140ms ease-out` on check.
- **C++ Views**: `16dp` vector icon, `kCheckIcon` when
  `features::IsRoundedIconsEnabled()` else `kCheckboxCheckCr2023OldIcon`; no
  toggle animation.

### 5. Dual ink drops in C++ Views

`views::Checkbox` installs two `InkDropHost`s: one on the host (no hover
highlight, square ripple `21dp`/`28dp`) and one on `image_container_view()`
(circular hover highlight ≈`32dp` matching Figma, but a **square** pressed
ripple `24dp`/`32dp` instead of Figma's circle).

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Standalone indicator in Figma vs. labeled control in code

The Figma component models only the `16px` indicator. `views::Checkbox` is a
`views::LabelButton` with border insets and a min-height bump
(`preferred height + 4`); `<cr-checkbox>` is a flex host with a label slot and
`20px` label padding. Standalone WebUI usage requires `class="no-label"`. The
Views label gap (`12dp`) and WebUI label gap (`20px`) also differ from each
other, with no Figma reference.

### 2. No indeterminate (mixed) state

Neither Figma nor code supports a tri-state / `aria-checked="mixed"` variant;
both implementations model `checked` as `bool`.

### 3. Missing Focused state & Dark theme in Figma

`State` has no `Focused` value even though both implementations render a focus
ring (Views: rectangle around icon bounds via `GetFocusRingPath()`; WebUI: `2px`
outline with `2px` offset, `--cr-focus-outline-hcm` in forced-colors). `Theme`
only offers `Light`.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use checkboxes for selecting zero or more independent options, or for a single
  opt-in setting confirmed by another action. Use radio buttons
  (`views::RadioButton` / `<cr-radio-button>`) for mutually exclusive choices,
  and prefer a toggle (`<cr-toggle>`) for settings that apply immediately.
- Label with a clear, positive statement of what checking does (avoid negated
  labels such as "Don't send…"); use sentence case.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI (`<cr-checkbox>`)**: `role="checkbox"`, `tabindex`, `aria-checked` and
  `aria-disabled` live on the internal `#checkbox`, not `:host`; use `focus()` /
  `getFocusableElement()`. The label slot is wired via
  `aria-labelledby="labelContainer"`; with `no-label` set `aria-label-override`.
  `aria-description` is supported. Toggles on `Space` (keyup) and `Enter`
  (keydown, except `Ctrl+Enter` on macOS); fires `change` (`detail: boolean`)
  only on user interaction. Clicks on slotted `<a>` links do not toggle.
- **C++ Views (`views::Checkbox`)**: sets `ax::mojom::Role::kCheckBox`,
  `CheckedState` (`kTrue`/`kFalse`) and default action verb
  (`kCheck`/`kUncheck`). Checkboxes always show a focus ring. When using an
  empty label, provide an accessible name via
  `GetViewAccessibility().SetName(...)`.

### 3. Icon Usage Guidelines

- The checkmark is built in; do not substitute custom icons. Views allows
  overriding only the checkmark color via `SetCheckedIconImageColor()`; WebUI
  via `--cr-checkbox-mark-color`.
