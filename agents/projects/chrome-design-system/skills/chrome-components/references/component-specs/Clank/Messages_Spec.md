# Messages Spec (Clank)

This document maps the specifications of the **Messages** (message banner)
(Clank) component across Figma and code.

______________________________________________________________________

## Overview

A **Message** is a small card that slides in from the top of the screen. It
shows a short, important message with an action the user can take right away, or
the user can dismiss it. Messages replace infobars as Clank's preferred way to
offer optional, contextual features (see go/clankmessages). They're hard to miss
but easy to ignore. Figma models one card with an icon, a title, optional
secondary text, a primary text button, and an optional secondary (settings)
action. In code this is `MessageBannerView`, configured through a
`PropertyModel` (`MessageBannerProperties`) and shown with
`MessageDispatcher#enqueueMessage`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                        | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                            |
| :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Message`                                                                                                                                              | `MessageBannerView`<br>`MessageDispatcher` (API)                                                                                                                                                                                                                                                                                                                                                                           |
| **Source Files**   | [Figma Link: `Message` (`9416:14186`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=9416-14186&m=dev) | [components/messages/android/internal/java/src/org/chromium/components/messages/MessageBannerView.java](//src/components/messages/android/internal/java/src/org/chromium/components/messages/MessageBannerView.java)<br>[components/messages/android/java/src/org/chromium/components/messages/MessageDispatcher.java](//src/components/messages/android/java/src/org/chromium/components/messages/MessageDispatcher.java) |

> Source page:
> [`↳ Messages` (`6380:48652`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6380-48652)
> is a documentation canvas. It holds a guidance card (`Guidance/Messages`,
> `18765:28773`), a showcase frame (`Messages`, `43325:31861`, light and dark),
> and an examples frame (`43343:118868`). All of these only hold instances. The
> main component set `Message` (`9416:14186`) lives on the `↳ Core components`
> page. Sub-component: `.secondary-action` (`75579:4935`), which shows a
> `permissions-menu` (`75627:8049`) when `Hover=true`.
>
> Supporting code: layout
> [message_banner_view.xml](//src/components/messages/android/internal/java/res/layout/message_banner_view.xml),
> dimensions in
> [dimens.xml](//src/components/messages/android/internal/java/res/values/dimens.xml),
> background
> [message_bg_tinted.xml](//src/components/messages/android/internal/java/res/drawable-v31/message_bg_tinted.xml)
> and
> [message_bg_color.xml](//src/components/messages/android/internal/java/res/color/message_bg_color.xml),
> model keys in
> [MessageBannerProperties.java](//src/components/messages/android/java/src/org/chromium/components/messages/MessageBannerProperties.java),
> binder
> [MessageBannerViewBinder.java](//src/components/messages/android/internal/java/src/org/chromium/components/messages/MessageBannerViewBinder.java),
> animation in
> [MessageBannerMediator.java](//src/components/messages/android/internal/java/src/org/chromium/components/messages/MessageBannerMediator.java),
> default timeout in
> [ChromeMessageAutodismissDurationProvider.java](//src/chrome/android/java/src/org/chromium/chrome/browser/messages/ChromeMessageAutodismissDurationProvider.java).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Message`: `Config` (Single-action | Multi-action) × `Action` (boolean,
  default `true`; it only affects `Single-action`).
- `.secondary-action`: `Hover` (boolean). When `true`, it shows the
  `permissions-menu` dropdown (`.Menu-items`).

| Feature / Variant                      | Figma Component                                          | Clank (Android)                                                                                                                  |
| :------------------------------------- | :------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------- |
| **Single action**                      | `Config=Single-action`, `Action=true`                    | Set `PRIMARY_BUTTON_TEXT` + `ON_PRIMARY_ACTION`; no `SECONDARY_ICON`                                                             |
| **No action (text only)**              | `Config=Single-action`, `Action=false`                   | Leave `PRIMARY_BUTTON_TEXT` unset (`PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET` hides the button)                             |
| **Multi-action**                       | `Config=Multi-action` (settings icon + divider + action) | `SECONDARY_ICON` / `SECONDARY_ICON_RESOURCE_ID` → `ListMenuButton`; setting it also shows `@id/message_divider`                  |
| **Secondary menu**                     | `.secondary-action` `Hover=true` → `permissions-menu`    | `SECONDARY_MENU_BUTTON_DELEGATE` (`ListMenuDelegate`) or `SECONDARY_BUTTON_MENU_TEXT` (one-item menu); `SECONDARY_MENU_MAX_SIZE` |
| **Secondary text**                     | `Secondary text if needed` layer (optional)              | `DESCRIPTION` (`CharSequence`), `DESCRIPTION_MAX_LINES`                                                                          |
| **Leading icon**                       | `Icon Placeholder` (24px, primary tint)                  | `ICON` / `ICON_RESOURCE_ID`; `ICON_TINT_COLOR` (default accent1, `TINT_NONE` for full-color)                                     |
| **Large icon**                         | *(Not modeled)*                                          | `LARGE_ICON` → `@dimen/message_icon_size_large` `36dp`; `ICON_ROUNDED_CORNER_RADIUS_PX`                                          |
| **Description icon**                   | *(Not modeled)*                                          | `DESCRIPTION_ICON` (18dp compound drawable), `RESIZE_DESCRIPTION_ICON`                                                           |
| **Progress spinner instead of button** | *(Not modeled)*                                          | `PRIMARY_WIDGET_APPEARANCE = PROGRESS_SPINNER` → `CircularProgressIndicator` (32dp, 3dp track)                                   |
| **Close button (pointer hover)**       | *(Not modeled)*                                          | `ENABLE_CLOSE_BUTTON`: a 24dp circular `ChromeImageButton` appears on pointer hover at the top-end corner                        |
| **Navigation vs. window scope**        | Guidance only ("Navigation Scope" / "Window Scope")      | `MessageDispatcher#enqueueMessage(model, webContents, @MessageScopeType, highPriority)` / `enqueueWindowScopedMessage`           |
| **Dark theme**                         | Dark-mode instances in the `Messages` frame              | `color-night/message_bg_color.xml` and night theme attrs                                                                         |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute              | Figma Design Token                                                                                                           | Clank (Android)                                                                                                                                                                                                                                                             |
| :---------------------------- | :--------------------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Background**                | `--clank/macro/messages/messages_bg_color` (light `#ffffff`, dark `#282a2c`)                                                 | API 31+: `@color/message_bg_color` → `?attr/colorSurfaceBright` (night: `?attr/colorSurfaceContainerHigh`). Below 31: `@color/dialog_bg_color_baseline`. Low-end devices: `@drawable/dialog_bg_baseline`                                                                    |
| **Corner radius**             | `--clank/shape/corner_radius/16` (**16px**)                                                                                  | `@dimen/message_banner_radius` **`12dp`**                                                                                                                                                                                                                                   |
| **Elevation / shadow**        | `messages_low`: key `0 0 2px #3c40431f` + ambient `0 0 6px #3c404326` (no offset, spread 0)                                  | `android:elevation="@dimen/message_banner_elevation"` `6dp` (back of stack `5dp`). Platform shadow, not a matched token                                                                                                                                                     |
| **Width**                     | `380px` (fixed)                                                                                                              | `BoundedLinearLayout` `maxWidthPortrait/Landscape = @dimen/message_max_width` `380dp`; lateral margin `12dp`                                                                                                                                                                |
| **Height / vertical padding** | Root `py` `--clank/shape/spacing/12` (12px), 68px tall with 2 lines                                                          | `minHeight = @dimen/message_banner_main_content_height` `72dp`; text column `paddingTop/Bottom = @dimen/message_banner_vertical_padding` `12dp`                                                                                                                             |
| **Icon**                      | 24px glyph, container `px` 12px, height 44px; color `--clank/sys/color/primary` (`#0b57d0`, dark `#a8c7fa`)                  | `@dimen/message_icon_size` `24dp`, `layout_marginHorizontal = @dimen/message_icon_margin` `12dp`; `app:tint="@macro/default_icon_color_accent1"`                                                                                                                            |
| **Title text**                | `textLarge`: Google Sans Text Regular, 16 / 24, tracking 0; `--clank/sys/color/onSurface, os` (`#1f1f1f`, dark `#e3e3e3`)    | `@style/TextAppearance.TextLarge.Primary`: `@dimen/text_size_large` `16sp`, `@dimen/text_size_large_leading` `24sp`, `@color/default_text_color_list`                                                                                                                       |
| **Secondary text**            | `textSmall`: Google Sans Text Regular, 12 / 16; `--clank/sys/color/onSurfaceVariant, osv` (`#444746`)                        | `@style/TextAppearance.TextSmall.Secondary`: `12sp` / `16sp`, `@color/default_text_color_secondary_list`                                                                                                                                                                    |
| **Text → action gap**         | `padding` spacer `4px`                                                                                                       | No spacer. The gap comes from the button's start padding (`TextButton` `paddingStart` `8dp`)                                                                                                                                                                                |
| **Primary action**            | `Text Button/1.Enabled customized`: width `88px`, `py` 12px, radius `--clank/shape/corner_radius/4` (4px)                    | `ButtonCompat` `@style/TextButton`: `minWidth = @dimen/button_min_width` `88dp`, `maxWidth = @dimen/message_primary_button_max_width` `112dp`, `minHeight` `48dp`, padding `8dp` start/end, `verticalInset=0dp`; ripple corners `0/0/12/12dp` (end corners = banner radius) |
| **Action text**               | `buttonText`: Google Sans Medium, 14 / 20, weight 500; `--clank/sys/color/primary` (`#0b57d0`, dark `#a8c7fa`)               | `TextAppearance.Button.Text.Blue` (parent `TextAccentMediumThick`: `@font/accent_font`, `14sp` / `20sp`), `@color/blue_when_enabled_list`; text color from `?attr/globalTextButtonTextColor`                                                                                |
| **Secondary action**          | `.secondary-action`: 24px `settings` glyph, padding `--clank/shape/spacing/10` (10px) → 44px; `onSurfaceVariant` (`#444746`) | `ListMenuButton` `48dp × 48dp` (`@dimen/min_touch_target_size`), `app:tint="@macro/default_icon_color_secondary"`, `?actionBarItemBackground` ripple                                                                                                                        |
| **Divider**                   | `Divider container` 4 × 24px (1px line); `--clank/macro/divider/divider_color` (`#dde3ea`)                                   | `ImageView` `1dp × 24dp`, `@drawable/divider_line_bg` → `@color/divider_color` (`?attr/colorSurfaceContainerHighest`, night `?attr/colorOutlineVariant`); `marginTop/Bottom` `12dp`                                                                                         |
| **Motion**                    | Guidance: animates in from the top; stays 10s by default; swipe up or sideways to dismiss                                    | `ENTER_DURATION_MS` `550`, `EXIT_DURATION_MS` `350` (translate + alpha); default auto-dismiss `10s` (`30s` with a11y gestures), override with `DISMISSAL_DURATION`; `MessageSwipeGestureListener`                                                                           |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Corner radius (16 vs. 12)

- **Figma**: `--clank/shape/corner_radius/16` (16px).
- **Clank**: `@dimen/message_banner_radius` is `12dp`. The primary-button ripple
  end corners also use `message_banner_radius`, so changing the radius keeps
  them in sync.

### 2. Elevation model

- **Figma**: A small, symmetric two-layer shadow (`messages_low`, blur 2px and
  6px, no y-offset).
- **Clank**: Standard Android `6dp` elevation (`5dp` for stacked messages behind
  the front one). Android draws its own key and ambient shadows, so the result
  won't match the Figma values exactly. Low-end devices have no elevation and
  use `dialog_bg_baseline` instead.

### 3. Background token resolution

- Figma has a dedicated `messages_bg_color` macro. Code resolves it per API
  level: `colorSurfaceBright` / night `colorSurfaceContainerHigh` on API 31+,
  and `dialog_bg_color_baseline` below that. The final hex values (`#ffffff` /
  `#282a2c`) depend on the theme and dynamic color *(not checked on device)*.

### 4. Button sizing

- Figma fixes the button width at 88px. Code uses `minWidth` `88dp` and
  `maxWidth` `112dp` with wrapping text. `onMeasure` stretches the button to the
  full banner height, and the ripple is clipped to the end corners only (not a
  4px radius).

### 5. Multi-action layout

- Figma puts the settings icon before the divider and the action after it. Code
  uses the same order (`ListMenuButton` → divider → `ButtonCompat`). But code
  only shows the divider when a secondary icon is set, so a secondary action
  without a primary button still shows a divider at the end. That's a possible
  visual bug *(not verified at runtime)*.

### 6. Fonts

- Figma uses Google Sans Text (body) and Google Sans (button). Code uses the
  default `TextAppearance.TextLarge/TextSmall` (no `fontFamily`, so the system
  default upstream) and `@font/accent_font` for the button. Whether Chrome
  builds render Google Sans here depends on downstream font overrides *(not
  checked)*.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Code-only features

Large icon, rounded icon corners, description icon, progress spinner, the
pointer-hover close button, title/description max lines, and
`PRIMARY_BUTTON_TEXT_MAX_LINES` all exist in `MessageBannerProperties`, but none
of them are modeled in Figma.

### 2. Interactive states

- Figma models only the secondary action's `Hover` state (opening the menu). It
  doesn't model pressed, focused or disabled states for the primary button or
  the card.
- In code, `ButtonCompat` and `ListMenuButton` have ripple and focus states.
  Taps are ignored for 500ms after the message becomes visible
  (`TAP_PROTECTION_DURATION_MS`).

### 3. Stacking

Code can stack two messages (`MessageAnimationCoordinator`), with a
`@dimen/message_peeking_layer_height` `8dp` peek and a lower back elevation.
Figma's guidance doesn't show stacking.

### 4. Title is optional in code

`@id/message_title` defaults to `gone`. Figma treats the primary text as
required.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Consider a Message if your feature can be triggered predictably by a
  heuristic, offers simple actions, and doesn't make users finish a flow.
- **Navigation scope** (default): the Message only shows on the web page it
  relates to. Use this when the content is tied to a specific page.
- **Window scope**: the Message shows on any tab and page (but not in the tab
  switcher). Use it only when the content isn't related to the web page (for
  example, a HaTS survey). Try a bottom sheet or another contextual surface
  first.
- **Do**: Leave out the secondary text if one header is enough.
- **Do**: Use Messages for contextually helpful features or information the user
  should acknowledge. Use a snackbar instead if the acknowledgement follows a
  user action (like closing a tab).
- **Don't**: Use Messages for one-off promotions or promotions that don't fit
  the context.
- **Don't**: Use Messages for critical decisions that need the user's attention.
  Use a dialog instead.
- The action label is the most important element of the button. It should say
  what happens when the user taps it.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **Timeout**: The default is 10s. When a11y gestures are on, it's at least 30s,
  adjusted by `AccessibilityState.getRecommendedTimeoutMillis`.
- **TalkBack**: The leading icon is `importantForAccessibility="no"`. Give the
  secondary button a content description with
  `SECONDARY_ICON_CONTENT_DESCRIPTION`. Its traversal order is after the primary
  button. Use `TITLE_CONTENT_DESCRIPTION` if the visible title isn't a good
  spoken label.
- **Touch targets**: The secondary button is `48dp`, and the primary button's
  `minHeight` is `48dp`. Figma's 44px secondary action is smaller than this.
- **Dismissal**: Users swipe up or sideways. Pointer users get a hover close
  button when `ENABLE_CLOSE_BUTTON` is set.

### 3. Icon Usage Guidelines

- Use a 24dp monochrome leading icon tinted with the accent (primary) color. For
  full-color brand or site icons, use `ICON_TINT_COLOR = TINT_NONE` and maybe
  `LARGE_ICON`.
- The secondary icon is usually `settings` (gear) and opens a menu of options
  (for example, "Never for this site").
