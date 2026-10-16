# Dialog Modal Spec (Desktop)

This document maps the specifications of the **Dialog Modal** (Desktop)
component across Figma and code.

______________________________________________________________________

## Overview

The **Dialog Modal** is an elevated, `12px`-rounded surface that interrupts the
current workflow to confirm an action, show critical information, or collect
focused input. Figma defines it as a frame-based template with two layouts
(`Default` and `With Illustration`). Each layout has a headline title, a
flexible content area, and a button bar: an `Outlined` extra button on the
leading side and a `Tonal` + `Primary` pair on the trailing side. The canonical
implementation is C++ Views (`views::DialogDelegate` /
`views::BubbleDialogModelHost`). The Figma documentation header flags **WebUI as
not supported** (`WEBUI` chip with a `cancel` icon); `<cr-dialog>` is listed
below for reference only.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | C++ Views (Desktop)                                                                                                                                                                                                                                                                                                                                      | WebUI (Desktop)                                                                                                      |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Dialog` (`DEFAULT`)<br>`Dialog` (`WITH ILLUSTRATION`)                                                                                                                                                                                                                                                                                                                                                                                                                                                         | `views::DialogDelegate`<br>`views::BubbleDialogModelHost`<br>`views::DialogClientView`<br>`views::BubbleFrameView`                                                                                                                                                                                                                                       | `<cr-dialog>` *(Figma marks WebUI as not supported)*                                                                 |
| **Source Files**   | [Figma Link: `Dialog` (`DEFAULT`, `323:21350`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-21350&m=dev)<br>[Figma Link: `Dialog` (`WITH ILLUSTRATION`, `5987:2854`)](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=5987-2854&m=dev)<br>(Parent doc frame: [`Key UIs / Dialog Modal`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=323-21577&m=dev)) | [ui/views/window/dialog_delegate.h](//src/ui/views/window/dialog_delegate.h)<br>[ui/views/bubble/bubble_dialog_model_host.h](//src/ui/views/bubble/bubble_dialog_model_host.h)<br>[ui/views/window/dialog_client_view.h](//src/ui/views/window/dialog_client_view.h)<br>[ui/views/bubble/bubble_frame_view.h](//src/ui/views/bubble/bubble_frame_view.h) | [ui/webui/resources/cr_elements/cr_dialog/cr_dialog.ts](//src/ui/webui/resources/cr_elements/cr_dialog/cr_dialog.ts) |

> [!NOTE] The Figma dialog is built from plain `FRAME`s, not a `COMPONENT` /
> `COMPONENT_SET`. Both `Dialog` frames are listed directly. The parent
> `323:21577` is only a documentation wrapper.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Dialog` (`323:21350` / `5987:2854`): **no Figma variant properties** (the
  dialog is frame-based). The two layouts are separate frames labeled by
  subheaders: `DEFAULT` | `WITH ILLUSTRATION`.
- Nested `Buttons` instances in the `Button Bar`: `Variant` (Primary | Tonal |
  Outlined) × `Icons` (None) × `State` (Default). See
  [Buttons_Spec_Desktop.md](./Buttons_Spec_Desktop.md).

| Feature / Variant              | Figma Component                                                                            | C++ Views (Desktop)                                                                                                                                           | WebUI (Desktop)                                        |
| :----------------------------- | :----------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------- |
| **Default layout**             | `Dialog` (`DEFAULT`, `323:21350`)                                                          | `views::BubbleDialogModelHost::CreateModal()` or a `views::DialogDelegate` subclass (`BubbleFrameView` + `DialogClientView`)                                  | `<cr-dialog>` wrapping native `<dialog>`               |
| **With Illustration layout**   | `Dialog` (`WITH ILLUSTRATION`, `5987:2854`): `Illustration` frame `408×120` above `Header` | `ui::DialogModel::Builder::SetBannerImage(banner, dark_mode_banner)` → `BubbleFrameView::SetHeaderView()` (`ThemeTrackingImageView`)                          | `<slot name="header">`, rendered *below* the title row |
| **Title**                      | `Header` → `Title` (`Modal title`)                                                         | `ui::DialogModel::Builder::SetTitle()` / `WidgetDelegate::SetTitle()` → `BubbleFrameView::default_title_`                                                     | `<div slot="title">`                                   |
| **Subtitle**                   | *(Not modeled in Figma)*                                                                   | `ui::DialogModel::Builder::SetSubtitle()`                                                                                                                     | Custom element in `header`/`body` slot                 |
| **Content area**               | `Content Area` (`neutral-container` placeholder)                                           | DialogModel fields / `SetContentsView()`                                                                                                                      | `<div slot="body">` (`.body-container.cr-scrollable`)  |
| **Button bar**                 | `Button Bar` (`justify-between`, `pt` 16)                                                  | `DialogClientView` button row (`views::TableLayout`: `[extra] … [cancel][ok]`)                                                                                | `<div slot="button-container">`                        |
| **Primary (trailing)**         | `Buttons` `Variant=Primary`                                                                | `ui::mojom::DialogButton::kOk` → default style `ui::ButtonStyle::kProminent`                                                                                  | `<cr-button class="action-button">`                    |
| **Tonal (trailing)**           | `Buttons` `Variant=Tonal`                                                                  | `ui::mojom::DialogButton::kCancel` → default style `ui::ButtonStyle::kTonal` (non-default buttons)                                                            | `<cr-button class="tonal-button">` / `.cancel-button`  |
| **Outlined (leading / extra)** | `Buttons` `Variant=Outlined`                                                               | `ui::DialogModel::Builder::AddExtraButton()` / `DialogDelegate::SetExtraView()`. `views::MdTextButton` uses `ui::ButtonStyle::kDefault` unless a style is set | Leading `<cr-button>` in `button-container`            |
| **Close (X) button**           | *(Not modeled; omitted on modal)*                                                          | Hidden by default for modals (`SetShowCloseButton(!IsModalDialog())`); `OverrideShowCloseButton()`                                                            | `show-close-button` attribute                          |
| **Footnote**                   | *(Not modeled)*                                                                            | `ui::DialogModel::Builder::SetFootnote()` / `SetFootnoteView()`                                                                                               | `<div slot="footer">`                                  |
| **Alert dialog**               | *(Not modeled)*                                                                            | `ui::DialogModel::Builder::SetIsAlertDialog()` (`kAlertDialog`; `kAlert` on Windows)                                                                          | n/a                                                    |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute             | Figma Design Token                                                                     | C++ Views (Desktop)                                                                                                                                                                                                                      | WebUI (Desktop)                                                                                                                                          |
| :--------------------------- | :------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Container background**     | `--desktop/sys/surface-colors/surface` (`#ffffff`)                                     | `BubbleBorder` → `ui::kColorDialogBackground` → `kColorPrimaryBackground` → `ui::kColorSysSurface`                                                                                                                                       | `--cr-dialog-background-color` (default `white`; `--google-grey-900` + 4% white in dark; `--color-webui-dialog-background` under `[webui-refresh-2026]`) |
| **Elevation / shadow**       | `desktop/elevation/3`: ambient `0 1px 3px 0 #0000004D` + key `0 4px 8px 3px #00000026` | `BubbleBorder::DIALOG_SHADOW` with no elevation → `{0,2} blur 8` `kColorBubbleBorderShadowSmall` (`kColorShadowBase` @ `0x33`) + `{0,2} blur 12` `kColorBubbleBorderShadowLarge` (@ `0x1A`). The window server draws the shadow on macOS | `box-shadow: 0 0 16px rgba(0,0,0,.12), 0 16px 16px rgba(0,0,0,.24)`                                                                                      |
| **Border stroke**            | None                                                                                   | `1dp` `ui::kColorBubbleBorder` → `kColorSysSurfaceVariant` (drawn because no elevation is set; see `ShouldDrawStrokeForArgs`)                                                                                                            | `border: 0` (`--cr-border-hcm` in forced colors)                                                                                                         |
| **Corner radius**            | `--desktop/corner-radius/12` (`12px`)                                                  | `DialogDelegate::GetCornerRadius()` → `ShapeContextTokens::kDialogRadius` → `ShapeSysTokens::kMediumSmall` = `12`. Exceptions: `2` for macOS `ModalType::kWindow`, `0` without compositing                                               | `--cr-dialog-border-radius`, default `8px`                                                                                                               |
| **Width**                    | `448px`                                                                                | `DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH` = `kMediumDialogWidth` = `448` (snaps to `320`/`448`/`512`)                                                                                                                                      | `--cr-dialog-width`, default `512px`                                                                                                                     |
| **Height**                   | `450px` (template)                                                                     | Content-driven; scroll area max `DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT` = `448`                                                                                                                                               | Content-driven, `max-height: 100vh`                                                                                                                      |
| **Outer padding**            | `--desktop/spacing/20` (`20px` all sides)                                              | `ChromeLayoutProvider` `INSETS_DIALOG` = `Insets::VH(20, 20)`; title `INSETS_DIALOG_TITLE` = `TLBR(20,20,0,20)`                                                                                                                          | title `20px` top/inline; body `0 20px`; button container `16px`                                                                                          |
| **Title color**              | `--desktop/sys/surface-colors/on-surface` (`#1f1f1f`)                                  | `CONTEXT_DIALOG_TITLE`/`STYLE_PRIMARY` → `ui::kColorLabelForeground` → `kColorPrimaryForeground` → `ui::kColorSysOnSurface`                                                                                                              | `--cr-primary-text-color`                                                                                                                                |
| **Title typography**         | `desktop/headline/four`: Google Sans, Medium 500, `16px` / `24px`                      | `STYLE_HEADLINE_4`: size `GetFontSizeDelta(16)`, `Weight::MEDIUM`, line height `24`                                                                                                                                                      | `calc(15 / 13 * 100%)` (~`15px`), `line-height: 1`, weight inherited                                                                                     |
| **Body text color**          | `on-surface` (`#1f1f1f`; template placeholder only)                                    | `CONTEXT_DIALOG_BODY_TEXT` → `ui::kColorDialogForeground` → `kColorSecondaryForeground` → `ui::kColorSysOnSurfaceSubtle`                                                                                                                 | `--cr-secondary-text-color`                                                                                                                              |
| **Body typography**          | `desktop/body/four (medium)` 12/18 (placeholder label only)                            | *(Not checked; depends on body content)*                                                                                                                                                                                                 | *(Not checked)*                                                                                                                                          |
| **Title → content gap**      | `--desktop/spacing/8` (`8px`)                                                          | `DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT` = `8` (text) / `_TOP_CONTROL` = `16` (controls)                                                                                                                                                | `--cr-dialog-title-slot-padding-bottom` `16px`                                                                                                           |
| **Illustration → title gap** | `--desktop/spacing/8` (`8px`)                                                          | Header view sits flush above `title_margins_` (`20px` top inset)                                                                                                                                                                         | n/a (header slot is below title)                                                                                                                         |
| **Content → button bar gap** | `--desktop/spacing/16` (`16px` `pt`)                                                   | `DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT` = `16` / `_BOTTOM_CONTROL` = `24`; button row top inset `0`                                                                                                                                 | `--cr-dialog-button-container-padding-top` `16px`                                                                                                        |
| **Button bar insets**        | `16` top / `20` sides & bottom (via dialog padding)                                    | `INSETS_DIALOG_BUTTON_ROW` = `TLBR(0, 20, 20, 20)`                                                                                                                                                                                       | `16px` all sides                                                                                                                                         |
| **Trailing button gap**      | `--desktop/spacing/8` (`8px`)                                                          | `DISTANCE_RELATED_BUTTON_HORIZONTAL` = `8`                                                                                                                                                                                               | `.cancel-button { margin-inline-end: 8px }`                                                                                                              |
| **Button width**             | Hug content (`73px` for "Button"; `16px` h-padding)                                    | Min `DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH` = `64`; columns linked up to `DISTANCE_BUTTON_MAX_LINKABLE_WIDTH` = `112`                                                                                                                     | Hug content                                                                                                                                              |
| **Illustration placeholder** | `--desktop/sys/container-colors/tonal-container` (`#d3e3fd`), `408×120`                | Provided by the banner image asset                                                                                                                                                                                                       | Provided by slotted content                                                                                                                              |
| **Backdrop scrim**           | *(Not modeled)*                                                                        | None for Views modals                                                                                                                                                                                                                    | `dialog::backdrop` `rgba(0,0,0,.6)`                                                                                                                      |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Frame-based Figma template (no component / variants)

The Figma dialog is two plain frames and has no variant properties, so Code
Connect mappings and property-driven specs (for example
`Illustration=True/False`) can't be generated from it. Figma does not model the
other dialog features: subtitle, close button, footnote, alert role, and scroll
overflow.

### 2. Illustration placement

- **Figma**: the `Illustration` (`408×120`) sits *inside* the `20px` padding,
  with an `8px` gap above the title, and its corners are not rounded.
- **Views**: `SetBannerImage()` → `BubbleFrameView::SetHeaderView()` lays the
  header out **full-bleed** (frame width, flush with the top edge).
  `GetClientCornerRadii()` stops rounding the upper client corners while a
  header is present. `SetMainImage()` is a different pattern: a square image on
  the leading side (`128px` width increase minus insets/border), not a top
  banner.
- **WebUI**: `<slot name="header">` renders *below* the title row, so a banner
  above the title requires a custom title in the body.

### 3. Elevation and stroke differ from `desktop/elevation/3`

Views dialogs use the legacy two-layer `DIALOG_SHADOW` (offset `0,2`, blur
`8`/`12`, Grey 800 at 20%/10%) plus a `1dp` `kColorSysSurfaceVariant` stroke.
`DialogDelegate::CreateDialogFrameView()` sets no elevation, so the Material
elevation-3 key/ambient values in Figma (`0 4 8 3` @15%, `0 1 3 0` @30%) are not
used, and Figma has no stroke.

### 4. WebUI `<cr-dialog>` defaults are legacy

`512px` width, `8px` radius, `16px` button padding, ~`15px` title at
`line-height: 1`, and a legacy shadow. Figma explicitly marks WebUI as not
supported for this template. Matching it requires overriding the `--cr-dialog-*`
variables.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Button order is platform-dependent

Figma shows `[Outlined] … [Tonal][Primary]`.
`DialogClientView::GetButtonRowViews()` swaps OK and Cancel when
`PlatformStyle::kIsOkButtonLeading` is true (Windows), which gives
`[extra] … [OK][Cancel]`. Also, `PlatformStyle::kDialogDefaultButtonCanBeCancel`
is `false` on macOS.

### 2. Linked button widths

Views links the widths of all button-row columns, up to `112px`, with a `64px`
minimum, so shorter labels stretch. Figma buttons hug their content.

### 3. Content margins depend on content type

Figma always uses an `8px` gap after the title and a `16px` gap before the
buttons. In Views, these are `8`/`16` when the content starts or ends with text,
and `16`/`24` when it starts or ends with a control
(`LayoutProvider::GetDialogInsetsForContentType`).

### 4. Close button and macOS window-modal radius

Figma omits the close X, and Views modals hide it by default (they match). macOS
`ModalType::kWindow` sheets use a `2px` radius (crbug.com/40144839) instead of
`12px`.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- For new dialogs, prefer declarative `ui::DialogModel::Builder` hosted with
  `views::BubbleDialogModelHost::CreateModal()` over subclassing
  `DialogDelegateView`.
- Use a single `Primary` (`kOk`, `kProminent`) action with a specific verb (for
  example "Delete" or "Turn on"). Use `kCancel` (`kTonal`) for the dismissive
  action. Use the leading extra button (`kDefault` / Outlined) sparingly, for
  one secondary flow such as "Learn more".
- For the `With Illustration` layout, provide both light and dark assets with
  `SetBannerImage(banner, dark_mode_banner)`.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Views**: the window role is `ax::mojom::Role::kDialog`, or `kAlertDialog` /
  `kAlert` (Windows) with `SetIsAlertDialog()`. The title is a `kHeading` when
  `features::kBubbleFrameViewTitleIsHeading` is enabled. Initial focus goes to
  the default button (`kOk`, then `kCancel`). `Enter` activates the default
  button. `Esc` cancels when `EscShouldCancelDialog()` is true.
  `InputEventActivationProtector` ignores clicks that arrive too soon after the
  dialog is shown.
- **WebUI**: provide `slot="title"` (wired to `aria-labelledby`). `Enter` clicks
  `.action-button` unless `ignore-enter-key` is set. `Esc` cancels unless
  `no-cancel` is set.
- Banner illustrations are decorative. Views already ignores them in the
  accessibility tree.

### 3. Icon Usage Guidelines

- The Figma template uses no icons. Dialog buttons follow the Buttons spec.

______________________________________________________________________

## 7. Verification Notes

> [!IMPORTANT] **Verified against code**: layout metrics (`layout_provider.cc`,
> `chrome_layout_provider.cc`), corner radius and button styles
> (`dialog_delegate.cc`), button order and linking (`dialog_client_view.cc`),
> shadow and stroke logic (`bubble_border.cc`), color mixer chains
> (`ui_color_mixer.cc`, `material_ui_color_mixer.cc`), title typography
> (`typography_provider.cc`, `bubble_frame_view.cc`), banner handling
> (`bubble_dialog_model_host.cc`), and `cr_dialog.css`.
>
> **Not verified**: rendered pixels or screenshots; the exact macOS native
> shadow; `--color-webui-dialog-background` resolution; `kColorShadowBase` under
> every theme; body text typography in real dialogs; final resolved
> `main_image_` size; the dark theme (Figma only shows light).
