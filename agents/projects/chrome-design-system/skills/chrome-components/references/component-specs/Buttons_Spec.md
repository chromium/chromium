# Component Spec: Buttons

This specification document outlines the mapping, design tokens, styling
variants, and interactive states of the **Buttons** component across Figma, C++
Views (Desktop), WebUI (Desktop), and Clank (Android).

______________________________________________________________________

## Overview

The **Buttons** component is a core interactive control that allows users to
trigger actions, execute commands, or submit forms. It supports text labels,
optional leading or trailing icons, and multiple levels of visual emphasis.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                       | C++ Views (Desktop)                                                                                                                                                                      | WebUI (Desktop)                                                                                                      | Clank (Android)                                                                                                                                                                                                            |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------ | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Buttons`                                                                                                                             | `views::MdTextButton` / `views::LabelButton`                                                                                                                                             | `<cr-button>`                                                                                                        | `ButtonCompat`                                                                                                                                                                                                             |
| **Source Files**   | [Figma Link: `20268:1070`](https://www.figma.com/design/qj3RvxSvSMVdw4tcH8GVMX/CDDS-Design-Kit---Core-UI--Chrome-?node-id=20268-1070) | [ui/views/controls/button/md_text_button.h](//src/ui/views/controls/button/md_text_button.h)<br>[ui/views/controls/button/label_button.h](//src/ui/views/controls/button/label_button.h) | [ui/webui/resources/cr_elements/cr_button/cr_button.ts](//src/ui/webui/resources/cr_elements/cr_button/cr_button.ts) | [ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java](//src/ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java)<br>[ui/android/java/res/values/styles.xml](//src/ui/android/java/res/values/styles.xml) |

______________________________________________________________________

## 2. Styling, Variants & Features (Layout & Style)

| Feature / Variant  | Figma Component    | C++ Views (Desktop)                                                      | WebUI (Web Frontend)                | Clank (Android)                                                               |
| :----------------- | :----------------- | :----------------------------------------------------------------------- | :---------------------------------- | :---------------------------------------------------------------------------- |
| **Primary Style**  | `Variant=Primary`  | `ui::ButtonStyle::kProminent`                                            | `<cr-button class="action-button">` | `style="@style/FilledButton"` / `R.style.FilledButtonThemeOverlay`            |
| **Tonal Style**    | `Variant=Tonal`    | `ui::ButtonStyle::kTonal`                                                | `<cr-button class="tonal-button">`  | `style="@style/FilledTonalButton"`                                            |
| **Outlined Style** | `Variant=Outlined` | `ui::ButtonStyle::kDefault`                                              | `<cr-button>` *(Default style)*     | `style="@style/OutlinedButton"`                                               |
| **Text Style**     | `Variant=Text`     | `ui::ButtonStyle::kText`                                                 | `<cr-button>` *(Flat style)*        | `style="@style/TextButton"` / `R.style.TextButtonThemeOverlay`                |
| **Leading Icon**   | `Icons=Leading`    | `LabelButton::SetImageModel()`                                           | Slot: `<slot name="prefix-icon">`   | `app:drawableStartCompat` / `setCompoundDrawablesRelativeWithIntrinsicBounds` |
| **Trailing Icon**  | `Icons=Trailing`   | `views::MdTextButtonWithDownArrow` or custom `LabelButtonImageContainer` | Slot: `<slot name="suffix-icon">`   | `app:drawableEndCompat` / `setCompoundDrawablesRelativeWithIntrinsicBounds`   |

______________________________________________________________________

## 3. Component States

| State                | Figma Component          | C++ Views (Desktop)                        | WebUI (Web Frontend)                                                         | Clank (Android)                                                                  |
| :------------------- | :----------------------- | :----------------------------------------- | :--------------------------------------------------------------------------- | :------------------------------------------------------------------------------- |
| **Default (Normal)** | `State=Default`          | `Button::ButtonState::STATE_NORMAL`        | Default state / no modifiers                                                 | `android:enabled="true"`                                                         |
| **Hovered**          | `State=Hovered`          | `Button::ButtonState::STATE_HOVERED`       | `:hover` pseudo-class<br>`#hoverBackground` overlay                          | State layer (`@dimen/default_hovered_alpha`) / pointer hover                     |
| **Pressed (Pushed)** | `State=Pressed`          | `Button::ButtonState::STATE_PRESSED`       | `:active` pseudo-class<br>`<cr-ripple>` (Ink ripple)                         | Ripple drawable (`@color/filled_button_ripple_color` / `RippleBackgroundHelper`) |
| **Disabled**         | `State=Disabled`         | `Button::ButtonState::STATE_DISABLED`      | Attribute: `<cr-button disabled>`                                            | Attribute: `android:enabled="false"` (`@dimen/default_disabled_alpha`)           |
| **Focused**          | *(Commonly represented)* | Triggers custom `views::FocusRing` drawing | `:focus` / `:focus-visible` pseudo-classes<br>`.focus-outline-visible` class | Focused outline / hardware focus ring (`FocusRing`)                              |

______________________________________________________________________

## 4. Design Token Comparison (Side-by-Side)

This table tracks how specific design tokens (colors, typography, spacing, and
shapes) defined in the Figma design system map directly to C++ Views, WebUI, and
Clank configurations.

| Design Attribute                 | Figma Design Token                                                               | C++ Views (Desktop)                                                          | WebUI (Web Frontend)                                                       | Clank (Android)                                                                                                      |
| :------------------------------- | :------------------------------------------------------------------------------- | :--------------------------------------------------------------------------- | :------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------- |
| **Primary Container Background** | `--desktop/sys/primary-colors/primary`<br>(`#0b57d0`)                            | `ui::kColorButtonBackgroundProminent`                                        | `--color-button-background-prominent`                                      | `@macro/filled_button_bg_color` / `?attr/globalFilledButtonBgColor` / `?attr/colorPrimary`                           |
| **On-Primary Foreground**        | `--desktop/sys/primary-colors/on-primary`<br>(`white`)                           | `ui::kColorButtonForegroundProminent`                                        | `--color-button-foreground-prominent`                                      | `@macro/default_text_color_on_accent1` / `?attr/globalFilledButtonTextColor` / `?attr/colorOnPrimary`                |
| **Tonal Container Background**   | `--desktop/sys/container-colors/tonal-container`<br>(`#d3e3fd`)                  | `ui::kColorButtonBackgroundTonal`                                            | `--color-button-background-tonal`                                          | `?attr/globalFilledTonalButtonBgColor` / `?attr/colorSecondaryContainer`                                             |
| **On-Tonal Foreground**          | `--desktop/sys/container-colors/on-tonal-container`<br>(`#041e49`)               | `ui::kColorButtonForegroundTonal`                                            | `--color-button-foreground-tonal`                                          | `?attr/globalFilledTonalButtonTextColor` / `?attr/colorOnSecondaryContainer`                                         |
| **Border Outline Color**         | `--desktop/sys/outline-colors/tonal-outline`<br>(`#a8c7fa`)                      | `ui::kColorButtonBorder`                                                     | `--color-button-border`                                                    | `?attr/globalOutlinedButtonBorderColor` / `?attr/colorOutline`                                                       |
| **Disabled Container BG**        | `--desktop/sys/state-colors/state-disabled-container`<br>(`rgba(31,31,31,0.12)`) | `ui::kColorButtonBackgroundProminentDisabled`                                | `--color-button-background-prominent-disabled`                             | `@dimen/filled_button_bg_disabled_alpha` (`0.12`) on button color                                                    |
| **Disabled Foreground**          | `--desktop/sys/state-colors/state-disabled`<br>(`rgba(31,31,31,0.38)`)           | `ui::kColorButtonForegroundDisabled`                                         | `--color-button-foreground-disabled`                                       | `@color/default_text_color_disabled_list` (`@dimen/default_disabled_alpha` = `0.38`)                                 |
| **Hover State Overlay**          | `--desktop/sys/state-colors/state-hover-on-prominent` / `state-hover-on-subtle`  | `ui::kColorButtonHoverBackgroundText`                                        | `--cr-hover-background-color` / `--cr-hover-on-prominent-background-color` | `@color/filled_button_ripple_color` / `@color/text_button_ripple_color_list_baseline`                                |
| **Font Family**                  | `--desktop/font/body`<br>(`"Google_Sans_Text:Medium"`)                           | Native font family resolved by `ui::ResourceBundle`                          | Default system font family                                                 | `sans-serif` / `@font/accent_font`                                                                                   |
| **Font Weight**                  | `--desktop/font_weight/medium`<br>(`500` / medium)                               | `MediumWeightForUI()` *(Returns `500`)*                                      | `font-weight: 500;` *(Hardcoded in `cr_button.css`)*                       | `android:fontFamily="sans-serif-medium"` / `@style/TextAppearance.AccentMediumStyle`                                 |
| **Font Size**                    | `--desktop/font_size/button`<br>(`13px`)                                         | `gfx::PlatformFont::GetFontSizeDelta(13)`                                    | `font-size: 13px;` *(Derived from base text/button settings)*              | `@dimen/text_size_medium` (`14sp`)                                                                                   |
| **Line Height**                  | `--desktop/line_height/button`<br>(`20px`)                                       | Handled via Typography Provider heights                                      | `line-height: 20px;` *(Default)* / `154%` *(For Prominent style)*          | `@dimen/text_size_medium_leading` (`20sp`)                                                                           |
| **Padding (Horizontal)**         | `--desktop/spacing/16`<br>(`16px` outer horizontal padding)                      | `DistanceMetric::kDistanceButtonHorizontalPadding`                           | `padding: 8px 16px;` *(Hardcoded in `cr_button.css`)*                      | `android:paddingStart="24dp"`, `android:paddingEnd="24dp"` (`@style/FilledButton`)                                   |
| **Padding (Vertical)**           | `--desktop/spacing/8`<br>(`8px` outer vertical padding)                          | `DistanceMetric::kDistanceButtonVerticalPadding`                             | `padding: 8px 16px;` *(Hardcoded in `cr_button.css`)*                      | `android:paddingTop="5dp"`, `android:paddingBottom="5dp"`, `verticalInset="@dimen/button_bg_vertical_inset"` (`4dp`) |
| **Gap (Icon-to-Label)**          | `--desktop/spacing/8`<br>(`8px`)                                                 | Handled internally in `LabelButton::GetImageLabelSpacing()`                  | `gap: 8px;` *(Hardcoded in `cr_button.css`)*                               | `android:drawablePadding="8dp"`                                                                                      |
| **Corner Radius**                | `--desktop/corner-radius/fully-rounded`<br>(`999px`)                             | `ShapeContextTokens::kButtonRadius` *(Resolves to fully pill-shaped button)* | `border-radius: 100px;` *(Pill style hardcoded in `cr_button.css`)*        | `@dimen/button_compat_corner_radius` (`500dp` pill shape via `RippleBackgroundHelper`)                               |

______________________________________________________________________

## 5. Architectural & Implementation Gaps

### 1. Shape & Corner Radius Gaps

- **Figma**: Uses `--desktop/corner-radius/fully-rounded` which is set to
  **`999px`** to enforce a safe, pill-shaped edge.
- **WebUI**: Implements a hardcoded **`border-radius: 100px;`** inside
  `cr_button.css`. While it visually achieves the same pill shape as `999px` for
  standard button heights, it does not use the CSS design token.
- **C++ Views**: Uses `ShapeContextTokens::kButtonRadius` which is resolved
  dynamically by the layout provider.
- **Clank (Android)**: Uses `@dimen/button_compat_corner_radius` set to
  **`500dp`** inside
  [`RippleBackgroundHelper`](//src/ui/android/java/src/org/chromium/ui/widget/RippleBackgroundHelper.java)
  to ensure a pill shape regardless of button height and font scaling.

### 2. Tonal Color Values (Static vs. Dynamic)

- **Figma**: Has static hardcoded hexadecimal values for tonal containers:
  background is **`#d3e3fd`** (`tonal-container`) and text is **`#041e49`**
  (`on-tonal-container`).
- **C++ Views / WebUI / Clank**: All three frameworks bind these properties to
  the dynamic color pipeline (`kColorSysTonalContainer` in Views,
  `--color-button-background-tonal` in WebUI, and
  `?attr/globalFilledTonalButtonBgColor` / `?attr/colorSecondaryContainer` in
  Clank). These colors are **dynamically generated** at runtime based on the
  user's active theme or dynamic Android wallpaper palette.

### 3. Touch Target Sizing (Mobile vs. Desktop)

- **Desktop (Views & WebUI)**: Buttons optimize for precision pointer clicking
  with standard ~`32px`-`36px` heights.
- **Clank (Android)**: Buttons enforce a strict minimum touch target of
  **`48dp`** (`@dimen/min_touch_target_size`) with a minimum width of **`88dp`**
  (`@dimen/button_min_width`), utilizing `button_bg_vertical_inset="4dp"` to
  align visually with adjacent components while preserving the touch target.

______________________________________________________________________

## 6. Styling, Variants, Features and States Mismatches

### 1. Styling & Variant Mismatches

- **The "Text" (Flat) Variant**:
  - **Figma** has a dedicated flat `"Text"` variant (no border, transparent
    background).
  - **C++ Views** has `ui::ButtonStyle::kText`.
  - **Clank (Android)** has `@style/TextButton` with transparent background and
    blue ripple.
  - **WebUI's `<cr-button>` does not have a built-in class** (like
    `.text-button` or `.flat-button`) to automatically remove the border and
    background; developers must manually override custom properties inline.

### 2. Feature & Layout Mismatches (Icons)

- **Simultaneous Leading & Trailing Icons**:
  - **WebUI**: Supports `<slot name="prefix-icon">` and
    `<slot name="suffix-icon">` simultaneously.
  - **Clank (Android)**: Supports both via `drawableStart` and `drawableEnd` /
    `setCompoundDrawablesRelativeWithIntrinsicBounds`.
  - **C++ Views**: Built around a single image model (`SetImageModel()`),
    requiring a custom layout container to support both icons simultaneously.

### 3. State Mismatches

- **Focus Ring (Accessibility)**:
  - The **Figma** component variant set does not model a `"Focused"` state.
  - **C++ Views**, **WebUI**, and **Clank** explicitly implement visible focus
    rings (`views::FocusRing`, `.focus-outline-visible`, and hardware focus
    rings).

______________________________________________________________________

## 7. Usage & UX Guidance

### 1. General Principles & Best Practices

- **Visual Hierarchy & Variant Selection**: Use primary actions selectively
  (`FilledButton` / `kProminent` / `.action-button`).
- **Action Pairing & Layout**:
  - Desktop: Cancel buttons on the left, action/submit buttons on the right.
  - Clank (Android): Managed by
    [`DualControlLayout`](//src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/DualControlLayout.java),
    placing primary action at the end/top and automatically stacking vertically
    on narrow screens.
- **Labeling**: Use strong, active action verbs. Sentence case for WebUI and
  Android, Title Case for Desktop Views.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- **WebUI**: `<cr-button>` enforces `role="button"` and `tabindex="0"`. Handles
  space/enter natively to click.
- **C++ Views**: Use `GetViewAccessibility().SetName()` to feed accessibility
  strings. Set accelerators for Cancel (`Esc`) and Default (`Enter`) buttons.
- **Clank (Android)**: Set `android:contentDescription`, support TalkBack focus
  navigation, and maintain min 48dp touch target
  (`@dimen/min_touch_target_size`).

### 3. Icon Usage Guidelines

- **Leading Icons**: Categorize and add visual weight to recurring layout items.
- **Trailing Icons**: Reserved for state indicators (e.g. dropdown arrows).
