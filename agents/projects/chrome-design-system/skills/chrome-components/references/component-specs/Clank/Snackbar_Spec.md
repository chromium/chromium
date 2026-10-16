# Snackbar Spec (Clank)

This document maps the specifications of the **Snackbar** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

A **Snackbar** is a short, temporary message anchored to the bottom of the
screen, with at most one optional text action (never "Dismiss" or "Cancel").
It's a Chrome-drawn element that mimics System UI. Figma models one look: a
`surface-container-high` card with a 16dp corner radius, one line of
`textMedium` text, and an optional `Button` (`Type=Text`). In code, a `Snackbar`
data object is queued by `SnackbarManager` and rendered by `SnackbarView`
(`floating_snackbar.xml`).

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                         | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Snackbar`                                                                                                                                              | `SnackbarView`<br>`Snackbar`<br>`SnackbarManager`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Source Files**   | [Figma Link: `Snackbar` (`7104:51532`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7104-51532&m=dev) | [chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/SnackbarView.java](//src/chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/SnackbarView.java)<br>[chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/Snackbar.java](//src/chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/Snackbar.java)<br>[chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/SnackbarManager.java](//src/chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/SnackbarManager.java) |

> Source page:
> [`↳ Snackbars` (`7201:64405`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7201-64405)
> is a documentation canvas. It holds a guidance card (`Guidance/Snackbars`,
> `39971:81849`) and a light/dark showcase (`Components/Snackbars`,
> `43256:69650`) that contain only instances. The main component (`7104:51532`)
> is on the `↳ Core components` page.
>
> The nested action is an instance of `Button` (`7015:48330`), which is covered
> by [Button_Spec_Clank.md](./Button_Spec_Clank.md).
>
> Supporting code: layout
> [floating_snackbar.xml](//src/chrome/browser/ui/messages/android/java/res/layout/floating_snackbar.xml),
> background
> [snackbar_background.xml](//src/chrome/browser/ui/messages/android/java/res/drawable/snackbar_background.xml),
> dimensions in
> [dimens.xml](//src/chrome/browser/ui/messages/android/java/res/values/dimens.xml)
> and
> [values-sw600dp/dimens.xml](//src/chrome/browser/ui/messages/android/java/res/values-sw600dp/dimens.xml),
> swipe-to-dismiss in
> [SnackbarSwipeHandler.java](//src/chrome/browser/ui/messages/android/java/src/org/chromium/chrome/browser/ui/messages/snackbar/SnackbarSwipeHandler.java),
> and the action button
> [ButtonCompat.java](//src/ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java)
> (`@style/TextButton`).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Snackbar`: no variant properties (a standalone `COMPONENT`). It has one
  boolean property, `Action` (true | false, default `true`), which shows or
  hides the nested `Button` instance (`Type=Text`, `State=Enabled`).

| Feature / Variant                  | Figma Component                                                                                                   | Clank (Android)                                                                                                                                           |
| :--------------------------------- | :---------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Message only**                   | `Action=false`                                                                                                    | `Snackbar.make(text, controller, type, umaId)` with no `setAction(...)`. `SnackbarView` sets the button to `GONE` and gives the message a 24dp end margin |
| **Message + action**               | `Action=true`                                                                                                     | `Snackbar#setAction(actionText, actionData)`. `ButtonCompat` with `@style/TextButton` (24dp side padding from `snackbar_button_padding`)                  |
| **Light / dark theme**             | Light and dark showcase                                                                                           | Theme attributes (`?attr/colorSurfaceContainerHigh`, `?attr/colorOnSurface`, accent1/primary) resolve per theme                                           |
| **Multi-line message**             | *(Not modeled, 1 line)*                                                                                           | `maxLines` is 2 by default (`DEFAULT_LINES`), or 5 (`MAX_LINES`) when `setDefaultLines(false)`. Ends with `ellipsize="end"`                               |
| **Leading profile image**          | *(Not modeled)*                                                                                                   | `Snackbar#setProfileImage(Drawable)`: a 24dp `ImageView` before the message                                                                               |
| **Google theme**                   | *(Not modeled)*                                                                                                   | `Snackbar#setTheme(Theme.GOOGLE)`: `default_control_color_active_baseline` background, `TextMedium.OnAccent1` text, `Button.Text.Filled` action           |
| **Custom background / text style** | *(Not modeled)*                                                                                                   | `Snackbar#setBackgroundColor(int)` / `Snackbar#setTextAppearance(int)` (BASIC theme only)                                                                 |
| **Type / priority**                | *(Not modeled)*                                                                                                   | `TYPE_ACTION`, `TYPE_NOTIFICATION`, `TYPE_PERSISTENT` (queued by `SnackbarCollection`)                                                                    |
| **Duration**                       | *(Not modeled)*                                                                                                   | `DEFAULT_SNACKBAR_DURATION_MS` 3000, `DEFAULT_SNACKBAR_DURATION_LONG_MS` 8000, `DEFAULT_TYPE_ACTION_SNACKBAR_DURATION_MS` 10000, a11y mode 30000          |
| **Action button states**           | `Button` `State` (Enabled, Hover, Focused, Pressed, Disabled, Focused talkback), fixed to `Enabled` in `Snackbar` | `ButtonCompat` ripple and focus states (see [Button_Spec_Clank.md](./Button_Spec_Clank.md))                                                               |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                 | Figma Design Token                                                                                   | Clank (Android)                                                                                                                                                                          | Match                                 |
| :------------------------------- | :--------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------ |
| **Container background**         | `clank/sys/color/scontainerhigh` (`#E9EEF6` light / `#282A2C` dark)                                  | `SemanticColorUtils.getFloatingSnackbarBackgroundColor()` → `?attr/colorSurfaceContainerHigh` (also the drawable's default `solid`)                                                      | ✅                                    |
| **Corner radius**                | `clank/shape/corner_radius/16` (16px)                                                                | `@dimen/snackbar_rounded_corner_radius` = 16dp                                                                                                                                           | ✅                                    |
| **Shadow / elevation**           | Effect `Current/infobar, snackbar`: drop shadows `#0000004D` blur 6 and `#0000001A` blur 1, offset 0 | `android:elevation="@dimen/snackbar_elevation"` = 2dp (system shadow)                                                                                                                    | ⚠️ Approximate                        |
| **Message text color**           | `clank/sys/color/onsurface` (`#1F1F1F` / `#E3E3E3`)                                                  | `TextAppearance.TextMedium.Primary` → `@color/default_text_color_list` → `?attr/colorOnSurface`                                                                                          | ✅                                    |
| **Message typography**           | `textMedium`: `font_family/alternative` (Google Sans Text), Regular 400, 14px / 20px, 0 tracking     | `TextAppearance.TextMedium`: `@dimen/text_size_medium` 14sp, `leading` `@dimen/text_size_medium_leading` 20sp. No `fontFamily` set (default font)                                        | ⚠️ Font family not checked downstream |
| **Message vertical padding**     | `clank/shape/spacing/14` (14px top and bottom)                                                       | `@dimen/snackbar_message_margin` = 14dp (`marginTop` / `marginBottom`)                                                                                                                   | ✅                                    |
| **Container start padding**      | `clank/shape/spacing/16` (16px, `pl` on the container)                                               | `@dimen/snackbar_text_view_margin` = 24dp (message `marginStart`)                                                                                                                        | ❌ 24dp vs 16dp                       |
| **Text → action gap**            | `clank/shape/spacing/16` (text `pr`) + 24px button padding = 40px to the label                       | Message `marginEnd` = 0 when there's an action, + 24dp button padding = 24dp to the label                                                                                                | ❌ 24dp vs 40dp                       |
| **End padding (no action)**      | `clank/shape/spacing/16` (text `pr`; container has no end padding)                                   | Message `marginEnd` = `@dimen/snackbar_text_view_margin` = 24dp                                                                                                                          | ❌ 24dp vs 16dp                       |
| **Min height**                   | 48px (14 + 20 + 14, or 4 + 40 + 4 with the button)                                                   | `@dimen/snackbar_min_height` = 48dp                                                                                                                                                      | ✅                                    |
| **Action label color**           | `clank/sys/color/primary` (`#0B57D0`)                                                                | `TextButton` `buttonTextColor` = `?attr/globalTextButtonTextColor` → `@color/default_text_color_accent1_tint_list` (`@macro/default_text_color_accent1`)                                 | ✅                                    |
| **Action typography**            | `buttonText`: `font_family/default` (Google Sans), Medium 500, 14px / 20px                           | `TextAppearance.Button.Text.Blue` → `TextAccentMediumThick` (14sp, 20sp leading, `AccentMediumStyle` = `sans-serif-medium` upstream, overridden downstream)                              | ✅ (size/weight)                      |
| **Action horizontal padding**    | `clank/shape/spacing/24` (24px)                                                                      | `@dimen/snackbar_button_padding` = 24dp (overrides `TextButton`'s 8dp)                                                                                                                   | ✅                                    |
| **Action height / touch target** | 40px pill (`rounded-[100px]`) with `clank/shape/spacing/4` vertical padding (48px total)             | `ButtonCompatBase`: `minHeight` = `@dimen/min_touch_target_size` 48dp, `verticalInset` = `@dimen/button_bg_vertical_inset` 4dp, ripple radius `@dimen/button_compat_corner_radius` 500dp | ✅                                    |
| **Screen side margins**          | 16dp (guidance text and measurement)                                                                 | Width = `min(@dimen/snackbar_width_max 480dp, parentWidth − 2 × @dimen/snackbar_floating_margin)`; 16dp on phones, 24dp on `sw600dp`. Centered                                           | ✅ phone (tablet 24dp not in Figma)   |
| **Max width**                    | *(Not specified; showcase uses 412px)*                                                               | `@dimen/snackbar_width_max` = 480dp                                                                                                                                                      | ➖                                    |
| **Bottom offset**                | "Anchored to the bottom of the screen" (no value)                                                    | Layout `marginBottom` = `@dimen/snackbar_margin` 8dp + additional bottom margin supplier (+ keyboard height in fullscreen)                                                               | ➖ Not specified in Figma             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

- **Data + manager + view split**: Figma has one visual component. In code,
  features build a `Snackbar` (data, builder-style setters), hand it to
  `SnackbarManager#showSnackbar`, and `SnackbarManager` creates or reuses one
  `SnackbarView`. There's no standalone widget to drop into a layout.
- **Horizontal padding**: Code uses 24dp at the start (and at the end with no
  action), but Figma uses 16dp. With an action, code puts no gap between the
  text and the button, so the text sits 24dp from the label. Figma's gap is 40dp
  (16 + 24).
- **Elevation vs. custom shadow**: Figma uses a two-layer, zero-offset shadow
  effect. Code uses `android:elevation` 2dp, which renders the platform's
  directional shadow.
- **Typography font family**: Figma uses Google Sans Text for the message and
  Google Sans for the action. Upstream, `TextAppearance.TextMedium` sets no font
  family and `AccentMediumStyle` is `sans-serif-medium`. Downstream overrides
  may change these. That wasn't checked.
- **Unused layout text appearance**: `floating_snackbar.xml` sets
  `TextAppearance.TextMedium.Primary.Baseline.Light` on the message, but
  `SnackbarView#updateInternal` always replaces it with `getTextAppearance()`.
- **Tablet margin**: `values-sw600dp` raises the side margin to 24dp. Figma only
  documents 16dp.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

- **Features in code that Figma doesn't model**: the `Theme.GOOGLE` styling
  (static `default_control_color_active_baseline` background, see
  crbug.com/40798080), the leading profile image, custom background color and
  text appearance overrides, multi-line messages (2 or 5 lines, ellipsized), and
  swipe-to-dismiss (`SnackbarSwipeHandler`).
- **Possible bug, action text style not reset on view reuse**:
  `getButtonTextAppearance()` returns `R.style.TextButton` for `BASIC`. That's a
  widget style, not a `TextAppearance`. Its text attributes are nested under
  `android:textAppearance`, so `TextView#setTextAppearance(R.style.TextButton)`
  doesn't apply them. `SnackbarManager#updateView` reuses the same
  `SnackbarView` via `update()`. So if a `GOOGLE` snackbar (which sets
  `TextAppearance.Button.Text.Filled`, an on-accent color) is replaced by a
  `BASIC` one, the action label can keep the on-accent text color on a
  `surface-container-high` background. That would mean low contrast. I found
  this by reading the code and didn't reproduce it.
- **Action button states**: Figma pins the nested button to `State=Enabled`.
  Hover, focus, and pressed come from the shared `ButtonCompat` ripple and focus
  handling. No snackbar-specific state is specified.
- **Touch handling**: The whole container is clickable. Tapping it resets the
  timeout (`manager.resetSnackbarTimeout()`) and touches are ignored while it
  animates. Figma doesn't specify this.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use a snackbar for brief, low-priority feedback about an operation (for
  example "Tab closed" with "Undo"). It shouldn't hold critical information.
- Use at most one action. The action is optional, and it must **never** be
  "Dismiss" or "Cancel" (per Figma guidance). Snackbars time out on their own
  and can be swiped away.
- Snackbars span the viewport width with 16dp side margins (24dp on tablets in
  code, up to 480dp wide) and sit at the bottom of the screen.
- Keep the message short. Code allows 2 lines by default (5 at most), but Figma
  shows a single line.
- Pick the `Snackbar` type to match priority: `TYPE_ACTION` for undo-style
  actions, `TYPE_NOTIFICATION` for informational messages, and `TYPE_PERSISTENT`
  only when the snackbar must stay until it's dismissed in code.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- `SnackbarView#updateAccessibilityPaneTitle` announces the message, the action
  text, and the "bottom bar" screen position string through
  `ViewCompat.setAccessibilityPaneTitle` (posted, see crbug.com/395925721).
- The action button's `contentDescription` is set to the action text.
- When accessibility services are on, the timeout stretches to
  `ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS` (30s).
- The action is a `ButtonCompat` with a 48dp minimum height
  (`@dimen/min_touch_target_size`), which meets the touch-target guideline.

### 3. Icon Usage Guidelines

- Figma defines no icon slot. Code only supports a 24dp leading profile image
  (`setProfileImage`), which should be limited to account or identity contexts.

______________________________________________________________________

> [!NOTE] **Not checked:** downstream font overrides (Google Sans and Google
> Sans Text), the exact dark-theme colors that the dynamic theme attributes
> resolve to, show/hide animation timing, `Theme.GOOGLE` contrast, and rendered
> output. There were no screenshots or render tests. Desktop (Views/WebUI)
> doesn't apply because this is a Clank-only component.
