# Dialog Spec (Clank)

This document maps the specifications of the **Dialog** (Clank) component across
Figma and code.

______________________________________________________________________

## Overview

A **Dialog** tells the user about a task, can hold critical information, or asks
for a decision. It keeps focus until the user dismisses it or takes the required
action. Figma models the dialog as two component sets. `Dialog-mobile` has eight
content configurations, and `Dialog-tablet` has one, the Page Info
configuration. In code, every configuration is the single MVC widget
`ModalDialogView`. It is driven by `ModalDialogProperties` and shown by
`AppModalPresenter` or `TabModalPresenter`. Configuration-specific content goes
in through `CUSTOM_VIEW`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                                                                                                                                                                                          | Clank (Android)                                                                                                                                                                                                                                        |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Dialog-mobile`<br>`Dialog-tablet`                                                                                                                                                                                                                                                                                       | `ModalDialogView`                                                                                                                                                                                                                                      |
| **Source Files**   | [Figma Link: `Dialog-mobile` (`7164:15`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7164-15&m=dev)<br>[Figma Link: `Dialog-tablet` (`40430:55718`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=40430-55718&m=dev) | [components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogView.java) |

> Source page:
> [`↳ Dialogs` (`6365:34191`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6365-34191).
> This is a documentation canvas with guidance, examples, and light/dark
> showcases. It holds only *instances*. The main component sets are on the
> `↳ Core components` page (`43097:26788`).
>
> Supporting code (not listed above because it isn't a primary widget class):
>
> - Model:
>   [ModalDialogProperties.java](//src/ui/android/java/src/org/chromium/ui/modaldialog/ModalDialogProperties.java)
> - Binder:
>   [ModalDialogViewBinder.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/ModalDialogViewBinder.java)
> - Presenters:
>   [AppModalPresenter.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/AppModalPresenter.java)
>   and
>   [TabModalPresenter.java](//src/components/browser_ui/modaldialog/android/java/src/org/chromium/components/browser_ui/modaldialog/TabModalPresenter.java)
> - Layouts:
>   [modal_dialog_view.xml](//src/components/browser_ui/modaldialog/android/java/res/layout/modal_dialog_view.xml)
>   and
>   [modal_dialog_title.xml](//src/components/browser_ui/modaldialog/android/java/res/layout/modal_dialog_title.xml)
> - Styles:
>   [modaldialog styles.xml](//src/components/browser_ui/modaldialog/android/java/res/values/styles.xml),
>   [modaldialog dimens.xml](//src/components/browser_ui/modaldialog/android/java/res/values/dimens.xml),
>   [browser_ui styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)
>   (`AlertDialogContent`, `TextAppearance.AlertDialogTitleStyle`), and
>   [widget styles.xml](//src/components/browser_ui/widget/android/java/res/values/styles.xml)
>   (`ThemeOverlay.BrowserUI.AlertDialog`)
> - Background:
>   [dialog_bg_no_shadow.xml](//src/components/browser_ui/widget/android/java/res/drawable/dialog_bg_no_shadow.xml)
>   and
>   [dialog_bg_color.xml](//src/components/browser_ui/styles/android/java/res/color/dialog_bg_color.xml)
>   (light), plus
>   [color-night/dialog_bg_color.xml](//src/components/browser_ui/styles/android/java/res/color-night/dialog_bg_color.xml)
>   (dark)

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Dialog-mobile`: `Config` (Download | Promo with illustration | Tab groups |
  Permission | Address | FRE | Base | New tab group). The default is `Base`. The
  boolean properties are `Buttons`, `Title`, `Subtitle`, `Label`, and `Icon`
  (all default `true`). There is also an `Image` instance swap (default
  `Placeholder`, `35692:11268`).
- `Dialog-tablet`: `Config` (Page Info).

`Config` describes product content rather than a structural variant.
`ModalDialogView` has no "config" concept. Each configuration is a combination
of model properties, usually with a feature-owned `CUSTOM_VIEW`.

| Feature / Variant                                             | Figma Component                                                                     | Clank (Android)                                                                                                                                                                                                                               |
| :------------------------------------------------------------ | :---------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Base (title + body + 2 buttons)**                           | `Config=Base` ("Close all tabs?")                                                   | `TITLE`, `MESSAGE_PARAGRAPH_1`, `POSITIVE_BUTTON_TEXT`, `NEGATIVE_BUTTON_TEXT`; `BUTTON_STYLES=PRIMARY_FILLED_NEGATIVE_OUTLINE` (→ `ThemeOverlay.BrowserUI.ModalDialog.FilledPrimaryButton`)                                                  |
| **Leading title icon**                                        | `Config=Base`, `Icon=true` (24px `warning`, onSurfaceVariant)                       | `TITLE_ICON` (`@id/title_icon`, 36×36dp, `marginEnd` 8dp)                                                                                                                                                                                     |
| **Permission (icon + message inline, two text buttons)**      | `Config=Permission`                                                                 | Permission prompts use `CUSTOM_VIEW` (feature-owned layout) + `BUTTON_STYLES=PRIMARY_OUTLINE_NEGATIVE_OUTLINE` (→ `TextPrimaryButton`, both `@style/TextButton`)                                                                              |
| **Promo with illustration**                                   | `Config=Promo with illustration` (`Image` swap at top, title, multi-paragraph body) | `CUSTOM_VIEW` for the image, or `MESSAGE_PARAGRAPHS`. There is no dedicated header-image slot.                                                                                                                                                |
| **Download (title + input rows + checkbox)**                  | `Config=Download`                                                                   | `TITLE` + `CUSTOM_VIEW` (download location view). The generic `CHECKBOX_TEXT` / `CHECKBOX_CHECKED` slot is available.                                                                                                                         |
| **Address (title + subtitle + list item + input + footnote)** | `Config=Address`, `Subtitle`, `Label` booleans                                      | `TITLE` + `CUSTOM_VIEW` (autofill save-address view)                                                                                                                                                                                          |
| **FRE (account picker list)**                                 | `Config=FRE` ("Choose an account")                                                  | `TITLE` + `CUSTOM_VIEW`, or `MENU_ITEMS` (`ModalDialogMenuItem` icon + text rows)                                                                                                                                                             |
| **New tab group (input + color picker)**                      | `Config=New tab group` (width 392)                                                  | `TITLE` + `CUSTOM_VIEW` (tab group creation view)                                                                                                                                                                                             |
| **Tab groups (top app bar + content grid)**                   | `Config=Tab groups` (`Top-app-bar` with back/more, fixed 642 height)                | `TITLE_BACK_BUTTON_VISIBLE`, `TITLE_MORE_BUTTON_VISIBLE` / `TITLE_MORE_BUTTON_DELEGATE`, `TITLE_CLOSE_BUTTON_VISIBLE`, and `CUSTOM_VIEW`. A full-screen alternative is available via `DIALOG_STYLES=FULLSCREEN_DIALOG` / `DIALOG_WHEN_LARGE`. |
| **Tablet Page Info**                                          | `Dialog-tablet` / `Config=Page Info` (600 wide, URL row + list items)               | `PageInfoDialog` → `CUSTOM_VIEW` in `ModalDialogView` (`CANCEL_ON_TOUCH_OUTSIDE=true`)                                                                                                                                                        |
| **Buttons hidden**                                            | `Buttons=false`                                                                     | Leave `POSITIVE_BUTTON_TEXT` / `NEGATIVE_BUTTON_TEXT` empty (the button bar hides), or use `CUSTOM_BUTTON_BAR_VIEW`                                                                                                                           |
| **Filled primary + text secondary**                           | Most configs (filled `Button` + text `Button`)                                      | `ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE`                                                                                                                                                                                                |
| **Two text buttons**                                          | `Config=Permission` (Block / Allow)                                                 | `ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE`. Despite the "OUTLINE" name, this uses `@style/TextButton`.                                                                                                                                   |
| **Filled negative**                                           | *(Not modeled)*                                                                     | `ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED`                                                                                                                                                                                                |
| **Vertical tonal button group**                               | *(Not modeled)*                                                                     | `BUTTON_GROUP_BUTTON_SPEC_LIST` → `FilledButton.Tonal.ThemeOverlay.{Single,Top,Middle,Bottom}Button`                                                                                                                                          |
| **Footer message**                                            | *(Not modeled)*                                                                     | `FOOTER_MESSAGE` (`TextAppearance.TextMedium.Secondary`, 24dp/16dp padding)                                                                                                                                                                   |
| **Scrollable title**                                          | *(Not modeled)*                                                                     | `TITLE_SCROLLABLE`                                                                                                                                                                                                                            |
| **Full-screen / dialog-when-large**                           | *(Not modeled)*                                                                     | `DIALOG_STYLES` (`FULLSCREEN_DIALOG`, `FULLSCREEN_DARK_DIALOG`, `DIALOG_WHEN_LARGE`)                                                                                                                                                          |
| **Dark theme**                                                | Light/dark showcase frames (`63417:54958`, `63417:54957`)                           | `values-night` / `color-night/dialog_bg_color.xml`                                                                                                                                                                                            |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

Figma values come from the `Dialog-mobile` (`7164:15`) and `Dialog-tablet`
(`40430:55718`) design context and the light/dark instances on the source page.

| Design Attribute                               | Figma Design Token                                                                                                                                                                 | Clank (Android)                                                                                                                                                                 | Match                                               |
| :--------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :-------------------------------------------------- |
| **Container background (light)**               | `clank/macro/dialog/dialog_bg_color` = `#FFFFFF`                                                                                                                                   | `@color/dialog_bg_color` → `?attr/colorSurface` (via `@drawable/dialog_bg_no_shadow`)                                                                                           | ✅ (same macro name)                                |
| **Container background (dark)**                | `clank/macro/dialog/dialog_bg_color` = `#282A2C`                                                                                                                                   | `color-night/dialog_bg_color` → `?attr/colorSurfaceContainerHigh`                                                                                                               | ✅ (`#282A2C` = M3 surfaceContainerHigh, dark)      |
| **Corner radius**                              | `clank/shape/corner_radius/24` (24px)                                                                                                                                              | `@dimen/dialog_corner_radius` = 24dp                                                                                                                                            | ✅                                                  |
| **Elevation / shadow**                         | Most configs: `drop-shadow 0 4 4 rgba(60,64,67,.15), 0 1 1.5 rgba(60,64,67,.3)`. `Tab groups` and tablet: `0 4 8 3 rgba(60,64,67,.15), 0 1 3 0 rgba(60,64,67,.3)` (M3 elevation 3) | No Chromium-defined elevation. The background drawable is `dialog_bg_no_shadow`, and the window comes from the `ThemeOverlay.Material3Expressive.Dialog(.Alert)` parent.        | ⚠️ Not verified (see §4)                            |
| **Scrim**                                      | `scrim` layer in guidance frames                                                                                                                                                   | `android:backgroundDimAmount` = 0.65                                                                                                                                            | ⚠️ Figma scrim color/opacity not checked            |
| **Mobile width**                               | 380px (412 screen − 2×16). `New tab group` is 392px.                                                                                                                               | `modal_dialog_view_external_margin` = 16dp; `maxWidthPortrait` = `@dimen/abc_dialog_min_width_minor`                                                                            | ✅ margin; ⚠️ AppCompat max-width value not checked |
| **Tablet width**                               | 600px                                                                                                                                                                              | `values-sw600dp`: `modal_dialog_max_width_major/minor` = 560dp, `modal_dialog_min_width` = 280dp                                                                                | ❌ 600 vs 560dp                                     |
| **Max height**                                 | `Tab groups` is a fixed 642px                                                                                                                                                      | `modal_dialog_max_height` = 560dp                                                                                                                                               | ❌ The Figma example is taller than the code cap    |
| **Header/content horizontal padding**          | `clank/shape/spacing/24` (24px)                                                                                                                                                    | `AlertDialogContent` `paddingStart/End` = `@dimen/dialog_padding_sides` → `abc_dialog_padding_material` (24dp in AppCompat)                                                     | ✅                                                  |
| **Header top padding**                         | `clank/shape/spacing/24` (24px)                                                                                                                                                    | `AlertDialogContent` `paddingTop` = `@dimen/dialog_padding_top` → `abc_dialog_padding_top_material` (18dp in AppCompat)                                                         | ❌ 24 vs 18dp                                       |
| **Title → body gap**                           | Header `pb` = `clank/shape/spacing/16` (16px); `Promo` text-block gap 8px                                                                                                          | The message paragraph also uses `AlertDialogContent` → `paddingTop` 18dp                                                                                                        | ❌ 16 vs 18dp                                       |
| **Title typography**                           | `Google Sans` Regular, `clank/font/font_size/headline` 22px / `line_height/headline` 28px, `letter_spacing/headline` 0                                                             | `TextAppearance.AlertDialogTitleStyle`: `@dimen/headline_size` 22sp, `@font/accent_font`, `textStyle=normal`. No line height is set (`DialogTitle`, not `TextViewWithLeading`). | ✅ size/weight; ⚠️ 28 leading not enforced          |
| **Title color**                                | `clank/sys/color/onsurface` (`#1F1F1F` / `#E3E3E3`)                                                                                                                                | `@color/default_text_color_list`                                                                                                                                                | ✅                                                  |
| **Body typography**                            | `Google Sans Text` Regular, `textmedium` 14px / 20px                                                                                                                               | `modal_dialog_paragraph_view` → `TextAppearance.TextMedium.Secondary`, `app:leading=@dimen/text_size_medium_leading` (20sp)                                                     | ✅                                                  |
| **Body color**                                 | `Base` / `Permission`: `clank/sys/color/onsurface`. `Promo` / `Address`: `clank/sys/color/onsurfacevariant` (`#444746` / `#C4C7C5`)                                                | `TextMedium.Secondary` → `@macro/default_text_color_secondary` (onSurfaceVariant) for every paragraph                                                                           | ❌ for `Base` / `Permission`                        |
| **Title icon**                                 | 24px Google Symbols glyph, gap `clank/shape/spacing/10` (`Base`) or `spacing/8` (`Permission`), color onSurfaceVariant (`warning`) / primary (`location_on`)                       | `@id/title_icon` 36×36dp, `layout_marginEnd` 8dp, tint supplied by the caller                                                                                                   | ❌ 24 vs 36dp; ⚠️ 10 vs 8dp gap for `Base`          |
| **Button bar padding**                         | `Buttons` row: `px` `spacing/16`, gap to content `spacing/16`, dialog `pb` `spacing/16`                                                                                            | `DualControlLayout` padding = `?attr/dualControlLayoutVerticalPadding` / `HorizontalPadding` → `modal_dialog_control_{vertical,horizontal}_padding_filled` = 16dp               | ✅                                                  |
| **Gap between buttons**                        | `clank/shape/spacing/16` (16px), wrapping                                                                                                                                          | `@dimen/dual_control_margin_between_items` = 8dp; stacked `@dimen/button_bar_stacked_margin` = 8dp                                                                              | ❌ 16 vs 8dp                                        |
| **Button alignment**                           | `justify-end` (trailing)                                                                                                                                                           | `app:buttonAlignment="end"`                                                                                                                                                     | ✅                                                  |
| **Button size**                                | 40px pill + `spacing/4` vertical wrapper = 48px target, `px` `spacing/24`                                                                                                          | `ButtonCompat` (`@style/FilledButton` / `@style/TextButton`). See [Button_Spec_Clank.md](./Button_Spec_Clank.md).                                                               | ✅ (per the Button spec)                            |
| **Primary button fill / label**                | `clank/sys/color/primary` / `clank/sys/color/onprimary`, `buttontext` 14/20 Google Sans Medium                                                                                     | `@style/FilledButton`                                                                                                                                                           | ✅ (per the Button spec)                            |
| **Secondary button label**                     | `clank/sys/color/primary` text, no fill                                                                                                                                            | `@style/TextButton`                                                                                                                                                             | ✅ (per the Button spec)                            |
| **Body input / list rows (Download, Address)** | Row `px` `spacing/24`, `py` `spacing/8`, icon–text gap `spacing/24`, `textlarge` 16/24 onSurface, 1px divider onSurfaceVariant                                                     | Feature-owned `CUSTOM_VIEW`                                                                                                                                                     | ⚠️ Not checked (out of `ModalDialogView` scope)     |
| **Checkbox row**                               | `textlarge` 16/24 onSurface, row `px` 24                                                                                                                                           | `@id/modal_dialog_checkbox`: `marginStart/End` 24dp, `marginTop` 16dp, `TextAppearance.TextMedium.Secondary` (14sp, secondary)                                                  | ❌ 16sp onSurface vs 14sp secondary                 |
| **Top app bar (Tab groups)**                   | 56px tall, `px` `spacing/16`, `py` `spacing/18`, 24px `arrow_back` / `more_vert` in onSurface                                                                                      | `modal_dialog_title.xml`: `title_back` / `title_more_button` / `title_close_button` at `@dimen/min_touch_target_size` (48dp); `modal_dialog_title_with_icons_padding` = 12dp    | ⚠️ Partial (no 56dp bar height in code)             |
| **Tablet divider**                             | `clank/macro/divider/divider_color` (`#DDE3EA`)                                                                                                                                    | Inside `PageInfoView` (`CUSTOM_VIEW`)                                                                                                                                           | ⚠️ Not checked                                      |
| **Bottom spacer when no button bar**           | Dialog `pb` `spacing/16`                                                                                                                                                           | `@dimen/modal_dialog_bottom_spacer_height` = 16dp                                                                                                                               | ✅                                                  |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

- **Config vs. composition.** Figma's `Config` axis (Download, Address, FRE, New
  tab group, Tab groups, Promo, Permission, Page Info) is product content. Code
  has one generic shell. Everything below the title (input rows, list items,
  color pickers, tab grids, illustrations) comes from a feature-owned
  `CUSTOM_VIEW`. So `ModalDialogView` doesn't own those tokens, and each feature
  must match them separately.
- **Header spacing comes from AppCompat.** The title/paragraph padding uses
  `@dimen/dialog_padding_top` / `dialog_padding_sides`, which alias the
  AppCompat `abc_dialog_padding_top_material` (18dp) and
  `abc_dialog_padding_material` (24dp). Figma uses 24px top and 16px title→body.
  That makes Chromium dialogs about 6dp shorter at the top and 2dp looser
  between title and body.
- **No title line height.** `TextAppearance.AlertDialogTitleStyle` sets size,
  font, and color but no leading. `DialogTitle` therefore uses the font's
  natural line height instead of the Figma `headline` 28px.
- **Body color is fixed.** Every message paragraph uses `TextMedium.Secondary`
  (onSurfaceVariant). Figma's `Base` and `Permission` configs use onSurface for
  the body. There's no model property to choose the primary body color.
- **Tablet width.** Figma's `Dialog-tablet` is 600px wide, but `values-sw600dp`
  caps `ModalDialogView` at 560dp (`maxWidthLandscape` / `maxWidthPortrait`).
- **Max height.** `modal_dialog_max_height` is 560dp. Figma's `Tab groups`
  example is 642px tall, so in code it would scroll or need `DIALOG_WHEN_LARGE`
  / full-screen.
- **Elevation and shadow aren't verified.** Figma uses two different shadow
  recipes: a light 2-layer drop shadow for most configs and M3 elevation-3 for
  `Tab groups` and tablet. Chromium sets
  `windowBackground=@drawable/dialog_bg_no_shadow` and inherits window elevation
  from `ThemeOverlay.Material3Expressive.Dialog(.Alert)`. `TabModalPresenter`
  sets the same no-shadow background on the view. No Chromium resource defines a
  dialog elevation to compare against.
- **Button-group variants.** The code supports a vertical tonal button group
  (`BUTTON_GROUP_BUTTON_SPEC_LIST`) and a filled-negative style, but neither is
  in Figma.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **Button spacing.** Figma uses a 16px gap between side-by-side buttons.
  `DualControlLayout` uses `dual_control_margin_between_items` (8dp), and 8dp
  when stacked.
- **Title icon size.** Figma shows a 24px glyph. Code reserves a 36×36dp
  `ImageView` with an 8dp end margin. Figma's `Base` config uses a 10px gap.
- **Checkbox styling.** The built-in checkbox uses 14sp secondary text with a
  16dp top margin. Figma's `Download` config shows 16px onSurface (`textlarge`)
  text in a 24px-padded row.
- **Top app bar.** Figma's `Tab groups` config uses a 56px `Top-app-bar`
  component with 16px horizontal padding. Code uses the title row with optional
  48dp back / more / close buttons and 12dp start padding. The title text style
  differs from a top-app-bar title.
- **Button naming.** `ButtonStyles.PRIMARY_OUTLINE_*` maps to
  `@style/TextButton` (text buttons), not outlined buttons. That matches Figma's
  text buttons, but the name is misleading.
- **Interactive states.** Figma has no dialog-level states. Button and list
  states come from the child components (`Button`, `List-item`, `Checkbox`).
  Code adds `BUTTON_TAP_PROTECTION_PERIOD_MS`, `FILTER_TOUCH_FOR_SECURITY`, and
  `CANCEL_ON_TOUCH_OUTSIDE` behaviors that Figma doesn't model.
- **Fading-edge scroll.** Code wraps the title/body in `FadingEdgeScrollView`
  (with scrollbars). Figma doesn't show an overflow or scroll state.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use dialogs sparingly because they interrupt. Use them only for critical
  information or decisions that need an explicit response (per Figma guidance).
- Use at most one filled (primary) button. Put the dismissive action as a text
  button to its leading side, with both aligned to the trailing edge.
- Write titles as short questions or statements ("Close all tabs?", "Save
  address?"), and write button labels as specific verbs ("Close tabs",
  "Download", "Save") rather than "OK".
- Prefer `ModalDialogProperties` slots (`TITLE`, `MESSAGE_PARAGRAPHS`,
  `TITLE_ICON`, `CHECKBOX_TEXT`, `FOOTER_MESSAGE`) before reaching for
  `CUSTOM_VIEW`. Custom content must match the Figma row tokens itself.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Set `CONTENT_DESCRIPTION` so TalkBack announces the dialog. The title is read
  as the dialog title.
- Title back, more, and close buttons are `@dimen/min_touch_target_size` (48dp).
  The close button carries `@string/close` as its `contentDescription`. Dialog
  buttons get a 48dp target from `ButtonCompat` plus vertical insets.
- The negative button uses `accessibilityTraversalAfter` the positive button.
  Check that the reading order matches the visual order when you customize the
  button bar.
- Use `TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE` and
  `APP_MODAL_DIALOG_BACK_PRESS_HANDLER` to make sure hardware Escape and system
  back dismiss correctly.
- For security-sensitive prompts (permissions), keep `FILTER_TOUCH_FOR_SECURITY`
  and `BUTTON_TAP_PROTECTION_PERIOD_MS` enabled.

### 3. Icon Usage Guidelines

- Title icons are optional and should reinforce meaning, such as `warning` for
  destructive actions or `location_on` for permission prompts. Figma renders
  them at 24px. The default tint is onSurfaceVariant, and the permission/brand
  context uses primary.
- Icons in body rows (`draft`, `folder`, `edit`) are 24px onSurfaceVariant
  glyphs, laid out by the feature's custom view.
