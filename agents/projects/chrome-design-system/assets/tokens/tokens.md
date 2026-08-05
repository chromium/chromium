# Chrome Design System Token Mappings (Figma to C++ & CSS)

This document maps all 143 Figma design tokens cataloged in [figma-variables.md](./figma-variables.md) to their equivalent C++ identifiers (Views) and CSS variables (WebUI) in Chromium.

---

## 1. System & Surface Colors

Defined in `ui/color/color_id.h` and exposed in WebUI as CSS variables:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/sys/base-colors/base` | `#ffffff` | `ui::kColorSysBase` | `--color-sys-base` |
| `desktop/sys/base-colors/base-container` | `#edf2fa` | `ui::kColorSysBaseContainer` | `--color-sys-base-container` |
| `desktop/sys/base-colors/base-container-elevated` | `#ffffff` | `ui::kColorSysBaseContainerElevated` | `--color-sys-base-container-elevated` |
| `desktop/sys/surface-colors/inverse-on-surface` | `#f2f2f2` | `ui::kColorSysInverseOnSurface` | `--color-sys-inverse-on-surface` |
| `desktop/sys/surface-colors/inverse-primary` | `#a8c7fa` | `ui::kColorSysInversePrimary` | `--color-sys-inverse-primary` |
| `desktop/sys/surface-colors/inverse-surface` | `#303030` | `ui::kColorSysInverseSurface` | `--color-sys-inverse-surface` |
| `desktop/sys/surface-colors/inverse-surface-primary` | `#062e6f` | `ui::kColorSysInverseSurfacePrimary` | `--color-sys-inverse-surface-primary` |
| `desktop/sys/surface-colors/on-surface` | `#1f1f1f` | `ui::kColorSysOnSurface` | `--color-sys-on-surface` |
| `desktop/sys/surface-colors/on-surface-subtle` | `#474747` | `ui::kColorSysOnSurfaceSubtle` | `--color-sys-on-surface-subtle` |
| `desktop/sys/surface-colors/surface` | `#ffffff` | `ui::kColorSysSurface` | `--color-sys-surface` |
| `desktop/sys/surface-colors/surface-1` | `#f8fafd` | `ui::kColorSysSurface1` | `--color-sys-surface-1` |
| `desktop/sys/surface-colors/surface-5` | `#eaf0f9` | `ui::kColorSysSurface5` | `--color-sys-surface-5` |
| `desktop/sys/surface-colors/surface-variant` | `#e1e3e1` | `ui::kColorSysSurfaceVariant` | `--color-sys-surface-variant` |
| `gem/sys/surface-colors/on-surface` | `#1f1f1f` | `ui::kColorSysOnSurface` (or `kColorGlicModalForeground`) | `--color-sys-on-surface` |

---

## 2. Primary, Tertiary & Container Colors

Defined in `ui/color/color_id.h` and exposed in WebUI as CSS variables:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/sys/container-colors/neutral-container` | `#f2f2f2` | `ui::kColorSysNeutralContainer` | `--color-sys-neutral-container` |
| `desktop/sys/container-colors/on-tonal-container` | `#041e49` | `ui::kColorSysOnTonalContainer` | `--color-sys-on-tonal-container` |
| `desktop/sys/container-colors/tonal-container` | `#d3e3fd` | `ui::kColorSysTonalContainer` | `--color-sys-tonal-container` |
| `desktop/sys/primary-colors/on-primary` | `#ffffff` | `ui::kColorSysOnPrimary` | `--color-sys-on-primary` |
| `desktop/sys/primary-colors/primary` | `#0b57d0` | `ui::kColorSysPrimary` | `--color-sys-primary` |
| `desktop/sys/tertiary-colors/on-tertiary` | `#ffffff` | `ui::kColorSysOnTertiary` | `--color-sys-on-tertiary` |
| `desktop/sys/tertiary-colors/on-tertiary-container` | `#072711` | `ui::kColorSysOnTertiaryContainer` | `--color-sys-on-tertiary-container` |
| `desktop/sys/tertiary-colors/tertiary` | `#146c2e` | `ui::kColorSysTertiary` | `--color-sys-tertiary` |
| `desktop/sys/tertiary-colors/tertiary-container` | `#c4eed0` | `ui::kColorSysTertiaryContainer` | `--color-sys-tertiary-container` |

---

## 3. State, Ripple & Interaction Colors

Defined in `ui/color/color_id.h` and exposed in WebUI as CSS variables:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/sys/state-colors/state-disabled` | `#1f1f1f61` | `ui::kColorSysStateDisabled` | `--color-sys-state-disabled` |
| `desktop/sys/state-colors/state-disabled-container` | `#1f1f1f1f` | `ui::kColorSysStateDisabledContainer` | `--color-sys-state-disabled-container` |
| `desktop/sys/state-colors/state-focus-ring` | `#0b57d0` | `ui::kColorSysStateFocusRing` | `--color-sys-state-focus-ring` / `--cr-focus-outline-color` |
| `desktop/sys/state-colors/state-header-hover` | `#a8c7fa` | `ui::kColorSysStateHeaderHover` | `--color-sys-state-header-hover` |
| `desktop/sys/state-colors/state-hover-dim-blend-protection` | `#062e6f2e` | `ui::kColorSysStateHoverDimBlendProtection` | `--color-sys-state-hover-dim-blend-protection` |
| `desktop/sys/state-colors/state-hover-on-prominent` | `#fdfcfb1a` | `ui::kColorSysStateHoverOnProminent` | `--color-sys-state-hover-on-prominent` |
| `desktop/sys/state-colors/state-hover-on-subtle` | `#1f1f1f0f` | `ui::kColorSysStateHoverOnSubtle` | `--color-sys-state-hover-on-subtle` |
| `desktop/sys/state-colors/state-inactive-ring` | `#062e6f8c` | `ui::kColorSysStateInactiveRing` | `--color-sys-state-inactive-ring` |
| `desktop/sys/state-colors/state-on-header-hover` | `#062e6f` | `ui::kColorSysStateOnHeaderHover` | `--color-sys-state-on-header-hover` |
| `desktop/sys/state-colors/state-ripple-neutral-on-prominent` | `#fdfcfb29` | `ui::kColorSysStateRippleNeutralOnProminent` | `--color-sys-state-ripple-neutral-on-prominent` |
| `desktop/sys/state-colors/state-ripple-neutral-on-subtle` | `#1f1f1f14` | `ui::kColorSysStateRippleNeutralOnSubtle` | `--color-sys-state-ripple-neutral-on-subtle` |
| `desktop/sys/state-colors/state-ripple-primary` | `#7cacf852` | `ui::kColorSysStateRipplePrimary` | `--color-sys-state-ripple-primary` |

---

## 4. Outline, Divider & Component Colors

Defined in `ui/color/color_id.h` and exposed in WebUI as CSS variables:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/sys/component-colors/divider` | `#d3e3fd` | `ui::kColorSysDivider` | `--color-sys-divider` |
| `desktop/sys/component-colors/header` | `#d3e3fd` | `ui::kColorSysHeader` | `--color-sys-header` |
| `desktop/sys/component-colors/omnibox-container` | `#edf2fa` | `ui::kColorSysOmniboxContainer` | `--color-sys-omnibox-container` |
| `desktop/sys/component-colors/on-header-divider` | `#a8c7fa` | `ui::kColorSysOnHeaderDivider` | `--color-sys-on-header-divider` |
| `desktop/sys/component-colors/on-header-primary` | `#0b57d0` | `ui::kColorSysOnHeaderPrimary` | `--color-sys-on-header-primary` |
| `desktop/sys/doNotUse/on-surface-secondary` | `#474747` | `ui::kColorSysOnSurfaceSecondary` | `--color-sys-on-surface-secondary` |
| `desktop/sys/doNotUse/surface-variant` | `#e1e3e1` | `ui::kColorSysSurfaceVariant` | `--color-sys-surface-variant` |
| `desktop/sys/error-colors/error` | `#b3261e` | `ui::kColorSysError` | `--color-sys-error` |
| `desktop/sys/error-colors/on-error` | `#ffffff` | `ui::kColorSysOnError` | `--color-sys-on-error` |
| `desktop/sys/outline-colors/neutral-outline` | `#c7c7c7` | `ui::kColorSysNeutralOutline` | `--color-sys-neutral-outline` |
| `desktop/sys/outline-colors/outline` | `#747775` | `ui::kColorSysOutline` | `--color-sys-outline` |
| `desktop/sys/outline-colors/tonal-outline` | `#a8c7fa` | `ui::kColorSysTonalOutline` | `--color-sys-tonal-outline` |

---

## 5. Reference & Static Colors

Defined in `ui/color/color_id.h` and exposed in WebUI as CSS variables:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `CDDS/sys/light/surface` | `#FFFFFF` | `ui::kColorSysSurface` / `ui::kColorRefNeutral100` | `--color-sys-surface` |
| `desktop/ref/neutral-surface/neutral-surface-2` | `#f3f6fc` | `ui::kColorSysSurface2` / `ui::kColorRefNeutral96` | `--color-sys-surface-2` |
| `desktop/ref/neutral-surface/neutral-surface-3` | `#eff3fa` | `ui::kColorSysSurface3` / `ui::kColorRefNeutral94` | `--color-sys-surface-3` |
| `desktop/ref/neutral/neutral-0` | `#000000` | `ui::kColorRefNeutral0` | `--color-ref-neutral-0` |
| `desktop/ref/neutral/neutral-100` | `#ffffff` | `ui::kColorRefNeutral100` | `--color-ref-neutral-100` |
| `desktop/ref/neutral/neutral-60` | `#8f8f8f` | `ui::kColorRefNeutral60` | `--color-ref-neutral-60` |
| `desktop/ref/tertiary/tertiary-95` | `#e7f8ed` | `ui::kColorRefTertiary95` | `--color-ref-tertiary-95` |
| `desktop/sys/static-colors/black` | `#000000` | `ui::kColorSysBlack` | `--color-sys-black` |
| `desktop/sys/static-colors/white` | `#ffffff` | `ui::kColorSysWhite` | `--color-sys-white` |

---

## 6. Typography (Fonts, Sizes, Weights, Line Heights)

Implemented via `views::TypographyProvider` in C++ and CSS font properties in WebUI:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `GIC Icon Scale/Icon M` | `Font(...)` | **(none)** | `--cr-icon-size` |
| `Icon M` | `16` | **(none)** | `--cr-icon-size` |
| `desktop/font/body` | `Google Sans Text` | **(none)** | `--cr-primary-font-family` |
| `desktop/font/headline` | `Google Sans` | **(none)** | **(none)** |
| `desktop/font_size/body-five` | `11` | **(none)** | **(none)** |
| `desktop/font_size/body-four` | `12` | **(none)** | **(none)** |
| `desktop/font_size/body-three` | `13` | **(none)** | **(none)** |
| `desktop/font_size/body-two` | `14` | **(none)** | **(none)** |
| `desktop/font_size/button` | `13` | **(none)** | **(none)** |
| `desktop/font_size/headline-five` | `14` | **(none)** | **(none)** |
| `desktop/font_size/headline-four` | `16` | **(none)** | **(none)** |
| `desktop/font_size/headline-three` | `18` | **(none)** | **(none)** |
| `desktop/font_size/label` | `9` | **(none)** | **(none)** |
| `desktop/font_weight/bold` | `Bold` | `gfx::Font::Weight::BOLD` | **(none)** |
| `desktop/font_weight/medium` | `Medium` | `gfx::Font::Weight::MEDIUM` | **(none)** |
| `desktop/font_weight/regular` | `Regular` | `gfx::Font::Weight::NORMAL` | **(none)** |
| `desktop/line_height/body-five` | `16` | **(none)** | **(none)** |
| `desktop/line_height/body-four` | `18` | **(none)** | **(none)** |
| `desktop/line_height/body-three` | `20` | **(none)** | **(none)** |
| `desktop/line_height/body-two` | `20` | **(none)** | **(none)** |
| `desktop/line_height/button` | `20` | **(none)** | **(none)** |
| `desktop/line_height/headline-five` | `20` | **(none)** | **(none)** |
| `desktop/line_height/headline-four` | `24` | **(none)** | **(none)** |
| `desktop/line_height/headline-three` | `24` | **(none)** | **(none)** |
| `desktop/line_height/label` | `9` | **(none)** | **(none)** |
| `gem/icon-size/icon-M` | `16` | **(none)** | `--cr-icon-size` |
| `gem/letter-spacing/regular` | `0` | **(none)** | **(none)** |

---

## 7. Typography (Composite Text Styles)

Defined in `ui/views/style/typography.h`:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/body/five (medium)` | `Font(...)` | `views::style::STYLE_BODY_5_MEDIUM` | **(none)** |
| `desktop/body/five (regular)` | `Font(...)` | `views::style::STYLE_BODY_5` | **(none)** |
| `desktop/body/four (medium)` | `Font(...)` | `views::style::STYLE_BODY_4_MEDIUM` | **(none)** |
| `desktop/body/four (regular)` | `Font(...)` | `views::style::STYLE_BODY_4` | **(none)** |
| `desktop/body/three (bold)` | `Font(...)` | `views::style::STYLE_BODY_3_BOLD` | **(none)** |
| `desktop/body/three (medium)` | `Font(...)` | `views::style::STYLE_BODY_3_MEDIUM` | **(none)** |
| `desktop/body/three (regular)` | `Font(...)` | `views::style::STYLE_BODY_3` | **(none)** |
| `desktop/body/two (medium)` | `Font(...)` | `views::style::STYLE_BODY_2_MEDIUM` | **(none)** |
| `desktop/body/two (regular)` | `Font(...)` | `views::style::STYLE_BODY_2` | **(none)** |
| `desktop/headline/five` | `Font(...)` | `views::style::STYLE_HEADLINE_5` | **(none)** |
| `desktop/headline/four` | `Font(...)` | `views::style::STYLE_HEADLINE_4` | **(none)** |
| `desktop/headline/three` | `Font(...)` | `views::style::STYLE_HEADLINE_3` | **(none)** |
| `desktop/special/button` | `Font(...)` | `views::style::CONTEXT_BUTTON_MD` | **(none)** |
| `desktop/special/label` | `Font(...)` | `views::style::STYLE_CAPTION_BOLD` / `views::style::CONTEXT_BADGE` | **(none)** |

---

## 8. Spacing Tokens

Represented in `views::LayoutProvider` in C++ and CSS pixel values/variables in WebUI:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/spacing/2` | `2` | **(none)** | **(none)** |
| `desktop/spacing/3` | `3` | **(none)** | **(none)** |
| `desktop/spacing/4` | `4` | **(none)** | **(none)** |
| `desktop/spacing/5` | `5` | **(none)** | **(none)** |
| `desktop/spacing/6` | `6` | **(none)** | **(none)** |
| `desktop/spacing/7` | `7` | **(none)** | **(none)** |
| `desktop/spacing/8` | `8` | **(none)** | **(none)** |
| `desktop/spacing/9` | `9` | **(none)** | **(none)** |
| `desktop/spacing/10` | `10` | **(none)** | **(none)** |
| `desktop/spacing/12` | `12` | **(none)** | `--cr-button-edge-spacing` |
| `desktop/spacing/13` | `13` | **(none)** | **(none)** |
| `desktop/spacing/14` | `14` | **(none)** | **(none)** |
| `desktop/spacing/16` | `16` | **(none)** | `--cr-form-field-bottom-spacing` |
| `desktop/spacing/18` | `18` | **(none)** | **(none)** |
| `desktop/spacing/20` | `20` | **(none)** | `--cr-section-padding` |
| `desktop/spacing/24` | `24` | **(none)** | `--cr-controlled-by-spacing` |
| `desktop/spacing/32` | `32` | **(none)** | **(none)** |

---

## 9. Corner Radius Tokens

Defined in `views::LayoutProvider` via `ShapeSysTokens` / `Emphasis` and CSS `border-radius`:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/corner-radius/2` | `2` | **(none)** | **(none)** |
| `desktop/corner-radius/4` | `4` | `views::ShapeSysTokens::kXSmall` / `views::Emphasis::kLow` | **(none)** |
| `desktop/corner-radius/6` | `6` | **(none)** | **(none)** |
| `desktop/corner-radius/8` | `8` | `views::ShapeSysTokens::kSmall` / `views::Emphasis::kHigh` | `--cr-card-border-radius` |
| `desktop/corner-radius/10` | `10` | **(none)** | **(none)** |
| `desktop/corner-radius/12` | `12` | `views::ShapeSysTokens::kMediumSmall` | **(none)** |
| `desktop/corner-radius/16` | `16` | `views::ShapeSysTokens::kMedium` | **(none)** |
| `desktop/corner-radius/20` | `20` | **(none)** | **(none)** |
| `desktop/corner-radius/fully-rounded` | `999` | `views::ShapeSysTokens::kFull` / `views::Emphasis::kMaximum` | **(none)** |
| `gem/corner-radius/fully-rounded` | `100` | `views::ShapeSysTokens::kFull` / `views::Emphasis::kMaximum` | **(none)** |

---

## 10. Elevation & Shadows

Represented via `gfx::ShadowValue` / `views::Emphasis` in C++ and `--cr-elevation-*` in WebUI:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/elevation/3` | `Effect(...)` | `views::Emphasis::kHigh` / `ui::kColorShadowValue...ElevationThree` | `--cr-elevation-3` |
| `desktop/elevation/3/ambient/Blur` | `3` | **(none)** | **(none)** |
| `desktop/elevation/3/ambient/Spread` | `0` | **(none)** | **(none)** |
| `desktop/elevation/3/ambient/X` | `0` | **(none)** | **(none)** |
| `desktop/elevation/3/ambient/Y` | `1` | **(none)** | **(none)** |
| `desktop/elevation/3/key/Blur` | `8` | **(none)** | **(none)** |
| `desktop/elevation/3/key/Spread` | `3` | **(none)** | **(none)** |
| `desktop/elevation/3/key/X` | `0` | **(none)** | **(none)** |
| `desktop/elevation/3/key/Y` | `4` | **(none)** | **(none)** |
| `desktop/elevation/4` | `Effect(...)` | `ui::kColorShadowValue...ElevationFour` | `--cr-elevation-4` |
| `desktop/elevation/4/ambient/Blur` | `3` | **(none)** | **(none)** |
| `desktop/elevation/4/ambient/Spread` | `0` | **(none)** | **(none)** |
| `desktop/elevation/4/ambient/X` | `0` | **(none)** | **(none)** |
| `desktop/elevation/4/ambient/Y` | `2` | **(none)** | **(none)** |
| `desktop/elevation/4/key/Blur` | `10` | **(none)** | **(none)** |
| `desktop/elevation/4/key/Spread` | `4` | **(none)** | **(none)** |
| `desktop/elevation/4/key/X` | `0` | **(none)** | **(none)** |
| `desktop/elevation/4/key/Y` | `6` | **(none)** | **(none)** |

---

## 11. Miscellaneous & Gem Tokens

Defined in `ui/gfx/color_palette.h` / `chrome/browser/ui/color/chrome_color_id.h` and WebUI CSS:

| Figma Variable Name | Value / Definition | C++ Equivalent Identifier | CSS Equivalent Identifier |
| :--- | :--- | :--- | :--- |
| `desktop/admin/g/blue` | `#4285f4` | `gfx::kGoogleBlue500` | `--google-blue-500` |
| `desktop/admin/g/green` | `#34a853` | `gfx::kGoogleGreen500` | `--google-green-500` |
| `desktop/admin/g/red` | `#ea4335` | `gfx::kGoogleRed500` | `--google-red-500` |
| `desktop/admin/g/yellow` | `#fbbc05` | `gfx::kGoogleYellow500` | `--google-yellow-500` |
| `desktop/admin/tab-groups/bookmarks-bar/blue` | `#e8f0fe` | `kColorTabGroupBookmarkBarBlue` / `kColorSavedTabGroupForegroundBlue` | `--color-tab-group-bookmark-bar-blue` |
| `desktop/admin/tab-groups/tab-strip/blue` | `#1a73e8` | `kColorTabGroupTabStripFrameActiveBlue` / `kColorSavedTabGroupOutlineBlue` | `--color-tab-group-tab-strip-frame-active-blue` |
| `desktop/admin/tab-groups/tab-strip/green` | `#188038` | `kColorTabGroupTabStripFrameActiveGreen` / `kColorSavedTabGroupOutlineGreen` | `--color-tab-group-tab-strip-frame-active-green` |
| `gem/gradients/angular` | `""` | **(none)** | **(none)** |
| `gem/gradients/tab-strip` | `""` | **(none)** | **(none)** |
