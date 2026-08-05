# Component Spec: Tooltip

This specification document outlines the mapping, design tokens, styling variants, and interactive states of the **Tooltip** component across Figma, C++ Views, and WebUI (Web Frontend).

---

## Overview

A **Tooltip** is a small, brief pop-up bubble that displays helpful, supplementary textual information when a user hovers over, focuses, or long-presses an anchored UI element. It contains static text only, handles no active inputs itself, and disappears automatically after a delay or upon mouse exit/blur of the parent element.

---

## 1. Component Metadata & Source Files

| Feature | Figma Component | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Component Name** | `Tooltip` | `views::corewm::TooltipViewAura` | `<cr-tooltip>` |
| **Source Files** | [Figma Link: `28535:534`](https://www.figma.com/design/ZRB6863VRSstVNLN6WI3Pt/CDDS-Design-Kit---Settings--Chrome-?node-id=28535-534&m=dev) | [ui/views/corewm/tooltip_view_aura.h](//src/ui/views/corewm/tooltip_view_aura.h) | [ui/webui/resources/cr_elements/cr_tooltip/cr_tooltip.ts](//src/ui/webui/resources/cr_elements/cr_tooltip/cr_tooltip.ts) |

---

## 2. Styling, Variants & Features (Layout & Style)

Both implementations only offer a single visual variant (standard bubble) and support wrapping text.

| Feature / Variant | Figma | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Standard Bubble** | Default appearance with dark transparent scrim. | Renders as a native widget using `views::corewm::TooltipViewAura` with a 1px border. | Renders as custom element `<cr-tooltip>` wrapping an internal `#tooltip` div. |
| **Text Wrapping** | Word-wrapped based on max-width constraint. | Wrapping handled via `gfx::RenderText` set to multiline (`SetMultiline(true)`). | Handled by standard browser block-level text flow and CSS width constraints. |
| **Positioning** | Top, bottom, left, right | Managed by `views::corewm::TooltipAura::GetTooltipBounds`, positioning relative to cursor. | Managed by the `position` attribute (`top`, `bottom`, `left`, `right`), calculating offsets dynamically. |

---

## 3. Component States

Tooltips do not accept user input or mouse interaction. They only transition between visible (showing) and hidden states.

| State | Figma State Property | C++ views::View / Widget | WebUI CSS classes / pseudo-classes |
| :--- | :--- | :--- | :--- |
| **Hidden** | Not visible | Widget is hidden/destroyed | `hidden` attribute is set on inner `#tooltip` |
| **Showing (Transition)** | N/A | Instantly visible | `.fade-in-animation` class applied (500ms delay by default) |
| **Visible** | Default state | Widget is visible (`widget_->Show()`) | Fully visible (opacity matches `--paper-tooltip-opacity` or `0.9`) |
| **Hiding (Transition)** | N/A | Instantly hidden / Widget destroyed | `.fade-out-animation` class applied (500ms duration) |

---

## 4. Design Token Comparison (Side-by-Side)

| Property / Token | Figma Design | C++ Views (Desktop) | WebUI (Web Frontend) |
| :--- | :--- | :--- | :--- |
| **Background Color** | `var(--desktop/sys/state-colors/state-scrim)` (`rgba(0,0,0,0.6)`) | `ui::kColorTooltipBackground` (maps to `kColorPrimaryBackground` at 80% opacity) | `var(--paper-tooltip-background, #616161)` |
| **Text Color** | `var(--desktop/sys/static-colors/white)` (`#ffffff`) | `ui::kColorTooltipForeground` (maps to `kColorPrimaryForeground` at 87% opacity) | `var(--paper-tooltip-text-color, white)` |
| **Border / Stroke** | None | 1px Solid `ui::kColorTooltipForeground` | None |
| **Corner Radius** | `2px` | `0px` (square corners, except `6px` on ChromeOS/Ash) | `2px` (`border-radius: 2px`) |
| **Padding** | Top/Bottom: `10px`<br>Left/Right: `8px` | `TLBR(4, 8, 5, 8)` insets (active: `3px` top, `7px` left, `4px` bottom, `7px` right inside the 1px border) | `8px` (all sides) |
| **Typography** | Font Family: `typeface/body` (Roboto)<br>Weight: `500` (Medium)<br>Size: `13px`<br>Line Height: `20px` | Font list determined by system font.<br>Weight: Regular<br>Size: System default (varies, typ. `11-12px`) | Font Family: System default<br>Weight: Regular<br>Size: `10px`<br>Line Height: `1` |
| **Max Width** | `216px` | Dynamic / Capped at standard max width (typically `800px` or half of screen width) | Centered/fit to parent bounds or manually configured |

---

## 5. Architectural & Implementation Gaps

1. **Corner Radius Discrepancy**:
   - Standard desktop Views tooltips have completely square corners (`0px` corner radius) and a 1px border. ChromeOS (Ash) overrides this to `6px`. Neither matches Figma's requirement of `2px`.
   - WebUI matches Figma with a `2px` border-radius.
2. **Typography Scaling**:
   - The typography size in WebUI (`10px` with `line-height: 1`) is extremely small compared to Figma's spec (`13px` body large text with `20px` line-height). This causes WebUI tooltips to appear much more compact and harder to read.
   - C++ Views uses system default fonts, which are generally smaller and thinner than Figma's Roboto Medium (`500` weight).
3. **Background Translucency & Styling**:
   - Figma uses a 60% opacity dark scrim (`rgba(0,0,0,0.6)`).
   - WebUI uses an opaque solid grey (`#616161`) by default, unless overridden.
   - C++ Views blends the color with `0xCC` (80% opacity) but adds a 1px stroke border, which does not exist in the design.
4. **Padding Variations**:
   - Figma uses asymmetric vertical padding (`10px` vertical vs `8px` horizontal).
   - WebUI uses uniform `8px` padding.
   - Views uses `TLBR(4, 8, 5, 8)`, leading to asymmetric padding.

---

## 6. Styling, Variants, Features and States Mismatches

- **Display Delays & Animations**:
  - WebUI `<cr-tooltip>` has a built-in fade-in animation delay of `500ms` and a fade-out delay of `600ms` to prevent accidental triggers. It also runs a CSS transition animation.
  - C++ Views shows the widget instantly after the `TooltipController` timeout triggers, without any visual fade-in/out transition.
- **Dynamic Sizing vs Figma Constraints**:
  - Figma specifies a fixed max-width and standard width of `216px` for the tooltip.
  - In both codebases, tooltips are dynamic: they wrap and size to the length of the string content up to a much larger bounds limit (e.g. `800px` in Views) to avoid rendering overly tall/narrow tooltips.

---

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices
- **Supplementary Info Only**: Tooltips should only provide auxiliary context. Critical path instructions, error messages, or interactive actions must never be put inside a tooltip.
- **Brief Text**: Tooltips should be concise (normally 1-2 short sentences maximum).
- **Labeling Conventions**: Keep sentences clear, using active verbs and direct language. Do not repeat the anchor text.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)
- **WebUI Accessibility**:
  - `<cr-tooltip>` is announced as `role="tooltip"`.
  - It automatically hooks pointerenter/leave, focus/blur, and click events on its target element to toggle visibility.
- **C++ Views Accessibility**:
  - `views::corewm::TooltipViewAura` sets the accessibility role to `ax::mojom::Role::kTooltip`.
  - The accessibility name is set directly from the rendered text string.
  - Focus rings are not painted on the tooltip since it is not focusable.

---

## 8. Inheritance Structure

*   **C++ Views (Desktop)**:
    ```
    views::View (Base visual unit)
       └── views::corewm::TooltipViewAura (Renders tooltip text using gfx::RenderText)
    ```
*   **WebUI (Web Frontend)**:
    ```
    HTMLElement (Browser base class)
       └── CrLitElement (Custom Lit element class wrapper)
              └── CrTooltipElement (Handles Lit lifecycle and shows/hides tooltip div)
    ```
