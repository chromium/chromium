# Radio Buttons Spec (Desktop)

This document maps the specifications of the **Radio Buttons** (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

**Radio Buttons** are mutually exclusive selection controls on Chrome Desktop
that let users choose exactly one option from a related set. The Figma component
models a standalone 20px indicator with `Selected?` (Yes | No) and four
interactive states (`Default`, `Hovered`, `Pressed`, `Disabled`); in code it is
implemented by `views::RadioButton` (C++ Views) and `<cr-radio-button>` (WebUI,
grouped via `<cr-radio-group>`), both of which add an integrated label and
keyboard focus ring.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                             | C++ Views (Desktop)                                                                      | WebUI (Desktop)                                                                                                                                                                                                                                                                          |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Radio Buttons` (component set, 8 variants)                                                                                                                                 | `views::RadioButton`                                                                     | `<cr-radio-button>`<br>`<cr-radio-group>`                                                                                                                                                                                                                                                |
| **Source Files**   | [Figma Link: `Radio Buttons` (`280:26337`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=280-26337&t=Bwx9DgfkzVhJNNVX-11) | [ui/views/controls/button/radio_button.h](//src/ui/views/controls/button/radio_button.h) | [ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts](//src/ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button.ts)<br>[ui/webui/resources/cr_elements/cr_radio_group/cr_radio_group.ts](//src/ui/webui/resources/cr_elements/cr_radio_group/cr_radio_group.ts) |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: `Selected?` (Yes | No) × `State` (Default | Hovered |
Pressed | Disabled)

| Feature / Variant          | Figma Component                       | C++ Views (Desktop)                                                                                                                 | WebUI (Desktop)                                                                                             |
| :------------------------- | :------------------------------------ | :---------------------------------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| **Selected**               | `Selected?=Yes`                       | `RadioButton::SetChecked(true)` → `kRadioButtonCheckedIcon` (`kRadioButtonActiveOldIcon` when rounded icons are disabled)           | `<cr-radio-button checked>` (`aria-checked="true"`, `.disc` scaled to `0.5`)                                |
| **Unselected**             | `Selected?=No`                        | `RadioButton::SetChecked(false)` (default) → `kCircleIcon` (`kRadioButtonNormalOldIcon` when rounded icons are disabled)            | `<cr-radio-button>` (`checked` = `false`, `aria-checked="false"`, `.disc` scaled to `0`)                    |
| **Default State**          | `State=Default`                       | `Button::STATE_NORMAL`                                                                                                              | `:host(:not([disabled]))`                                                                                   |
| **Hovered State**          | `State=Hovered`                       | `Button::STATE_HOVERED`: `InkDropHighlight` (`CircleLayerDelegate`) on `image_container_view()`                                     | `#button:hover #overlay` (32px circle, `--cr-hover-background-color`)                                       |
| **Pressed State**          | `State=Pressed`                       | `Button::STATE_PRESSED`: `InkDrop::CreateSquareRipple()` centered on the icon (`kCheckboxInkDropSize` 24×24)                        | `<cr-ripple class="circle" recenters>` created by `CrRippleMixin` inside `.disc-wrapper`                    |
| **Disabled State**         | `State=Disabled`                      | `View::SetEnabled(false)` → `Button::STATE_DISABLED`                                                                                | `<cr-radio-button disabled>` (`aria-disabled="true"`, `pointer-events: none`)                               |
| **Focused State**          | *(Not modeled in Figma)*              | `views::FocusRing` using `RadioButton::GetFocusRingPath()` (16dp-radius circle around icon)                                         | `#button:focus-visible #overlay` (`border: 2px solid var(--cr-focus-outline-color)`)                        |
| **Label / Content**        | *(Not modeled; standalone indicator)* | `RadioButton(const std::u16string& label, int group_id)` (label inherited from `views::Checkbox`/`LabelButton`)                     | `label` property (`#label`), default `<slot>` (`#slotted-content`), `hide-label-text`, `.label-first` class |
| **Grouping / Exclusivity** | *(Not modeled)*                       | `group_id` / `View::SetGroup()`; `SetChecked(true)` unchecks peers in the group (optionally scoped by a cascading radio-group view) | `<cr-radio-group selected="...">` with `name` on each `<cr-radio-button>`                                   |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute           | Figma Design Token                                                                                  | C++ Views (Desktop)                                                                                                                  | WebUI (Desktop)                                                                                                                              |
| :------------------------- | :-------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------- |
| **Selected Ring + Dot**    | `--desktop/sys/primary-colors/primary` (`#0b57d0`)                                                  | `ui::kColorCheckboxContainer` → `kColorSysPrimary` (via `Checkbox::GetIconImageColor()`)                                             | `--cr-radio-button-checked-color` → `--color-radio-button-foreground-checked` (fallback `--cr-fallback-color-primary`)                       |
| **Unselected Ring**        | `--desktop/sys/outline-colors/outline` (`#747775`)                                                  | `ui::kColorCheckboxOutline` → `kColorSysOutline`                                                                                     | `--cr-radio-button-unchecked-color` → `--color-radio-button-foreground-unchecked` (fallback `--cr-fallback-color-outline`)                   |
| **Disabled Ring + Dot**    | `--desktop/sys/state-colors/state-disabled` (`#1f1f1f61`, ~38%)                                     | `ui::kColorCheckboxContainerDisabled` / `ui::kColorCheckboxOutlineDisabled` → `kColorSysStateDisabledContainer` (~12% alpha) ⚠️      | `--color-radio-foreground-disabled` (undefined name ⚠️) → fallback `--cr-fallback-color-disabled-foreground` (on-surface @ `.38`)            |
| **Hover State Layer**      | `--desktop/sys/state-colors/state-hover-on-subtle` (`#1f1f1f0f`)                                    | `InkDropHighlight` with base color `ui::kColorCheckboxForegroundUnchecked` → `kColorSecondaryForeground` at default ink-drop opacity | `--cr-hover-background-color` → `--color-sys-state-hover` (fallback on-surface @ `.08`) ⚠️                                                   |
| **Pressed State Layer**    | `state-hover-on-subtle` + `--desktop/sys/state-colors/state-ripple-neutral-on-subtle` (`#1f1f1f14`) | Ink-drop ripple, base color `kColorSecondaryForeground` at default ripple opacity                                                    | `--cr-radio-button-(un)checked-ripple-color` → `--cr-active-background-color` → `--color-sys-state-pressed` (fallback on-surface @ `.12`) ⚠️ |
| **Focus Ring**             | *(Not modeled)*                                                                                     | `views::FocusRing` default color (`GetCascadingAccentColor()`, i.e. the cascading accent / focus color)                              | `--cr-focus-outline-color` → `--color-sys-state-focus-ring` (fallback `--cr-fallback-color-primary`)                                         |
| **Indicator Box Size**     | `20×20px`                                                                                           | `kRadioButtonIconDipSizeCr2023` = `20` dip                                                                                           | `--cr-radio-button-size: 16px` ⚠️                                                                                                            |
| **Ring Stroke / Dot**      | Material `radio_button_checked` glyph scaled to 20px (ring ≈ 16.7px, stroke ≈ 1.7px, dot ≈ 8.3px)   | Vector icons `radio_button_checked.icon` / `circle.icon` rendered at 20dip                                                           | `.disc-border` `2px` solid border on 16px circle; `.disc` scaled to `0.5` (8px)                                                              |
| **State Layer (Ink) Size** | `32×32px` mask (in a `36×36px` "States" frame, offset −8px)                                         | Highlight radius = `InkDropHost::GetLargeSize(24×24).width() / 2` (≈16dp → 32dp circle); ripple from 24×24 square                    | `--cr-radio-button-ink-size: 32px`                                                                                                           |
| **Shape / Corner Radius**  | `--desktop/corner-radius/fully-rounded` (`999`)                                                     | Circular vector icon; `CircleLayerDelegate` highlight; `SkPath::Circle` focus ring                                                   | `border-radius: 50%` on `.disc-border`, `.disc`, `#overlay`, `cr-ripple`                                                                     |
| **Label Spacing / Insets** | *(Not modeled)*                                                                                     | `INSETS_CHECKBOX_RADIO_BUTTON` = `gfx::Insets::VH(5, 6)`                                                                             | `gap: var(--cr-radio-button-label-spacing, 20px)`                                                                                            |
| **Disabled Label**         | *(Not modeled)*                                                                                     | `LabelButton` disabled text color                                                                                                    | `#labelWrapper { opacity: var(--cr-disabled-opacity) }` (`.38`)                                                                              |

______________________________________________________________________

## 4. Visual & Token Discrepancies

- **Disabled color (Views)**: Figma uses `state-disabled` (on-surface ~38%
  alpha), but Views reuses checkbox colors and resolves disabled to
  `kColorSysStateDisabledContainer` (on-surface ~12% alpha), so disabled radio
  buttons render considerably lighter than designed. The dedicated
  `ui::kColorRadioButtonForegroundDisabled` (also mapped to
  `kColorSysStateDisabledContainer`) and the other
  `kColorRadioButtonForeground*` ids are not used by `views::RadioButton` (only
  by menu radio items).
- **Disabled color (WebUI)**: `cr_radio_button_style_lit.css` references
  `--color-radio-foreground-disabled`, which does not match the generated name
  for `kColorRadioButtonForegroundDisabled`
  (`--color-radio-button-foreground-disabled`). It therefore always falls back
  to `--cr-fallback-color-disabled-foreground` (on-surface @ `.38`), which
  happens to match Figma's `state-disabled`.
- **Hover/pressed tokens**: Figma specifies the "on-subtle" state tokens
  (`state-hover-on-subtle` 6%, `state-ripple-neutral-on-subtle` 8%, stacked on
  press). WebUI uses the generic `--color-sys-state-hover` /
  `--color-sys-state-pressed` (8% / 12% fallbacks);
  `--cr-hover-on-subtle-background-color` and
  `--cr-active-neutral-on-subtle-background-color` exist but are unused. Views
  derives ink-drop colors from `kColorSecondaryForeground` (on-surface-subtle)
  with default ink-drop opacities rather than the sys state tokens. In Figma,
  the hover layer is neutral for both selected and unselected states; WebUI's
  dark-mode legacy path (`html:not([webui-refresh-2026])`) uses a blue ripple
  for the checked state.
- **Indicator size**: Figma and Views use a 20px indicator; WebUI renders a 16px
  disc with a 2px border (8px dot). The WebUI glyph is therefore slightly
  smaller with a thicker relative stroke than the Material glyph in Figma.
- **Dark mode**: Figma's variable values shown are light-mode only. Both code
  paths resolve dark-mode values through the color pipeline (`kColorSys*` /
  `--color-sys-*`), with additional legacy hardcoded Google-palette overrides in
  WebUI when `webui-refresh-2026` is not set.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **No focus variant in Figma**: Both implementations draw a focus ring (Views
  16dp-radius circle; WebUI 2px border on the 32px overlay), but no
  `State=Focused` variant exists to validate color/thickness.
- **No label in Figma**: The Figma component is the indicator only. Both code
  components bundle the label (Views: `LabelButton` label; WebUI: `label`
  property + slot, `.label-first` reverses order, `hide-label-text` visually
  hides it). Label typography and indicator-to-label spacing (Views insets
  `5/6`, WebUI `20px` gap) are unspecified in this Figma component.
- **Pressed state composition**: Figma stacks hover + ripple overlays as a
  static state; code shows an animated ripple (Views square ripple from a 24×24
  source clipped to a circle highlight; WebUI `cr-ripple` circle that
  recenters).
- **Grouping behavior**: Group exclusivity, roving focus and arrow-key
  navigation are code-only concerns (Views `group_id` +
  `IsGroupFocusTraversable()` returning `false`, selection on focus; WebUI
  `<cr-radio-group>` with `ArrowUp/Down/Left/Right` handling, RTL-aware).
- **Selection-on-focus (Views)**: `RadioButton::OnFocus()` checks the button
  when it gains focus (except for accessibility focus actions), so arrow-key
  traversal changes selection; there is no Figma guidance for this.
- **Disabled hit-testing (WebUI)**: `:host([disabled])` sets
  `pointer-events: none`, so disabled radios also suppress hover/tooltip on the
  label.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use radio buttons when the user must pick exactly one option from a small,
  visible set (typically 2–5); use a dropdown for longer lists and a
  checkbox/switch for independent on/off choices.
- Always present radio buttons in a group with one option selected by default
  where a sensible default exists; never use a single standalone radio button.
- Keep labels short, parallel in structure, and sentence case; the whole row
  (indicator + label) should be clickable.
- Stack options vertically for scannability; use `.label-first` (WebUI) only
  where layouts require trailing indicators (e.g. settings rows).

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: `#button` has `role="radio"`, `aria-checked`, `aria-disabled`,
  `aria-labelledby="label"` and `aria-describedby="slotted-content"`; wrap
  buttons in `<cr-radio-group>` (`role="radiogroup"`) to get roving `tabindex`,
  arrow-key navigation (RTL-aware), and exclusive selection. Provide an
  accessible group label (e.g. `aria-label` / `aria-labelledby` on the group).
  Use `hide-label-text` rather than omitting `label` when the label is visually
  redundant.
- **C++ Views**: `views::RadioButton` sets `ax::mojom::Role::kRadioButton` and
  checked state automatically. Assign the same `group_id` to all buttons in a
  set (or use a cascading radio-group view to scope the group) so Tab focuses
  only the selected button and arrow keys move/select within the group. Always
  provide a label or accessible name.
- Both platforms provide a visible focus ring; do not suppress it. Ensure the
  32px state layer/focus target is not clipped by parent containers.

### 3. Icon Usage Guidelines

- The indicator glyph is fixed (Material `radio_button_checked` / circle); do
  not substitute custom icons. Views uses `kRadioButtonCheckedIcon` and
  `kCircleIcon` from `//ui/views/vector_icons/`, WebUI draws the indicator with
  CSS.
- Do not place additional icons inside the indicator; if an option needs an
  icon, include it in the label/slotted content (WebUI) or a custom row layout.
