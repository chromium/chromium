# Dialog Bubble Spec (Desktop)

This document maps the specifications of the **Dialog Bubble** (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

The **Dialog Bubble** ("Bubble Template" in Figma) is the anchored, non-modal
dialog surface on Chrome Desktop: a `12px`-radius elevated `surface` card with a
header (title, optional subtitle, optional back arrow, close button), a
free-form content area and a trailing button bar. Figma models three widths
(Small `320` / Medium `448` / Large `512`) × three layouts (Basic, Subhead +
Divider, Illustration header); it is implemented by
`views::BubbleDialogDelegate(View)` rendered inside `views::BubbleFrameView`
(C++ Views only — the Figma doc marks **WebUI as not supported**).

______________________________________________________________________

## 1. Component Metadata & Source Files

> [!NOTE] The Figma node is a documentation frame (`Key UIs / Dialog Bubble`).
> The bubble itself is **not** a Figma `COMPONENT`/`COMPONENT_SET`; each example
> is a plain `FRAME` named `Bubble` built from the `Bubble Header`, `Buttons`
> and `Icon Buttons` component instances. The frames below are the frame-based
> representations of the component.

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | C++ Views (Desktop)                                                                                                                                                                      | WebUI (Desktop)                         |
| :----------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------------------------------- |
| **Component Name** | `Bubble` (frame-based "Bubble Template", 9 example frames)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | `views::BubbleDialogDelegateView`<br>`views::BubbleDialogDelegate`<br>`views::BubbleFrameView`                                                                                           | *(Not supported — Figma marks WEBUI ✗)* |
| **Source Files**   | [Figma Link: `Key UIs / Dialog Bubble` (`18300:625`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=18300-625)<br>[Figma Link: `Bubble` — Basic, Small (`18300:630`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=18300-630)<br>[Figma Link: `Bubble` — Subhead + Divider, Small (`18300:669`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=18300-669)<br>[Figma Link: `Bubble` — Illustration, Small (`18300:714`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/Chrome-Desktop--Core--Design-Kit?node-id=18300-714) | [ui/views/bubble/bubble_dialog_delegate_view.h](//src/ui/views/bubble/bubble_dialog_delegate_view.h)<br>[ui/views/bubble/bubble_frame_view.h](//src/ui/views/bubble/bubble_frame_view.h) | N/A                                     |

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes: the bubble is frame-based, so there are no Figma variant
properties. The documentation frame lays out two implicit axes:

- `Bubble` (frame): **Size** (Small `320` | Medium `448` | Large `512`) ×
  **Layout** (Basic | Subhead + Back Arrow + Divider | Illustration Header).
- `Bubble Header` (instance, sub-component): Title only | Back arrow + Title +
  Subhead (both with trailing `close` `Icon Buttons`).
- `Buttons` (instance): `Variant` (Primary | Tonal | Outlined) × `Icons` (None)
  × `State` (Default) — see [Buttons spec](./Buttons_Spec_Desktop.md).

| Feature / Variant             | Figma Component                                         | C++ Views (Desktop)                                                                                                                                                         | WebUI (Desktop) |
| :---------------------------- | :------------------------------------------------------ | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------------- |
| **Small width**               | `SMALL (320DP)` row                                     | `LayoutProvider::kSmallDialogWidth = 320` (`DISTANCE_BUBBLE_PREFERRED_WIDTH`); `ChromeLayoutProvider::GetSnappedDialogWidth()` snap point                                   | N/A             |
| **Medium width**              | `MEDIUM (448DP)` row                                    | `kMediumDialogWidth = 448` (`DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH`, `DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH`); snap point                                              | N/A             |
| **Large width**               | `LARGE (512DP)` row                                     | `kLargeDialogWidth = 512` (`DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH`); snap point. Explicit widths via `DialogDelegate::set_fixed_width()`                              | N/A             |
| **Title**                     | `Bubble Header` → `Heading text` ("Bubble title")       | `WidgetDelegate::SetTitle()` → `BubbleFrameView::default_title_` (`CreateDefaultTitleLabel()`, `STYLE_HEADLINE_4`)                                                          | N/A             |
| **Subhead**                   | `Bubble Header` second line ("Subhead if necessary")    | `BubbleDialogDelegate::SetSubtitle()` → `BubbleFrameView::subtitle_` (`CONTEXT_LABEL`, `STYLE_SECONDARY`)                                                                   | N/A             |
| **Close button**              | `Close` → `Icon Buttons` (`close`, 20px)                | `WidgetDelegate::SetShowCloseButton(true)` → `BubbleFrameView::CreateCloseButton()` (`vector_icons::kCloseIcon`). *Default is hidden* (`SetShowCloseButton(false)` in ctor) | N/A             |
| **Back arrow**                | `Back Arrow` → `Icon Buttons` (`arrow_back`, 20px)      | *No built-in support* — requires a custom title view (`BubbleFrameView::SetTitleView()`) or content                                                                         | N/A             |
| **Divider under header**      | `Divider` line (full-bleed, `component-colors/divider`) | *No built-in support* — client adds a `views::Separator` to contents                                                                                                        | N/A             |
| **Illustration header**       | `Illustration` frame (120px, `tonal-container`)         | `BubbleFrameView::SetHeaderView()` (close button overlaps header, `ButtonsPositioning::kOnFrameEdge`)                                                                       | N/A             |
| **Content area**              | `Content Area` slot                                     | Contents view of `BubbleDialogDelegateView` (`DialogDelegate::set_margins()`)                                                                                               | N/A             |
| **Primary action (trailing)** | `Buttons` `Variant=Primary`                             | OK button (`ui::mojom::DialogButton::kOk`); default style `ui::ButtonStyle::kProminent` when default button                                                                 | N/A             |
| **Secondary action**          | `Buttons` `Variant=Tonal`                               | Cancel button (`DialogButton::kCancel`); default style `ui::ButtonStyle::kTonal` (`DialogDelegate::GetDialogButtonStyle()`)                                                 | N/A             |
| **Leading action**            | `Buttons` `Variant=Outlined` (left-aligned)             | `DialogDelegate::SetExtraView()` (e.g. `views::MdTextButton`, `ui::ButtonStyle::kDefault`)                                                                                  | N/A             |
| **Button order**              | Tonal then Primary (Primary trailing)                   | `PlatformStyle::kIsOkButtonLeading` (`true` on Windows → OK left of Cancel)                                                                                                 | N/A             |
| **Footnote**                  | *(Not modeled in Figma)*                                | `BubbleFrameView::SetFootnoteView()` (`kColorBubbleFooterBackground`)                                                                                                       | N/A             |
| **Title icon / main image**   | *(Not modeled in Figma)*                                | `WidgetDelegate::SetIcon()` + `ShowIcon`; `BubbleDialogDelegate::SetMainImage()`                                                                                            | N/A             |
| **Minimize / progress**       | *(Not modeled in Figma)*                                | `BubbleFrameView` minimize button (`CanMinimize()`), `SetProgress()`                                                                                                        | N/A             |
| **Arrow / anchoring**         | *(Not modeled in Figma)*                                | `BubbleBorder::Arrow`, `SetAnchorView()`, `SetDisplayVisibleArrow()`                                                                                                        | N/A             |
| **Dark theme**                | *(Not modeled in Figma)*                                | Resolved dynamically via `ui::ColorProvider`                                                                                                                                | N/A             |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                                                                                            | C++ Views (Desktop)                                                                                                                                                                   | WebUI (Desktop) |
| :------------------------------- | :---------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :-------------- |
| **Container Background**         | `--desktop/sys/surface-colors/surface` (`#ffffff`)                                                                            | `ui::kColorBubbleBackground` (→ `kColorSysSurface`) via `BubbleDialogDelegate::SetBackgroundColor()` default                                                                          | N/A             |
| **Corner Radius**                | `--desktop/corner-radius/12` (`12px`)                                                                                         | `DialogDelegate::GetCornerRadius()` → `ShapeContextTokens::kDialogRadius` → `ShapeSysTokens::kMediumSmall` = `12` (`0` without compositing; `2` for Mac window-modal)                 | N/A             |
| **Elevation / Shadow**           | `desktop/elevation/3`: key `0 4 8 3` `#00000026` (15%) + ambient `0 1 3 0` `#0000004D` (30%)                                  | Default (no elevation): `(0,2)` blur `8` `kColorBubbleBorderShadowSmall` (grey800 @ `0x33`) + `(0,2)` blur `12` `kColorBubbleBorderShadowLarge` (@ `0x1A`). Mac: `NO_SHADOW` (native) | N/A             |
| **Border Stroke**                | *(None in Figma)*                                                                                                             | `1px` `ui::kColorBubbleBorder` (→ `kColorSysSurfaceVariant`) drawn when shadow present & no custom elevation                                                                          | N/A             |
| **Title Text**                   | `desktop/headline/four`: Google Sans Medium `16/24`, `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                    | `STYLE_HEADLINE_4` (16px Medium, line height `24`); color `kColorLabelForeground` → `kColorPrimaryForeground` → `kColorSysOnSurface`                                                  | N/A             |
| **Subhead Text**                 | `desktop/body/four (regular)`: Google Sans Text Regular `12/18`, `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`) | `CONTEXT_LABEL` + `STYLE_SECONDARY` (12px regular, font-height line height); `kColorLabelForegroundSecondary` → `kColorSecondaryForeground` → `kColorSysOnSurfaceSubtle`              | N/A             |
| **Close / Back Icon**            | `close` / `arrow_back` 20px, `--desktop/sys/surface-colors/on-surface-subtle` (`#474747`)                                     | `kCloseIcon` (`CANVAS_DIMENSIONS, 20`), `ui::kColorIcon` → `kColorSysOnSurfaceSubtle`; hovered `kColorIconHovered`                                                                    | N/A             |
| **Icon Button Hit / State Size** | `28px` state layer (`-4px` around 20px icon), `fully-rounded`                                                                 | `INSETS_VECTOR_IMAGE_BUTTON` = `4` → `28dp` button; `InstallCircleHighlightPathGenerator()`                                                                                           | N/A             |
| **Header Padding**               | `--desktop/spacing/20` top & sides; `--desktop/spacing/8` header bottom                                                       | `INSETS_DIALOG_TITLE` → `TLBR(20, 20, 0, 20)` (Chrome `INSETS_DIALOG` = `VH(20,20)`)                                                                                                  | N/A             |
| **Title ↔ Close gap**            | `--desktop/spacing/8`                                                                                                         | `DISTANCE_RELATED_LABEL_HORIZONTAL` = `12` (button-area left padding)                                                                                                                 | N/A             |
| **Back arrow ↔ Title gap**       | `--desktop/spacing/6` (+ `--desktop/spacing/2` top offset on arrow)                                                           | *No built-in back arrow*                                                                                                                                                              | N/A             |
| **Title → Content gap**          | `8px` (header `pb-8`)                                                                                                         | Content top margin `DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT` = `8` (`…_TOP_CONTROL` = `16`)                                                                                           | N/A             |
| **Content Side Padding**         | `--desktop/spacing/20`                                                                                                        | `INSETS_DIALOG` left/right = `20`                                                                                                                                                     | N/A             |
| **Content → Buttons gap**        | `16px` (content `pb-8` + button bar `pt-8`)                                                                                   | Content bottom margin `DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT` = `16` (`…_BOTTOM_CONTROL` = `24`); `INSETS_DIALOG_BUTTON_ROW` top = `0`                                           | N/A             |
| **Button Bar Bottom / Sides**    | `--desktop/spacing/20`                                                                                                        | `INSETS_DIALOG_BUTTON_ROW` → `TLBR(0, 20, 20, 20)`                                                                                                                                    | N/A             |
| **Gap between buttons**          | `--desktop/spacing/8`                                                                                                         | `DISTANCE_RELATED_BUTTON_HORIZONTAL` = `8` (OK↔Cancel, and minimum Extra↔Cancel)                                                                                                      | N/A             |
| **Divider Color**                | `--desktop/sys/component-colors/divider` (`#d3e3fd`)                                                                          | `views::Separator` default `ui::kColorSeparator` → `kColorSysDivider` (`kColorRefPrimary90` light)                                                                                    | N/A             |
| **Divider Spacing**              | `8px` above & below the divider (wrapper `py-8`), full-bleed `0px` side inset                                                 | *Client-defined*                                                                                                                                                                      | N/A             |
| **Illustration Header**          | `120px` tall, `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`), clipped to top corners                            | `SetHeaderView()` (height = `header_view->GetHeightForWidth()`); client-view layer clipped via `GetClientCornerRadii()`                                                               | N/A             |
| **Close on Illustration**        | `20px` from top & right edge                                                                                                  | `ButtonsPositioning::kOnFrameEdge`: `DISTANCE_CLOSE_BUTTON_MARGIN` = `20`                                                                                                             | N/A             |
| **Title below Illustration**     | `pt-8`, `px-20`, `pb-8`                                                                                                       | `title_margins_.top()` (`20`) added below `header_height`                                                                                                                             | N/A             |
| **Button Typography / Colors**   | `desktop/special/button` 13/20 Medium; Primary/Tonal/Outlined tokens                                                          | See [Buttons_Spec_Desktop.md](./Buttons_Spec_Desktop.md) (`views::MdTextButton`)                                                                                                      | N/A             |
| **Content placeholder fill**     | `--desktop/sys/container-colors/neutral-container` (`#f2f2f2`) — placeholder only                                             | N/A (client content)                                                                                                                                                                  | N/A             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Shadow does not match `desktop/elevation/3`

- **Figma** applies `elevation/3` (key `0/4/8/3` @15%, ambient `0/1/3/0` @30%)
  and no border stroke.
- **Default Views bubble** (`ShadowConfiguration::elevation == nullopt`) draws a
  legacy two-layer shadow (`(0,2)` blur `8` @20% + blur `12` @10% of
  `kColorShadowBase` = grey800) **plus** a `1px` `kColorBubbleBorder` stroke
  ([bubble_border.cc](//src/ui/views/bubble/bubble_border.cc)).
- Opting into `set_shadow_config({.elevation = 3})` uses
  `gfx::ShadowValue::MakeShadowValues(3, …)` = key `(0,1)` blur `12`
  (`kColorShadowValueKeyShadowElevationThree`, @`0x66`) + ambient `(0,4)` blur
  `64` (@`0x40`) — still different geometry/opacity from the Figma token.
- On **Mac** `DIALOG_SHADOW == NO_SHADOW` (system window shadow); on ChromeOS it
  is `CHROMEOS_SYSTEM_UI_SHADOW`.

### 2. Back arrow and divider are not framework features

`BubbleFrameView` provides title, subtitle, title icon, close, minimize,
progress bar, header view and footnote — but no back-navigation button and no
header divider. Bubbles that need the Figma "Subhead + Back Arrow + Divider"
layout must build a custom title view (`SetTitleView()`) and insert their own
`views::Separator`, so spacing (`6px` arrow gap, `2px` arrow offset, `8px`
divider padding) is not centrally enforced.

### 3. Title spacing below the illustration header

Figma places the title `8px` below the illustration, while
`BubbleFrameView::GetTitleLabelInsetsFromFrame()` adds the full
`title_margins_.top()` (`20`) below `header_height`, leaving a `12px` gap
difference.

### 4. Title ↔ close button gap

Figma uses `8px` between the heading and the close button;
`BubbleFrameView::GetButtonAreaSize()` reserves
`DISTANCE_RELATED_LABEL_HORIZONTAL` (`12`) of left padding.

### 5. Subtitle line height

Figma subhead uses `body-four` with a fixed `18px` line height. The Views
subtitle uses `CONTEXT_LABEL` / `STYLE_SECONDARY` (12px default font) whose line
height falls back to the font height rather than `STYLE_BODY_4` (`18`).

### 6. Content margins depend on content type

Figma spacing matches `DialogContentType::kText` defaults (`8` top / `16`
bottom). Bubbles that call `GetDialogInsetsForContentType(kControl, kControl)`
get `16` / `24`, which is not represented in Figma.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Close button hidden by default

Figma shows a close button on every bubble. `BubbleDialogDelegate`'s constructor
calls `SetShowCloseButton(false)`; callers must opt in.

### 2. Button order is platform-dependent

Figma always orders `[Outlined] … [Tonal] [Primary]`. On Windows,
`PlatformStyle::kIsOkButtonLeading` swaps OK and Cancel, producing
`[Extra] … [Primary] [Tonal]`.

### 3. Outlined leading button is a client extra view

The leading `Outlined` button is not a dialog button; it maps to
`SetExtraView()`, which accepts any view and does not enforce
`ui::ButtonStyle::kDefault` or the `8px`/stretchy spacing semantics beyond
`DISTANCE_RELATED_BUTTON_HORIZONTAL`.

### 4. Features in code missing from Figma

Footnote (`SetFootnoteView`, `kColorBubbleFooterBackground` →
`kColorSysNeutralContainer`), title icon, main image (`SetMainImage`), minimize
button, progress indicator, visible arrow, anchor highlighting and dark theme
are not modeled in Figma.

### 5. Close / back icon-button states

Figma only shows the `Default` state of the 20px icon buttons. Views applies
ink-drop hover/pressed highlights (circle path) and `kColorIconHovered`; no
Figma token is specified for these states in this frame.

### 6. No WebUI equivalent

The Figma doc flags **WEBUI ✗**. There is no `cr-*` bubble element; WebUI
content shown in bubbles is hosted by `WebUIBubbleDialogView`
([webui_bubble_dialog_view.h](//src/chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h)),
which is still a Views `BubbleDialogDelegateView` frame. `<cr-dialog>` is a
modal dialog and is out of scope for this spec.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use a dialog bubble for lightweight, contextual tasks anchored to a UI element
  (e.g. a toolbar button). Prefer a modal dialog for blocking decisions.
- Pick the smallest width that fits: Small (`320`) by default, Medium (`448`)
  for standalone or denser bubbles, Large (`512`) only when necessary. Let
  `GetSnappedDialogWidth()` snap rather than hard-coding widths.
- Keep one Primary (prominent) action on the trailing edge; use Tonal for the
  secondary/dismiss action and the leading extra view for tertiary actions (e.g.
  "Learn more", "Manage").
- Titles use sentence case and concise noun/verb phrases; use the subhead only
  when necessary.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **C++ Views**: the default title is exposed as `ax::mojom::Role::kHeading`
  (level 1) when `features::kBubbleFrameViewTitleIsHeading` is enabled. The
  close button has tooltip and accessible name `IDS_APP_CLOSE`. `Esc` cancels
  the dialog (`DialogClientView` accelerator); `Enter` triggers the default
  button. `InputEventActivationProtector` ignores possibly unintended clicks
  shortly after showing. Use `set_close_on_deactivate()` intentionally, and
  provide an accessible name for custom back buttons.
- **WebUI**: not supported (see §5.6).

### 3. Icon Usage Guidelines

- Close (`kCloseIcon`) and back (`arrow_back`) icons are `20px` inside a `28px`
  circular icon button, colored `on-surface-subtle` (`kColorIcon`).
- Illustration headers should fill the full bubble width and rely on the frame's
  rounded-corner clipping rather than drawing their own corners.

______________________________________________________________________

> [!NOTE] **Not verified**: rendered pixel output (spacing was derived from
> `LayoutProvider`/`ChromeLayoutProvider` metrics, not screenshots); dark-mode
> Figma values (Figma only provides light); exact Figma hover/pressed tokens for
> the header icon buttons; the inner structure of the `Bubble Header` main
> component (only instances were inspected); and ChromeOS/Mac-specific shadow
> rendering.
