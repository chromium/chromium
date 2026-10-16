# IPH Spec (Clank)

This document maps the specifications of the **IPH** (In-product help bubble)
(Clank) component across Figma and code.

______________________________________________________________________

## Overview

The **IPH** bubble tells the user about a change or new feature in the UI. It's
ephemeral, so it shouldn't hold critical information, and it normally has a
single string with no headline (per the Figma guidance). Figma models one fixed
look: a `primary` filled bubble with an arrow and an optional circular highlight
on the anchor. In code this is `TextBubble` (drawn with `ArrowBubbleDrawable`
and shown in an `AnchoredPopupWindow`), plus an optional pulse from
`ViewHighlighter`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                              | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `IPH`                                                                                                                                        | `TextBubble`<br>`ArrowBubbleDrawable`<br>`ViewHighlighter` (highlight)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Source Files**   | [Figma Link: `IPH` (`7145:36`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=7145-36&m=dev) | [components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/TextBubble.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/ArrowBubbleDrawable.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/textbubble/ArrowBubbleDrawable.java)<br>[components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/highlight/ViewHighlighter.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/highlight/ViewHighlighter.java) |

> Source page:
> [`↳ IPH + New Label` (`6371:41111`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-41111)
> is a documentation canvas with a guidance card and component showcase
> instances (`43247:58395`, `63174:48359`). The same page also documents
> `New-label` (see [NewLabel_Spec_Clank.md](./NewLabel_Spec_Clank.md)).
>
> The Figma component description says: "Code reference: TextBubble. Circle
> highlight is not required."
>
> Supporting code: content layout
> [textbubble_text.xml](//src/components/browser_ui/widget/android/java/res/layout/textbubble_text.xml)
> (and `textbubble_text_with_image.xml`), dimensions in
> [dimens.xml](//src/components/browser_ui/widget/android/java/res/values/dimens.xml),
> window positioning in
> [AnchoredPopupWindow.java](//src/ui/android/java/src/org/chromium/ui/widget/AnchoredPopupWindow.java),
> pulse in
> [PulseDrawable.java](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/highlight/PulseDrawable.java).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `IPH`: *None* (standalone `COMPONENT`, no variant properties). It has three
  layers: `IPH bubble` (text), `Triangle` (arrow), and `Highlight` (40px
  circle).

| Feature / Variant            | Figma Component                       | Clank (Android)                                                                                                       |
| :--------------------------- | :------------------------------------ | :-------------------------------------------------------------------------------------------------------------------- |
| **Default (primary bubble)** | `IPH`                                 | `new TextBubble.Builder(...).build()` (`inverseColor=false`)                                                          |
| **Arrow**                    | `Triangle` layer (above bubble, left) | `Builder#setShowArrow(true)`; `ArrowBubbleDrawable` places the arrow above or below the anchor and flips it as needed |
| **No arrow**                 | *(Not modeled)*                       | `setShowArrow(false)`                                                                                                 |
| **Anchor highlight**         | `Highlight` layer (optional)          | `ViewHighlighter.turnOnHighlight(view, HighlightParams)` → `PulseDrawable` (circle or rounded rect)                   |
| **Inverse colors**           | *(Not modeled)*                       | `Builder#setInverseColor(true)` (surface bg, primary text)                                                            |
| **Round (pill) bubble**      | *(Not modeled)*                       | `Builder#setIsRoundBubble(true)` (radius = height / 2, no arrow)                                                      |
| **Leading image**            | *(Not modeled)*                       | `Builder#setImageDrawable(...)` → `textbubble_text_with_image.xml`                                                    |
| **Snooze / "Got it" button** | *(Not modeled)*                       | `Builder#setSnoozeRunnable` / `setSnoozeDismissRunnable` → `ButtonCompat` `@style/TextButton.OnAccent1.Snooze`        |
| **Auto-dismiss / focusable** | *(Not modeled)*                       | `setAutoDismissTimeout(ms)`, `setFocusable(bool)`, `setDismissOnTouchInteraction(bool)`                               |
| **Dark theme**               | *(Not shown on this page)*            | Night-mode resolution of `?attr/colorPrimary` / `?attr/colorOnPrimary`                                                |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute           | Figma Design Token                                                                                                | Clank (Android)                                                                                                                                                                                                                                     |
| :------------------------- | :---------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Bubble background**      | `--clank/sys/color/primary` (light `#0b57d0`)                                                                     | `ArrowBubbleDrawable#setBubbleColor(SemanticColorUtils.getDefaultControlColorActive())` → `?attr/colorPrimary`. Inverse: `getDefaultBgColor()` → `?attr/colorSurface`                                                                               |
| **Arrow fill**             | Same as bubble (`Triangle` vector)                                                                                | Same paint color as bubble (`mArrowPaint.setColor(color)`)                                                                                                                                                                                          |
| **Text color**             | `--clank/sys/color/onPrimary, op` (`#ffffff`)                                                                     | `@style/TextAppearance.TextMediumThick.OnAccent1` → `@color/default_text_color_on_accent1_list` → `?attr/colorOnPrimary`. Inverse: `TextMediumThick.Accent1` (primary)                                                                              |
| **Typography**             | `TextMediumThick`: Google Sans Text Medium, 14 / 20, weight 500, tracking 0 (layer named `TextAccentMediumThick`) | `TextAppearance.TextMediumThick` (parent `MediumStyle` = `sans-serif-medium`), `@dimen/text_size_medium` `14sp`, `@dimen/text_size_medium_leading` `20sp`                                                                                           |
| **Padding**                | `--clank/shape/spacing/16` (16px, all sides)                                                                      | `android:padding="16dp"` on `@id/message`. With snooze: start `24dp` / end `4dp` (`text_bubble_with_snooze_padding_*`)                                                                                                                              |
| **Corner radius**          | `--clank/shape/corner_radius/8` (8px)                                                                             | `@dimen/text_bubble_corner_radius` `8dp` (round bubble: height / 2)                                                                                                                                                                                 |
| **Arrow size**             | `20 × 8` px                                                                                                       | `@dimen/text_bubble_arrow_width` `20dp` × `@dimen/text_bubble_arrow_height` **`10dp`**                                                                                                                                                              |
| **Arrow position**         | Offset to the left (x≈14.5px), under the highlight                                                                | Computed: centered on the anchor and clamped to bubble edges (`AnchoredPopupWindow`, `HorizontalOrientation.CENTER`)                                                                                                                                |
| **Anchor gap**             | 4px between highlight circle bottom and arrow tip                                                                 | `@dimen/text_bubble_margin` `4dp`                                                                                                                                                                                                                   |
| **Highlight color**        | `--clank/macro/iph/iph_highlight_color` (`#0b57d0`; fill opacity is in the vector asset, not checked)             | `PulseDrawable#setUseLightPulseColor(false)` → `SemanticColorUtils.getDefaultIconColorAccent1()`, with animated alpha. Resource `@color/iph_highlight_color` exists (`color_primary_with_alpha_10`, night `_50`) but `PulseDrawable` doesn't use it |
| **Highlight shape / size** | Circle, 40px, `--clank/shape/corner_radius/fullyRounded` (999)                                                    | `PulseDrawable.createCircle`, baseline radius `@dimen/iph_pulse_baseline_radius` `16dp` (32dp, then pulses larger); or `createRoundedRectangle`                                                                                                     |
| **Elevation / shadow**     | None                                                                                                              | `TextBubble` doesn't set elevation on `AnchoredPopupWindow` *(default not checked)*                                                                                                                                                                 |
| **Enter / exit motion**    | *(Not modeled)*                                                                                                   | `@style/TextBubbleAnimation` (`@anim/textbubble_in` / `textbubble_out`)                                                                                                                                                                             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Arrow height (8 vs. 10)

- **Figma**: The `Triangle` is `20 × 8` px.
- **Clank**: `text_bubble_arrow_height` is `10dp`, so the code arrow is 2dp
  taller (and a bit sharper).

### 2. Font family

- **Figma**: Google Sans Text Medium.
- **Clank**: `TextAppearance.MediumStyle` is `sans-serif-medium` upstream. Only
  `AccentMediumStyle` has a comment saying it's overridden downstream. Whether
  Chrome builds render Google Sans Text here depends on downstream theming *(not
  checked)*.

### 3. Highlight is a separate system

- The Figma `Highlight` is a layer inside the IPH component. In code the
  highlight is a separate API (`ViewHighlighter` / `PulseDrawable`) applied to
  the anchor view by the feature (for example `MenuButton`,
  `OptionalButtonCoordinator`), not by `TextBubble`.
- Code pulses the highlight (animated radius and alpha) and uses a 16dp baseline
  radius. Figma shows a static 40px circle.
- The color source differs: code uses `getDefaultIconColorAccent1` with animated
  alpha. The `@color/iph_highlight_color` resource (primary @ 10% light / 50%
  night) isn't used by `PulseDrawable`.

### 4. Arrow placement is dynamic

- Figma fixes the arrow at the top-left. Code computes it from the anchor rect:
  above or below, centered with clamping. The Figma layout is one possible
  result.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Code-only variants

Inverse colors, round (pill) bubble, leading image, snooze / "Got it" button,
and no-arrow bubbles all exist in `TextBubble.Builder`, but none of them are
modeled in Figma.

### 2. Interactive states

The Figma bubble has no states. In code, the bubble dismisses on outside touch
(or inside touch with `setDismissOnTouchInteraction`). It can also be focusable,
and the snooze/dismiss `ButtonCompat` has ripple and focus states.

### 3. Text width

Figma uses `whitespace-nowrap` in the instance. Code wraps the text with
`TextViewWithTightWrap` (`layout_weight=1`), and `AnchoredPopupWindow`
constrains the width.

### 4. Dark mode

This page only shows light mode, so dark token values weren't verified *(not
checked)*.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use IPH only for non-critical, ephemeral announcements of new or changed UI,
  since the bubble may be dismissed or never seen.
- Keep it to one short string with no headline. Bubbles with a title and body
  usually go unread.
- Anchor the bubble to the feature it describes. The circle highlight is
  optional.
- Gate IPH with Feature Engagement (`Tracker`) so it doesn't show too often or
  pile up with other bubbles.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **TalkBack**: `TextBubble` takes a separate `accessibilityString`, which is
  shown when touch exploration is on. In that mode the bubble dismisses on any
  touch, and auto-dismiss timeouts are ignored.
- **Focus**: Use `setFocusable(true)` when the bubble has an actionable button
  (snooze / "Got it") so keyboard and switch users can reach it.
- **Touch target**: Snooze/dismiss buttons use `ButtonCompat` and must keep a
  `48dp` minimum touch target.

### 3. Icon Usage Guidelines

- The optional leading image (`setImageDrawable`) is tinted primary in inverse
  mode. Use simple monochrome icons.
- The Figma component doesn't model icons.
