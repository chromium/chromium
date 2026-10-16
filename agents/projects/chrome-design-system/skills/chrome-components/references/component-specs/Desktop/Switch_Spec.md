# Switch Spec (Desktop)

This document maps the specifications of the **Switch** (Desktop) component
across Figma and code.

______________________________________________________________________

## Overview

The **Switch** (toggle button) component is a binary on/off control on Chrome
Desktop used to immediately activate or deactivate a setting or feature state.
It consists of a pill-shaped `26px × 16px` track and a sliding circular handle
(thumb) that expands from `8px` when unselected (`Off`) to `12px` when selected
(`On`), supporting `Default`, `Disabled`, `Hovered`, `Pressed`, and keyboard
focus states across C++ Views (`views::ToggleButton`) and WebUI (`<cr-toggle>`).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                      | C++ Views (Desktop)                                                                        | WebUI (Desktop)                                                                                                      |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Switch` (component set, 4 variants)                                                                                                                                 | `views::ToggleButton`                                                                      | `<cr-toggle>`                                                                                                        |
| **Source Files**   | [Figma Link: `Switch` (`280:26373`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26373&t=Bwx9DgfkzVhJNNVX-11) | [ui/views/controls/button/toggle_button.h](//src/ui/views/controls/button/toggle_button.h) | [ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: `Selected?` (Yes | No) × `State` (Default | Disabled) (4
variants in the `Switch` component set).

| Feature / Variant                        | Figma Component               | C++ Views (Desktop)                                                                                                                                           | WebUI (Desktop)                                                                                                                        |
| :--------------------------------------- | :---------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------- |
| **Selected (On / Checked)**              | `Selected?=Yes`               | `ToggleButton::SetIsOn(true)` (instant) or `ToggleButton::AnimateIsOn(true)` (`80ms` slide animation)                                                         | `<cr-toggle checked>` (`checked = true`, sets `aria-checked="true"`)                                                                   |
| **Unselected (Off / Unchecked)**         | `Selected?=No`                | `ToggleButton::SetIsOn(false)` or `ToggleButton::AnimateIsOn(false)` *(Default)*                                                                              | `<cr-toggle>` (`checked = false`, sets `aria-checked="false"`) *(Default)*                                                             |
| **Default (Enabled) State**              | `State=Default`               | `Button::ButtonState::STATE_NORMAL` (`View::GetEnabled() == true`)                                                                                            | Default state (`:not([disabled])`)                                                                                                     |
| **Disabled State**                       | `State=Disabled`              | `Button::ButtonState::STATE_DISABLED` (`View::SetEnabled(false)`)                                                                                             | `<cr-toggle disabled>` (`aria-disabled="true"`, `tabindex="-1"`, `pointer-events: none`)                                               |
| **Hovered State**                        | *(Not modeled in Figma)*      | `Button::ButtonState::STATE_HOVERED` (`20px` circular `InkDropHighlight` around thumb + `250ms` `hover_animation_` blending `kColorToggleButtonThumbOnHover`) | `:hover` (`20px` circular `#knob::before` halo + `:host([checked]:hover) #knob` switches to `--color-toggle-button-thumb-on-hover`)    |
| **Pressed (Active) State**               | *(Not modeled in Figma)*      | `Button::ButtonState::STATE_PRESSED` (`kThumbPressedOutset = 1` expands thumb diameter by `2px` + `20px` circular `SquareInkDropRipple`)                      | `:active` (`--cr-toggle-knob-diameter` expands from `8px` → `10px` when off and `12px` → `14px` when on + `20px` `#ink` `<cr-ripple>`) |
| **Focused State**                        | *(Not modeled in Figma)*      | `views::FocusRing` (`FocusRingHighlightPathGenerator` around the `26×16` pill track via `GetFocusRingPath()`)                                                 | `:host(:focus-visible) #bar` (`outline: 2px solid var(--cr-toggle-checked-bar-color); outline-offset: 2px;`)                           |
| **Pointer Drag Gesture**                 | *(Not applicable)*            | Click / tap activation only (`ToggleButton::NotifyClick()`)                                                                                                   | Horizontal drag (`pointerdown` + `pointermove` exceeding `MOVE_THRESHOLD_PX = 5` with `setPointerCapture`)                             |
| **Unselected Track Inner Border Toggle** | Always `1px solid` when `Off` | `ToggleButton::SetInnerBorderEnabled(bool)` (`true` by default; draws `1px` inner border when `!GetIsOn()`)                                                   | `--cr-toggle-bar-border: 1px solid var(--cr-toggle-bar-border-color)` on `#bar`                                                        |
| **Thumb Drop Shadow**                    | None (flat handle)            | `ToggleButton(PressedCallback, bool has_thumb_shadow)` (`false` by default; optional `gfx::ShadowValue` with `kShadowOffsetY = 1`, `kShadowBlur = 2`)         | None (`box-shadow: none;` on `#knob`)                                                                                                  |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                               | Figma Design Token                                                                                | C++ Views (Desktop)                                                                                              | WebUI (Desktop)                                                                                                           |
| :--------------------------------------------- | :------------------------------------------------------------------------------------------------ | :--------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------ |
| **Selected Track Background (Default)**        | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)                                             | `ui::kColorToggleButtonTrackOn` → `ui::kColorSysPrimary`<br>(customizable via `SetTrackOnColor()`)               | `--cr-toggle-checked-bar-color` → `--color-toggle-button-track-on`<br>(fallback `--cr-fallback-color-primary`)            |
| **Selected Handle / Thumb Color (Default)**    | `--desktop/sys/primary-colors/on-primary`<br>(`#ffffff`)                                          | `ui::kColorToggleButtonThumbOn` → `ui::kColorSysOnPrimary`<br>(customizable via `SetThumbOnColor()`)             | `--cr-toggle-checked-button-color` → `--color-toggle-button-thumb-on`<br>(fallback `--cr-fallback-color-on-primary`)      |
| **Unselected Track Background (Default)**      | `--desktop/sys/surface-colors/surface-variant`<br>(`#e1e3e1`)                                     | `ui::kColorToggleButtonTrackOff` → `ui::kColorSysSurfaceVariant`<br>(customizable via `SetTrackOffColor()`)      | `--cr-toggle-unchecked-bar-color` → `--color-toggle-button-track-off`<br>(fallback `--cr-fallback-color-surface-variant`) |
| **Unselected Track Border (Default)**          | `--desktop/sys/outline-colors/outline`<br>(`#747775`, `1px` solid)                                | `ui::kColorToggleButtonShadow` → `ui::kColorSysOutline` (`kBorderStrokeWidth = 1.0f`)                            | `--cr-toggle-bar-border: 1px solid var(--cr-toggle-bar-border-color)` → `--color-toggle-button-thumb-off`                 |
| **Unselected Handle / Thumb Color (Default)**  | `--desktop/sys/outline-colors/outline`<br>(`#747775`)                                             | `ui::kColorToggleButtonThumbOff` → `ui::kColorSysOutline`<br>(customizable via `SetThumbOffColor()`)             | `--cr-toggle-unchecked-button-color` → `--color-toggle-button-thumb-off`<br>(fallback `--cr-fallback-color-outline`)      |
| **Selected Track Background (Disabled)**       | `--desktop/sys/state-colors/state-disabled-container`<br>(`#1f1f1f1f` / `rgba(31,31,31,0.12)`)    | `ui::kColorToggleButtonTrackOnDisabled` → `ui::kColorSysStateDisabledContainer`                                  | `--color-toggle-button-track-on-disabled`<br>(fallback `--cr-fallback-color-disabled-background`)                         |
| **Selected Handle / Thumb Color (Disabled)**   | `--desktop/sys/surface-colors/surface`<br>(`#ffffff`)                                             | `ui::kColorToggleButtonThumbOnDisabled` → `ui::kColorSysSurface`                                                 | `--color-toggle-button-thumb-on-disabled`<br>(fallback `--cr-fallback-color-surface`)                                     |
| **Unselected Track Background (Disabled)**     | Transparent (`none`, border only)                                                                 | `ui::kColorToggleButtonTrackOffDisabled` → `ui::kColorSysSurfaceVariant` *(filled, not transparent)*             | `--cr-toggle-unchecked-bar-color: transparent;` (`:host([disabled])`)                                                     |
| **Unselected Track Border (Disabled)**         | `--desktop/sys/state-colors/state-disabled`<br>(`#1f1f1f61` / `rgba(31,31,31,0.38)`, `1px` solid) | `ui::kColorToggleButtonShadow` → `ui::kColorSysOutline` *(does not switch to disabled token)*                    | `1px solid var(--cr-toggle-unchecked-button-color)` → `--color-toggle-button-thumb-off-disabled`                          |
| **Unselected Handle / Thumb Color (Disabled)** | `--desktop/sys/state-colors/state-disabled`<br>(`#1f1f1f61` / `rgba(31,31,31,0.38)`)              | `ui::kColorToggleButtonThumbOffDisabled` → `ui::kColorSysStateDisabled`                                          | `--color-toggle-button-thumb-off-disabled`<br>(fallback `--cr-fallback-color-disabled-foreground`)                        |
| **Selected Handle Hover / Active Color**       | *(Not modeled in Figma)*                                                                          | `ui::kColorToggleButtonThumbOnHover` → `ui::kColorSysPrimaryContainer` (blended via `hover_animation_`)          | `--color-toggle-button-thumb-on-hover`<br>(fallback `--cr-fallback-color-primary-container`)                              |
| **Hover Halo Overlay Color**                   | *(Not modeled in Figma)*                                                                          | `ui::kColorToggleButtonHover` → `ui::kColorSysStateHover`                                                        | `--cr-hover-on-subtle-background-color` → `--color-sys-state-hover-on-subtle`                                             |
| **Pressed / Ripple Halo Color**                | *(Not modeled in Figma)*                                                                          | `ui::kColorToggleButtonPressed` → `ui::kColorSysStatePressed`                                                    | `--cr-active-neutral-on-subtle-background-color` → `--color-sys-state-ripple-neutral-on-subtle`                           |
| **Focus Ring Color**                           | *(Not modeled in Figma)*                                                                          | `ui::kColorFocusableBorderFocused` → `ui::kColorSysStateFocusRing` (via `views::FocusRing`)                      | `--cr-toggle-checked-bar-color` (`outline: 2px solid` on `:host(:focus-visible) #bar`)                                    |
| **Track Dimensions (Width × Height)**          | `26px` × `16px`                                                                                   | `kTrackSize = gfx::Size(26, 16)` (`26px` × `16px`)                                                               | `--cr-toggle-bar-width: 26px;` and `height: 16px;` on `#bar`                                                              |
| **Handle Diameter (Selected / On)**            | `12px` × `12px`                                                                                   | `12px` (`16px` track height minus \`2 ×                                                                          | kThumbInsetSelected                                                                                                       |
| **Handle Diameter (Unselected / Off)**         | `8px` × `8px`                                                                                     | `8px` (`16px` track height minus \`2 ×                                                                           | kThumbInset                                                                                                               |
| **Handle Diameter (Pressed / Active)**         | *(Not modeled in Figma)*                                                                          | `14px` when `On`, `10px` when `Off` (`thumb_bounds.Outset(kThumbPressedOutset)` where `kThumbPressedOutset = 1`) | `14px` (`:host([checked]:active)`) and `10px` (`:host(:active)`)                                                          |
| **Track-to-Handle Inset (Selected / On)**      | `--desktop/spacing/2`<br>(`2px`)                                                                  | `kThumbInsetSelected = -2` (`2px` inset from track edges)                                                        | `2px` (`--cr-toggle-knob-center-edge-distance_: 8px` minus `6px` thumb radius)                                            |
| **Track-to-Handle Inset (Unselected / Off)**   | `--desktop/spacing/4`<br>(`4px`)                                                                  | `kThumbInset = -4` (`4px` inset from track edges)                                                                | `4px` (`--cr-toggle-knob-center-edge-distance_: 8px` minus `4px` thumb radius)                                            |
| **Hover / Ripple Halo Diameter**               | *(Not modeled in Figma)*                                                                          | `kHoverDiameter = 20` (`20px` × `20px` centered on thumb)                                                        | `--cr-toggle-ripple-diameter: 20px;` (`20px` × `20px` centered on `#knob`)                                                |
| **Track & Handle Corner Radius**               | `--desktop/corner-radius/fully-rounded`<br>(`999px`)                                              | `track_rect.height() / 2` (`8px`) for track; `DrawCircle(..., thumb_bounds.height() / 2.f)` for thumb            | `border-radius: 50px;` on `#bar`; `border-radius: 50%;` on `#knob`                                                        |
| **State Transition Duration**                  | *(Static states)*                                                                                 | `80ms` linear (`slide_animation_`) for toggle; `250ms` linear (`hover_animation_`) for thumb hover blend         | `80ms linear` CSS transition on `#bar` (`background-color`), `#knob` (`transform`, `background-color`, `width`, `height`) |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Disabled Unselected (`Off`) Track Fill & Border Discrepancy in C++ Views

- **Figma & WebUI**: When the switch is unselected (`Selected?=No`) and
  `Disabled`, the track background is **transparent** and its `1px` border uses
  the disabled foreground token (`--desktop/sys/state-colors/state-disabled`
  `rgba(31, 31, 31, 0.38)` in Figma;
  `--cr-toggle-unchecked-bar-color: transparent` and
  `--color-toggle-button-thumb-off-disabled` border in WebUI
  [cr_toggle.css](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.css)).
- **C++ Views**: In
  [material_ui_color_mixer.cc](//src/ui/color/material_ui_color_mixer.cc),
  `ui::kColorToggleButtonTrackOffDisabled` is mapped to
  `ui::kColorSysSurfaceVariant` instead of `SK_ColorTRANSPARENT`, and
  `ToggleButton::PaintButtonContents()` in
  [toggle_button.cc](//src/ui/views/controls/button/toggle_button.cc) always
  paints the unselected inner border using `ui::kColorToggleButtonShadow`
  (`ui::kColorSysOutline`) without checking whether the control is disabled. As
  a result, a disabled `Off` `views::ToggleButton` retains an opaque
  `surface-variant` track fill and full-contrast `outline` border rather than
  the hollow disabled outline specified in Figma.

### 2. Focus Ring Color & Offset (`views::FocusRing` vs. WebUI `#bar` Outline)

- **Figma**: Does not explicitly model a `Focused` variant for `Switch`.
- **C++ Views**: Uses `views::FocusRing` with a custom
  `FocusRingHighlightPathGenerator` around the `26×16` pill track, which paints
  using `ui::kColorFocusableBorderFocused` (`ui::kColorSysStateFocusRing`) and
  standard Views focus ring halo thickness/offset.
- **WebUI**: Applies
  `outline: 2px solid var(--cr-toggle-checked-bar-color); outline-offset: 2px;`
  directly to `#bar` on `:host(:focus-visible)` in
  [cr_toggle.css](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.css)
  rather than using `--cr-focus-outline-color` (`--color-sys-state-focus-ring`).
  While both resolve to `primary` in default themes, custom overrides to
  `--cr-focus-outline-color` do not affect `<cr-toggle>`.

### 3. Forced Colors / High-Contrast Mode (HCM) Workaround in WebUI

- **C++ Views**: Relies on the platform/high-contrast `ui::ColorProvider` mixers
  plus the `1px` inner border (`inner_border_enabled_`) to distinguish states.
- **WebUI**: Includes a `@media (forced-colors: active)` override in
  [cr_toggle.css](//src/ui/webui/resources/cr_elements/cr_toggle/cr_toggle.css)
  that forces `:host #knob { background-color: CanvasText !important; }` (noted
  under `TODO(crbug.com/40168415)`) so the knob remains visible when OS forced
  colors strip background fills.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing Interactive States in Figma (`Hovered`, `Pressed`, `Focused`)

- **Figma**: The `Switch` component set (`280:26373`) only models two `State`
  values (`Default` and `Disabled`) across `Selected?=Yes` and `Selected?=No`.
- **C++ Views & WebUI**: Both implement rich interactive states not shown in the
  Figma component set:
  - **Pressed Thumb Expansion**: Pressing the switch expands the circular handle
    diameter by `2px` (`8px` → `10px` when `Off`; `12px` → `14px` when `On`) via
    `kThumbPressedOutset = 1` in `ToggleButton::GetThumbBounds()` and
    `:host(:active)` / `:host([checked]:active)` in `cr_toggle.css`.
  - **Selected Thumb Hover Recoloring**: Hovering over an `On` switch changes
    the thumb color from `on-primary` (`#ffffff`) to `primary-container`
    (`ui::kColorToggleButtonThumbOnHover` /
    `--color-toggle-button-thumb-on-hover`).
  - **`20px` Circular Hover & Ripple Halo**: Both C++ Views
    (`kHoverDiameter = 20`) and WebUI (`--cr-toggle-ripple-diameter: 20px`) draw
    a `20px` circular hover/ripple indicator centered on the thumb that extends
    `2px` beyond the `16px`-tall track.

### 2. Pointer Drag-to-Toggle Support (WebUI vs. C++ Views)

- **WebUI**: `<cr-toggle>` supports dragging the thumb horizontally
  (`pointerdown` with `setPointerCapture` + `pointermove` once horizontal
  movement exceeds `MOVE_THRESHOLD_PX = 5`, accounting for
  `:host-context([dir=rtl])`), allowing users to slide the switch on or off in
  addition to clicking.
- **C++ Views**: `views::ToggleButton` inherits standard `views::Button` click
  handling (`NotifyClick()`) and only toggles on click/tap release or keyboard
  activation, without horizontal pointer-drag tracking.

### 3. Event Acceptance Without Visual Disabling (`AcceptsEvents` in C++ Views)

- **C++ Views**: `views::ToggleButton` provides `SetAcceptsEvents(bool)` in
  addition to `View::SetEnabled(bool)`, allowing callers to temporarily block
  user input while keeping the toggle visually enabled and retaining keyboard
  focus.
- **WebUI**: `<cr-toggle>` only supports the standard `disabled` property.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Immediate Effect**: Use a `Switch` (`views::ToggleButton` / `<cr-toggle>`)
  for binary settings that take effect **immediately** without requiring the
  user to click a separate `"Save"` or `"Apply"` button (unlike checkboxes
  inside forms).
- **Standalone Visual Control**: Neither `views::ToggleButton` nor `<cr-toggle>`
  renders a built-in text label. Pair the switch with an adjacent label row
  (typically with the label on the leading side and the switch on the trailing
  side in settings surfaces).
- **Avoid Custom Color Overrides**: Prefer the default Material 3 system tokens
  (`ui::kColorToggleButton*` / `--color-toggle-button-*`). Only use
  `SetThumbOnColor()` / `SetTrackOnColor()` or `--cr-toggle-*` CSS variables
  when a surface has an approved surface-specific theme requirement.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **C++ Views (`views::ToggleButton`)**:
  - Automatically sets `ax::mojom::Role::kSwitch` and updates
    `ax::mojom::CheckedState::kTrue` / `kFalse` whenever `SetIsOn()` or
    `AnimateIsOn()` is called.
  - Because `ToggleButton` has no internal label text, callers **must** provide
    an accessible name via `GetViewAccessibility().SetName(...)` (or link it to
    a `views::Label` via `SetName(label_view)`).
  - Automatically mirrors canvas painting and focus ring geometry in RTL layouts
    (`SetFlipCanvasOnPaintForRTLUI(true)`).
- **WebUI (`<cr-toggle>`)**:
  - Automatically sets `role="switch"`, `tabindex="0"`, `aria-checked`, and
    `aria-disabled` on `firstUpdated()` and keeps them synchronized in
    `updated()`.
  - Consumers should always associate an accessible label using
    `aria-labelledby` or `aria-label` on `<cr-toggle>`.
  - Toggles on `Enter` `keydown` (ignoring `Ctrl+Enter` on macOS) and `Space`
    `keyup`, and stops `click` event propagation (`e.stopPropagation()`) so
    enclosing clickable settings rows do not double-toggle the control.

### 3. Icon Usage Guidelines

- **No Internal Icons**: The Chrome Desktop `Switch` component (`26px × 16px`
  track with `8px`/`12px` thumb) is intentionally compact and does **not**
  support icons inside the thumb or track. If an icon is needed to represent the
  feature being toggled, place it leading the text label in the parent row
  container.
