# Text Fields Spec (Desktop)

This document maps the specifications of the **Text Fields** (Desktop) component
across Figma and code. It covers both Desktop implementations: **C++ Views** and
**WebUI**.

______________________________________________________________________

## Overview

**Text Fields** are single-line text inputs on Chrome Desktop. Each platform has
its own Figma component:

- **`Text Fields (Views)`** is an outlined, fully rounded (`8px`) field. It
  shows `Default`, `Selected` (focused, with a text-selection highlight) and
  `Disabled` states and is implemented by `views::Textfield`.
- **`Text Fields (WebUI)`** is a filled field with a label, top-rounded corners
  and an underline. It shows `Default`, `Selected`, `Disabled` and `Error`
  states and is implemented by `<cr-input>`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                            | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                  |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Text Fields (Views)` (component set, 3 variants)<br>`Text Fields (WebUI)` (component set, 4 variants)                                                                                                                                                                                                                                     | `views::Textfield`                                                                       | `<cr-input>`                                                                                                     |
| **Source Files**   | [Figma Link: `Text Fields (Views)` (`30230:10704`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=30230-10704&m=dev)<br>[Figma Link: `Text Fields (WebUI)` (`280:26417`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26417&m=dev) | [ui/views/controls/textfield/textfield.h](//src/ui/views/controls/textfield/textfield.h) | [ui/webui/resources/cr_elements/cr_input/cr_input.ts](//src/ui/webui/resources/cr_elements/cr_input/cr_input.ts) |

Figma-to-code mapping:

- `Text Fields (Views)` → `views::Textfield`.
- `Text Fields (WebUI)` → `<cr-input>`.

The two designs are visually different (outlined vs. filled), so they don't map
across platforms.

Related sources:

- **Views**: [textfield.cc](//src/ui/views/controls/textfield/textfield.cc),
  [focusable_border.cc](//src/ui/views/controls/focusable_border.cc),
  [focus_ring.h](//src/ui/views/controls/focus_ring.h),
  [layout_provider.cc](//src/ui/views/layout/layout_provider.cc),
  [typography_provider.cc](//src/ui/views/style/typography_provider.cc),
  [chrome_typography_provider.cc](//src/chrome/browser/ui/views/chrome_typography_provider.cc),
  [material_ui_color_mixer.cc](//src/ui/color/material_ui_color_mixer.cc).
- **WebUI**:
  [cr_input_style_lit.css](//src/ui/webui/resources/cr_elements/cr_input/cr_input_style_lit.css),
  [cr_input.css](//src/ui/webui/resources/cr_elements/cr_input/cr_input.css),
  [cr_input.html.ts](//src/ui/webui/resources/cr_elements/cr_input/cr_input.html.ts).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- **`Text Fields (Views)`**: `State` (Default | Selected | Disabled).
- **`Text Fields (WebUI)`**: `State` (Default | Selected | Disabled | Error).

Structure of each Figma component:

- **Views**: a `Text Field` frame (outlined, `8px` radius) containing a single
  `Text` value. In `Selected`, the text sits on a `Highlight` rectangle (text
  selection). There is no label and no icon.
- **WebUI**: a `Label` row; a `Text Field` container with `Content` (an
  `LHS Content` group holding a 16px `Icon` and the `Text` value, a `Spacer`,
  and an empty 15px `RHS Content` slot); and an `Underline`.

### 2a. C++ Views — `Text Fields (Views)` ↔ `views::Textfield`

| Feature / Variant            | Figma Component                           | C++ Views (Desktop)                                                                                                                                           |
| :--------------------------- | :---------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Default State**            | `State=Default`                           | Enabled `views::Textfield`. `views::FocusableBorder(kColorTextfieldOutline)` (1px) plus the `kColorTextfieldBackground` round-rect background                 |
| **Selected (Focused) State** | `State=Selected` (2px ring + `Highlight`) | `View::HasFocus()`: `views::FocusRing` (2px halo, `SetOutsetFocusRingDisabled(true)`). Selected text uses `kColorTextfieldSelectionBackground`                |
| **Disabled State**           | `State=Disabled`                          | `View::SetEnabled(false)`. `GetTextStyle()` returns `style::STYLE_DISABLED`, giving the disabled background, a transparent outline and disabled text          |
| **Read-only**                | *(Not modeled in Figma)*                  | `Textfield::SetReadOnly(true)`. Styled the same as Disabled (background, outline, text color)                                                                 |
| **Invalid / Error**          | *(Not modeled in Views Figma)*            | `Textfield::SetInvalid(true)`: `kColorTextfieldOutlineInvalid` border, `FocusRing::SetInvalid` (`kColorAlertHighSeverity`), `STYLE_INVALID` placeholder       |
| **Hovered State**            | *(Not modeled in Figma)*                  | `InkDropHost` highlight (`kColorTextfieldHover`, opacity `1.0`, painted above the content). Suppressed while focused                                          |
| **Placeholder**              | *(Not modeled in Figma)*                  | `Textfield::SetPlaceholderText()`. Color comes from `CONTEXT_TEXTFIELD_PLACEHOLDER` (`kColorTextfieldForegroundPlaceholder`) or `SetPlaceholderTextColorId()` |
| **Label**                    | *(Not modeled in Figma)*                  | No built-in label. Pair it with a `views::Label` and set the accessible name                                                                                  |
| **Custom border / insets**   | *(Not modeled in Figma)*                  | `SetBorder()` (removes the `FocusRing`), `SetUseDefaultBorder()`, `SetExtraInsets()`, `SetCornerRadius()`                                                     |
| **Dark Theme**               | *(Not modeled in Figma)*                  | Resolved dynamically through `ui::ColorProvider`                                                                                                              |

### 2b. WebUI — `Text Fields (WebUI)` ↔ `<cr-input>`

| Feature / Variant            | Figma Component                | WebUI (Desktop)                                                                                                                       |
| :--------------------------- | :----------------------------- | :------------------------------------------------------------------------------------------------------------------------------------ |
| **Default State**            | `State=Default`                | `<cr-input>`: `#underline-base` (1px) is visible and `#underline` is hidden (`opacity: 0`, `width: 0`)                                |
| **Selected (Focused) State** | `State=Selected`               | `focused_` attribute, reflected on `<input>` focus (internal only). `#underline` (2px) animates to `width: 100%`                      |
| **Disabled State**           | `State=Disabled`               | `<cr-input disabled>`. The host `color` becomes the disabled foreground, and the hover layer is suppressed                            |
| **Error State**              | `State=Error`                  | `<cr-input invalid error-message="...">` (or `auto-validate` with `pattern`/`required`/`min`/`max`). Shows `#error` text              |
| **Label**                    | `Label` row (always shown)     | The `label` property renders `#label.cr-form-field-label` (`hidden` when empty)                                                       |
| **Leading Icon**             | `LHS Content` › `Icon` (16px)  | `<slot name="inline-prefix">` (e.g. `<cr-icon slot="inline-prefix">` / `<cr-icon-button>`)                                            |
| **Trailing Content**         | `RHS Content` (15px, empty)    | `<slot name="inline-suffix">` (inside the field) or `<slot name="suffix">` (outside the field, e.g. `<cr-button>`)                    |
| **Value / Placeholder**      | `Text` ("Text")                | The `value` / `placeholder` properties on the internal `<input id="input">`                                                           |
| **Hovered State**            | *(Not modeled in Figma)*       | `#input-container:hover #hover-layer` (not when `readonly` / `disabled`)                                                              |
| **Read-only**                | *(Not modeled in Figma)*       | `<cr-input readonly>`: no underline, all four corners rounded `8px`, input `opacity: 0.6`                                             |
| **Stroked (Outlined) Style** | *(Not modeled in WebUI Figma)* | `<cr-input class="stroked">`: 1px border, `8px` radius, `36px` min height, 2px focus border (visually close to `Text Fields (Views)`) |
| **Error message text**       | *(Not modeled in Figma)*       | `#error` (`role="alert"`, `aria-live="assertive"`). Space is reserved even when valid unless `--cr-input-error-display: none`         |
| **Force underline**          | *(Not modeled in Figma)*       | The `force-underline` attribute shows the focused underline without focus                                                             |
| **Dark Theme**               | *(Not modeled in Figma)*       | Resolved dynamically through `--color-textfield-*` variables (or the `@media (prefers-color-scheme: dark)` fallbacks)                 |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

Legend: ✅ matches · ⚠️ mismatch or risk (see §4–5).

### 3a. C++ Views — `Text Fields (Views)`

| Design Attribute          | Figma Design Token                                                                                    | C++ Views (Desktop)                                                                                                                                                                                                                                                                                     |
| :------------------------ | :---------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Container Background**  | *(No fill in Figma — transparent)*                                                                    | `kColorTextfieldBackground` → `kColorSysSurface`, painted with `CreateSolidRoundRectPainter`. Override with `SetBackgroundColor()` ⚠️                                                                                                                                                                   |
| **Outline (Default)**     | `--desktop/sys/outline-colors/neutral-outline` (`#c7c7c7`), 1px                                       | `views::FocusableBorder(kColorTextfieldOutline)` → `kColorSysNeutralOutline`. Stroke is 1 physical px (`kStrokeWidth = dsf`) ✅                                                                                                                                                                         |
| **Focus Ring (Selected)** | `--desktop/sys/state-colors/state-focus-ring` (`#0b57d0`), 2px border                                 | `views::FocusRing`: `kDefaultHaloThickness = 2`, `kDefaultHaloInset = -1`, so the ring straddles the edge (1px outside, 1px inside). Color is `GetCascadingAccentColor()` → `kColorFocusableBorderFocused` → `kColorSysStateFocusRing`, contrast-adjusted against the background ✅ color / ⚠️ geometry |
| **Selection Highlight**   | `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`)                                          | `kColorTextfieldSelectionBackground` → `kColorTextSelectionBackground` → `kColorSysTonalContainer` ✅                                                                                                                                                                                                   |
| **Selected Text Color**   | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                                                 | `kColorTextfieldSelectionForeground` → `kColorTextSelectionForeground` = `GetColorWithMaxContrast(kColorTextSelectionBackground)` (black/white, not `on-surface`) ⚠️                                                                                                                                    |
| **Text Color**            | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                                                 | `CONTEXT_TEXTFIELD` + `STYLE_PRIMARY` → `kColorTextfieldForeground` → `kColorSysOnSurface`. Override with `SetTextColorId()` ✅                                                                                                                                                                         |
| **Disabled Background**   | `--desktop/sys/state-colors/state-disabled-container` (`#1f1f1f1f`)                                   | `kColorTextfieldBackgroundDisabled` = `GetResultingPaintColor(kColorSysStateDisabledContainer` over `kColorSysSurface)` (opaque). Also used for read-only ✅ value / ⚠️ opacity                                                                                                                         |
| **Disabled Outline**      | None (no border)                                                                                      | `kColorTextfieldOutlineDisabled` → `SK_ColorTRANSPARENT` ✅                                                                                                                                                                                                                                             |
| **Disabled Text**         | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`)                                             | `STYLE_DISABLED` → `GetDisabledColorId(CONTEXT_TEXTFIELD)` → `kColorTextfieldForegroundDisabled` → `kColorSysStateDisabled`. Override with `SetDisabledTextColorId()` ✅                                                                                                                                |
| **Invalid Outline**       | *(Not modeled)*                                                                                       | `kColorTextfieldOutlineInvalid` → `kColorTextfieldForegroundPlaceholderInvalid` = `BlendForMinContrast(kColorSysError, kColorTextfieldBackground)`                                                                                                                                                      |
| **Invalid Focus Ring**    | *(Not modeled)*                                                                                       | `kColorAlertHighSeverity` (`FocusRing::SetInvalid(true)`)                                                                                                                                                                                                                                               |
| **Placeholder Color**     | *(Not modeled)*                                                                                       | `kColorTextfieldForegroundPlaceholder` → `kColorSysOnSurfaceSubtle` (`kColorTextfieldForegroundPlaceholderInvalid` when invalid)                                                                                                                                                                        |
| **Hover Overlay**         | *(Not modeled)*                                                                                       | `kColorTextfieldHover` → `kColorSysStateHoverOnSubtle` (InkDrop highlight, transparent while focused)                                                                                                                                                                                                   |
| **Typography**            | `desktop/body/four (regular)`: Google Sans Text, 400, `12px` / **`18px`**                             | `Textfield::GetDefaultFontList()` = `GetFontListWithDelta(ui::kLabelFontSizeDelta)` (12px platform UI font, regular). Control line height `kControlHeight = 16` ⚠️                                                                                                                                      |
| **Horizontal Padding**    | `--desktop/spacing/10` (`10px`)                                                                       | `DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING = 10` (plus `SetExtraInsets()`) ✅                                                                                                                                                                                                                          |
| **Vertical Padding**      | `--desktop/spacing/9` (`9px`) + `18px` line = `36px`                                                  | `DISTANCE_CONTROL_VERTICAL_TEXT_PADDING = 10` + `16px` line = `36px` (`GetControlHeightForFont`) ⚠️ same height, different split                                                                                                                                                                        |
| **Height**                | `36px` (Default / Selected). The Disabled symbol is `34px`, with a `-2.94%` inset that draws it at 36 | Preferred height `36dp` (`LayoutProvider::GetControlHeightForFont(CONTEXT_TEXTFIELD, ...)`) ✅                                                                                                                                                                                                          |
| **Corner Radius**         | `--desktop/corner-radius/8` (`8px`, all corners)                                                      | `ShapeContextTokens::kTextfieldRadius` → `ShapeSysTokens::kSmall` = `8` (override with `SetCornerRadius()`). Shared by the border, background and focus-ring path ✅                                                                                                                                    |
| **Width**                 | `219px` (fixed in the kit)                                                                            | Determined by the layout (`SetDefaultWidthInChars()` / layout manager)                                                                                                                                                                                                                                  |

### 3b. WebUI — `Text Fields (WebUI)`

| Design Attribute                  | Figma Design Token                                                                       | WebUI (Desktop)                                                                                                                                                                                                                           |
| :-------------------------------- | :--------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Container Background**          | `--desktop/sys/surface-colors/surface-variant` (`#e1e3e1`)                               | `--cr-input-background-color` → `--color-textfield-filled-background` (`kColorTextfieldFilledBackground` → `kColorSysSurfaceVariant`), fallback `--cr-fallback-color-surface-variant` ✅                                                  |
| **Disabled Container Background** | `--desktop/sys/state-colors/state-disabled-container` (`#1f1f1f1f`)                      | `--color-textfield-background-disabled` (`kColorTextfieldBackgroundDisabled` = `GetResultingPaintColor(kColorSysStateDisabledContainer` over `kColorSysSurface)`, so it is opaque), fallback `--cr-fallback-color-disabled-background` ⚠️ |
| **Underline (Default)**           | `--desktop/sys/outline-colors/outline` (`#747775`), 1px                                  | `--cr-input-border-bottom: 1px solid --color-textfield-filled-underline` (`kColorTextfieldFilledUnderline` → `kColorSysOutline`), fallback `--cr-fallback-color-outline` ✅                                                               |
| **Underline (Selected)**          | `--desktop/sys/state-colors/state-focus-ring` (`#0b57d0`), 2px                           | `#underline` `2px solid --cr-input-focus-color` → `--color-textfield-filled-underline-focused` (`kColorTextfieldFilledUnderlineFocused` → `kColorSysPrimary`), fallback `--cr-fallback-color-primary` ⚠️ token name differs               |
| **Underline (Disabled)**          | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`), 1px                           | `--cr-input-border-bottom: 1px solid currentColor`, where host `color` = `--color-textfield-foreground-disabled` (`kColorTextfieldForegroundDisabled` → `kColorSysStateDisabled`) ✅                                                      |
| **Underline (Error)**             | `--desktop/sys/error-colors/error` (`#b3261e`), 2px                                      | `#underline` with `border-color: --cr-input-error-color` → `--color-textfield-filled-error` (**no such ColorId**), so it always uses `--cr-fallback-color-error` ⚠️                                                                       |
| **Label Color**                   | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)                             | `--cr-input-label-color` → `--color-textfield-foreground-label` (`kColorTextfieldForegroundLabel` → `kColorSysOnSurfaceSubtle`). On focus: `--cr-input-focus-label-color` (defaults to the label color) ✅                                |
| **Label Color (Disabled)**        | `--desktop/sys/surface-colors/on-surface-subtle` (unchanged)                             | `#label` keeps `--cr-input-label-color` (`opacity: 1` override in `cr_input.css`) ✅                                                                                                                                                      |
| **Label Color (Error)**           | `--desktop/sys/error-colors/error`                                                       | `:host([invalid]) #label { color: var(--cr-input-error-color) }` (see the Error underline row) ✅ value / ⚠️ token                                                                                                                        |
| **Input Text Color**              | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                                    | `--cr-input-color` → `--cr-primary-text-color` → `--color-primary-foreground` (legacy, not `--color-textfield-foreground` / `kColorSysOnSurface`) ⚠️                                                                                      |
| **Input Text Color (Disabled)**   | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`)                                | `--cr-input-color: currentColor` → `--color-textfield-foreground-disabled` ✅                                                                                                                                                             |
| **Placeholder Color**             | *(Not modeled in Figma)*                                                                 | `--color-textfield-foreground-placeholder` (`kColorSysOnSurfaceSubtle`), fallback `--cr-fallback-on-surface-subtle` (**typo, undefined variable**) ⚠️                                                                                     |
| **Icon Color**                    | `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)                             | Slotted `--cr-icon-color` / `--cr-icon-button-fill-color` → `--color-textfield-foreground-icon` (`kColorSysOnSurfaceSubtle`) ✅                                                                                                           |
| **Icon Color (Disabled)**         | `--desktop/sys/state-colors/state-disabled`                                              | Slotted `--cr-icon-color: currentColor` (disabled foreground) ✅                                                                                                                                                                          |
| **Icon Color (Error)**            | `--desktop/sys/error-colors/error`                                                       | Slotted `--cr-icon-color: var(--cr-input-error-color)` ✅ value / ⚠️ token                                                                                                                                                                |
| **Hover Overlay**                 | *(Not modeled in Figma)*                                                                 | `--cr-input-hover-background-color` → `--cr-hover-background-color` → `--color-sys-state-hover` (not `kColorTextfieldHover` → `kColorSysStateHoverOnSubtle`, which Views uses)                                                            |
| **Caret Color**                   | *(Not modeled in Figma)*                                                                 | `caret-color: --cr-input-focus-color` (`--cr-input-error-color` when invalid)                                                                                                                                                             |
| **Label Typography**              | `desktop/body/five (medium)`: Google Sans Text, 500, `11px` / `16px`, letter-spacing `0` | `#label`: `font-size: 11px; line-height: 16px` (overrides `.cr-form-field-label`'s `.625rem` / `1`), `font-weight: 500`, **`letter-spacing: .4px`** ⚠️. The font family is inherited                                                      |
| **Input Typography**              | `desktop/body/four (regular)`: Google Sans Text, 400, `12px` / **`18px`**                | `#input`: `font-size: var(--cr-input-font-size, 12px)`, **`line-height: 16px`**, `font-weight: inherit`, `font-family: inherit` ⚠️                                                                                                        |
| **Label → Field Gap**             | `--desktop/spacing/8` (`8px`, label `pb`)                                                | `.cr-form-field-label { margin-bottom: 8px }` ✅                                                                                                                                                                                          |
| **Label Trailing Padding**        | `--desktop/spacing/10` (`10px`, label `pr`)                                              | None ⚠️ (minor)                                                                                                                                                                                                                           |
| **Content Horizontal Padding**    | `--desktop/spacing/10` (`10px`)                                                          | `--cr-input-padding-start` / `--cr-input-padding-end`: `10px` ✅                                                                                                                                                                          |
| **Content Vertical Padding**      | `--desktop/spacing/9` (`9px`) + `18px` text line = `36px` field                          | `--cr-input-padding-top` / `--cr-input-padding-bottom`: **`10px`** + `16px` line (`#inner-input-content { height: 16px }`) = `36px` ⚠️ same height, different split                                                                       |
| **Icon ↔ Text Gap**               | `--desktop/spacing/4` (`4px`)                                                            | `#inner-input-content { gap: 4px }` ✅                                                                                                                                                                                                    |
| **Icon Size**                     | `16px` × `16px`                                                                          | `--cr-icon-button-icon-size: 16px` (slotted `<cr-icon-button>`, `24px` hit area, `-8px` start margin). A slotted `<cr-icon>` defaults to `24px` (`--iron-icon-width/height`) unless the consumer overrides it ⚠️                          |
| **Container Corner Radius**       | `--desktop/corner-radius/8` (`8px` top-left / top-right, `0` bottom)                     | `--cr-input-border-radius: 8px 8px 0 0` (hardcoded) ✅                                                                                                                                                                                    |
| **Component Width**               | `219px` (fixed in the kit)                                                               | `width: var(--cr-input-width, 100%)` (fluid)                                                                                                                                                                                              |
| **Underline Weight**              | 1px (Default / Disabled), 2px (Selected / Error)                                         | `#underline-base` 1px; `#underline` 2px (shown for `focused_` / `invalid` / `force-underline`) ✅                                                                                                                                         |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### C++ Views

#### V1. Default background: transparent in Figma, `surface` in code

The Figma `State=Default` / `Selected` frames have no fill. `views::Textfield`
always paints a `kColorTextfieldBackground` (`kColorSysSurface`) round rect
unless the caller sets `SetBackgroundColor()` to something else. On non-surface
containers (e.g. tonal dialogs or the side panel), the field shows a visible
surface-colored box that the Figma component doesn't show.

#### V2. Focus ring geometry

Figma draws the Selected state as a **2px inside border** that replaces the 1px
outline (the field stays 36px). Views keeps the 1px `FocusableBorder` and draws
a separate `FocusRing` layer: a 2px halo centered on the view edge
(`kDefaultHaloInset = -1`, outset disabled), so it extends 1px outside the
bounds. Its color is also contrast-adjusted (`BlendForMinContrast` against the
cascading background), so it may differ slightly from `state-focus-ring`.

#### V3. Selected-text foreground

Figma keeps the selected text `on-surface`. Views uses
`kColorTextfieldSelectionForeground` → `kColorTextSelectionForeground`, which is
the max-contrast color (black/white) against the tonal container. The material
mixer doesn't override it.

#### V4. Typography

- Views uses `ui::kLabelFontSizeDelta` (a 12px platform UI font) and a 16px
  control line height (`kControlHeight`) with 10px vertical padding.
- Figma specifies Google Sans Text 12/18 with 9px padding. The overall 36px
  height matches.

#### V5. Disabled is shared with read-only, and the background is opaque

`GetTextStyle()` and `UpdateDefaultBorder()` treat `GetReadOnly()` the same as
disabled. Figma has no read-only variant. `kColorTextfieldBackgroundDisabled`
pre-composites the translucent `state-disabled-container` over `surface`, so it
is opaque.

#### V6. Figma `Disabled` symbol size

The `State=Disabled` symbol is 34px tall, with a `-2.94%` vertical inset on the
inner frame (rendered ≈36px). This looks like a Figma construction quirk; code
keeps a constant 36px.

### WebUI

#### W1. Error color variable has no backing ColorId

`--cr-input-error-color` reads `--color-textfield-filled-error`. No
`kColorTextfieldFilledError` exists. The defined ID is
`kColorTextfieldFilledForegroundInvalid` → `kColorSysError`, which would map to
`--color-textfield-filled-foreground-invalid`. So the error color always uses
the static `--cr-fallback-color-error` (`#b3261e` light / `#f2b8b5` dark) and
ignores dynamic/themed color. Fix: point the CSS at
`--color-textfield-filled-foreground-invalid`.

#### W2. Placeholder fallback typo

`--cr-input-placeholder-color` falls back to `--cr-fallback-on-surface-subtle`,
which is undefined. The correct name is `--cr-fallback-color-on-surface-subtle`.
On pages that don't load the color-provider CSS, the placeholder color is
invalid.

#### W3. Focused underline token: `state-focus-ring` vs. `primary`

Figma binds the Selected underline to
`--desktop/sys/state-colors/state-focus-ring`. Code uses
`kColorTextfieldFilledUnderlineFocused` → `kColorSysPrimary`. Both are `#0b57d0`
in the light baseline, but they can diverge under theming or in high contrast.
Views uses `kColorSysStateFocusRing` for its ring.

#### W4. Input text uses a legacy color

The value color is `--cr-primary-text-color` → `--color-primary-foreground`
rather than `--color-textfield-foreground` (`kColorTextfieldForeground` →
`kColorSysOnSurface`, which Views uses and which matches Figma's `on-surface`).

#### W5. Disabled container is opaque in code

Figma uses translucent `state-disabled-container` (`#1f1f1f1f`) directly.
`kColorTextfieldBackgroundDisabled` pre-composites it over `kColorSysSurface`
(not over `surface-variant`), so the result is an opaque color.

#### W6. Typography deltas

- The input line height is `16px` in code vs. `18px` (`body-four`) in Figma. The
  code adds 1px to each vertical padding (10 vs. 9) to keep the 36px field
  height.
- The label has `letter-spacing: .4px` (inherited from `.cr-form-field-label`)
  vs. `0` in Figma.
- The font family and weight are inherited from the page.

### Cross-platform

- **Hover**: Views uses `kColorTextfieldHover` (`state-hover-on-subtle`). WebUI
  uses the generic `--color-sys-state-hover`. Neither Figma component models
  hover.
- **Error**: WebUI Figma models Error; Views Figma doesn't, even though
  `views::Textfield::SetInvalid()` exists.
- **Shared color IDs**: Views uses the outlined IDs (`kColorTextfieldOutline*`,
  `kColorTextfieldBackground`). WebUI's default filled style uses
  `kColorTextfieldFilled*`. WebUI `.stroked` uses
  `--color-side-panel-textfield-border`, not `kColorTextfieldOutline`.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### C++ Views

- **States in code that Figma doesn't model**: hover (InkDrop), invalid (red
  border and ring), read-only, placeholder, and dark theme.
- **No label or icon slots**: the Views Figma component is a bare field.
  `views::Textfield` has no built-in label. Callers compose a `views::Label` and
  set the accessible name.
- **Custom borders**: calling `SetBorder()` removes the `FocusRing` entirely.
  Callers that do this lose the Figma `Selected` treatment.

### WebUI

- **States in code that Figma doesn't model**: hover, read-only, the `.stroked`
  style, Focused + Error together (the error color wins), and dark theme.
- **Error message**: the Figma `Error` variant changes only colors. `<cr-input>`
  also renders `error-message` text below the field and reserves its space even
  when valid, unless `--cr-input-error-display: none` is set.
- **Selected naming**: the state is driven by the internal `focused_` attribute
  (not a public API). Use real focus or `force-underline`.
- **Leading icon always shown in Figma**: there is no "no icon" variant, and
  `RHS Content` has no content spec.
- **Underline animation**: code animates the focus underline (`opacity 120ms`,
  `width 180ms ease-out`). Figma has no motion spec.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use `views::Textfield` in native Views UI and `<cr-input>` in WebUI. Use
  `<cr-textarea>` / `views::Textarea` for multi-line input and
  `<cr-search-field>` for search.
- Always provide a visible label. In Views, place a `views::Label` next to the
  field; in WebUI, set `label`. Use placeholder text only for example input.
- Explain errors with text. In WebUI, set `error-message` with `invalid`. In
  Views, use `SetInvalid(true)` plus nearby supporting text or a tooltip.
- Let layouts size the field. Don't hardcode the Figma `219px` width.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **C++ Views (`views::Textfield`)**: `FocusBehavior::ALWAYS`, with a focus ring
  on focus. Set the accessible name through
  `GetViewAccessibility().SetName(...)` or by associating a label. Placeholder
  text isn't a substitute for an accessible name. Invalid state is shown with
  the red border and focus ring.
- **WebUI (`<cr-input>`)**: focus goes to the internal `<input>` (`focus()`,
  `inputElement`, `inputTabindex`). The accessible name comes from `label`
  (`aria-labelledby="label"` when `aria-label` isn't set). Otherwise
  `aria-label` falls back from `ariaLabel` to `label` to `placeholder`. When
  invalid, `aria-invalid="true"` and `aria-errormessage` point to `#error`, and
  `#error` gets `role="alert"` with `aria-live="assertive"`.
- Error state must not rely on color alone on either platform.

### 3. Icon Usage Guidelines

- **Views**: the Figma component has no icons. If an icon is needed, compose it
  outside the `views::Textfield` (or use `SetExtraInsets()` to make room for an
  overlaid view).
- **WebUI**: use 16px icons in the `inline-prefix` / `inline-suffix` slots. Icon
  color is set automatically through `--cr-icon-color`. When slotting a bare
  `<cr-icon>`, set `--iron-icon-width` / `--iron-icon-height: 16px` (it defaults
  to 24px).

______________________________________________________________________

## 7. Verification Status

### C++ Views — `Text Fields (Views)`

- ✅ **Matches**:
  - Default outline (`kColorSysNeutralOutline`, 1px)
  - Focus ring color source (`kColorSysStateFocusRing`) and 2px thickness
  - Selection highlight (`kColorSysTonalContainer`)
  - Text color (`kColorSysOnSurface`)
  - Disabled text (`kColorSysStateDisabled`) and transparent disabled outline
  - 8px corner radius (`kTextfieldRadius` → `kSmall`)
  - 10px horizontal padding
  - 36dp height
- ⚠️ **Mismatches**:
  - Default background is `surface` in code but has no fill in Figma (V1)
  - Focus ring straddles the edge instead of sitting inside (V2)
  - Selected-text foreground is max-contrast instead of `on-surface` (V3)
  - Typography is the 12px platform font with a 16px line, vs. Google Sans Text
    12/18 (V4)
  - Vertical padding is 10 vs. 9 (V4)
  - Disabled background is opaque (V5)
  - The Figma Disabled symbol is 34px (V6)
- ❓ **Not checked**:
  - No pixel rendering or screenshot comparison
  - Dark mode and high contrast (`ui_color_mixer.cc` legacy mappings were not
    analyzed beyond listing)
  - The platform-specific `kLabelFontSizeDelta` result on each OS, and whether
    Google Sans Text is applied
  - Chrome-layer `ChromeLayoutProvider` overrides of the textfield distance
    metrics (none found for these two metrics, but not exhaustively checked)
  - Exact `FocusRing` corner radius when drawn around an 8px path

### WebUI — `Text Fields (WebUI)`

- ✅ **Matches**:
  - Container background, default underline, disabled underline, text and icon
    colors
  - Label color, size and weight, and the 8px label gap
  - 10px horizontal padding, 4px icon gap, `8px 8px 0 0` radius, 2px focus/error
    underline weight
- ⚠️ **Mismatches**:
  - The `--color-textfield-filled-error` ColorId is missing (W1)
  - Placeholder fallback typo (W2)
  - Focus underline uses `primary` instead of `state-focus-ring` (W3)
  - Legacy text color variable (W4)
  - Opaque disabled background (W5)
  - Line height 16 vs. 18, vertical padding 10 vs. 9, and label letter-spacing
    .4px (W6)
  - A slotted `<cr-icon>` defaults to 24px
  - The error message, hover and `.stroked` style aren't in Figma
- ❓ **Not checked**:
  - No pixel rendering
  - Dark mode and forced-colors values beyond the CSS fallbacks
  - Whether every `kColorTextfield*` ID is exposed as a `--color-*` CSS variable
    (assumed, because other WebUI code uses these variables)
