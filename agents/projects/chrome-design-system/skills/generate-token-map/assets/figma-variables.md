# CDDS Design Kit \- Core UI (Chrome) \- Variable Definitions

This document catalogs all **143 design variables** extracted from the **CDDS Design Kit — Core UI (Chrome)** Figma file (`4474:11230`).

The variables are organized into logical design token categories for easy reference and implementation across Chromium WebUI and C++ Views.

## Summary Table of Contents

- [1\. System & Surface Colors](#1-system--surface-colors) (`14` variables)
- [2\. Primary, Tertiary & Container Colors](#2-primary-tertiary--container-colors) (`9` variables)
- [3\. State, Ripple & Interaction Colors](#3-state-ripple--interaction-colors) (`12` variables)
- [4\. Outline, Divider & Component Colors](#4-outline-divider--component-colors) (`12` variables)
- [5\. Reference & Static Colors](#5-reference--static-colors) (`9` variables)
- [6\. Typography (Fonts, Sizes, Weights, Line Heights)](#6-typography-fonts-sizes-weights-line-heights) (`27` variables)
- [7\. Typography (Composite Text Styles)](#7-typography-composite-text-styles) (`14` variables)
- [8\. Spacing Tokens](#8-spacing-tokens) (`17` variables)
- [9\. Corner Radius Tokens](#9-corner-radius-tokens) (`10` variables)
- [10\. Elevation & Shadows](#10-elevation--shadows) (`18` variables)
- [11\. Miscellaneous & Gem Tokens](#11-miscellaneous--gem-tokens) (`9` variables)

---

## 1\. System & Surface Colors

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/sys/base-colors/base` | `#ffffff` | `#ffffff`  |
| `desktop/sys/base-colors/base-container` | `#edf2fa` | `#edf2fa`  |
| `desktop/sys/base-colors/base-container-elevated` | `#ffffff` | `#ffffff`  |
| `desktop/sys/surface-colors/inverse-on-surface` | `#f2f2f2` | `#f2f2f2`  |
| `desktop/sys/surface-colors/inverse-primary` | `#a8c7fa` | `#a8c7fa`  |
| `desktop/sys/surface-colors/inverse-surface` | `#303030` | `#303030`  |
| `desktop/sys/surface-colors/inverse-surface-primary` | `#062e6f` | `#062e6f`  |
| `desktop/sys/surface-colors/on-surface` | `#1f1f1f` | `#1f1f1f`  |
| `desktop/sys/surface-colors/on-surface-subtle` | `#474747` | `#474747`  |
| `desktop/sys/surface-colors/surface` | `#ffffff` | `#ffffff`  |
| `desktop/sys/surface-colors/surface-1` | `#f8fafd` | `#f8fafd`  |
| `desktop/sys/surface-colors/surface-5` | `#eaf0f9` | `#eaf0f9`  |
| `desktop/sys/surface-colors/surface-variant` | `#e1e3e1` | `#e1e3e1`  |
| `gem/sys/surface-colors/on-surface` | `#1f1f1f` | `#1f1f1f`  |

## 2\. Primary, Tertiary & Container Colors

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/sys/container-colors/neutral-container` | `#f2f2f2` | `#f2f2f2`  |
| `desktop/sys/container-colors/on-tonal-container` | `#041e49` | `#041e49`  |
| `desktop/sys/container-colors/tonal-container` | `#d3e3fd` | `#d3e3fd`  |
| `desktop/sys/primary-colors/on-primary` | `#ffffff` | `#ffffff`  |
| `desktop/sys/primary-colors/primary` | `#0b57d0` | `#0b57d0`  |
| `desktop/sys/tertiary-colors/on-tertiary` | `#ffffff` | `#ffffff`  |
| `desktop/sys/tertiary-colors/on-tertiary-container` | `#072711` | `#072711`  |
| `desktop/sys/tertiary-colors/tertiary` | `#146c2e` | `#146c2e`  |
| `desktop/sys/tertiary-colors/tertiary-container` | `#c4eed0` | `#c4eed0`  |

## 3\. State, Ripple & Interaction Colors

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/sys/state-colors/state-disabled` | `#1f1f1f61` | `#1f1f1f61`  |
| `desktop/sys/state-colors/state-disabled-container` | `#1f1f1f1f` | `#1f1f1f1f`  |
| `desktop/sys/state-colors/state-focus-ring` | `#0b57d0` | `#0b57d0`  |
| `desktop/sys/state-colors/state-header-hover` | `#a8c7fa` | `#a8c7fa`  |
| `desktop/sys/state-colors/state-hover-dim-blend-protection` | `#062e6f2e` | `#062e6f2e`  |
| `desktop/sys/state-colors/state-hover-on-prominent` | `#fdfcfb1a` | `#fdfcfb1a`  |
| `desktop/sys/state-colors/state-hover-on-subtle` | `#1f1f1f0f` | `#1f1f1f0f`  |
| `desktop/sys/state-colors/state-inactive-ring` | `#062e6f8c` | `#062e6f8c`  |
| `desktop/sys/state-colors/state-on-header-hover` | `#062e6f` | `#062e6f`  |
| `desktop/sys/state-colors/state-ripple-neutral-on-prominent` | `#fdfcfb29` | `#fdfcfb29`  |
| `desktop/sys/state-colors/state-ripple-neutral-on-subtle` | `#1f1f1f14` | `#1f1f1f14`  |
| `desktop/sys/state-colors/state-ripple-primary` | `#7cacf852` | `#7cacf852`  |

## 4\. Outline, Divider & Component Colors

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/sys/component-colors/divider` | `#d3e3fd` | `#d3e3fd`  |
| `desktop/sys/component-colors/header` | `#d3e3fd` | `#d3e3fd`  |
| `desktop/sys/component-colors/omnibox-container` | `#edf2fa` | `#edf2fa`  |
| `desktop/sys/component-colors/on-header-divider` | `#a8c7fa` | `#a8c7fa`  |
| `desktop/sys/component-colors/on-header-primary` | `#0b57d0` | `#0b57d0`  |
| `desktop/sys/doNotUse/on-surface-secondary` | `#474747` | `#474747`  |
| `desktop/sys/doNotUse/surface-variant` | `#e1e3e1` | `#e1e3e1`  |
| `desktop/sys/error-colors/error` | `#b3261e` | `#b3261e`  |
| `desktop/sys/error-colors/on-error` | `#ffffff` | `#ffffff`  |
| `desktop/sys/outline-colors/neutral-outline` | `#c7c7c7` | `#c7c7c7`  |
| `desktop/sys/outline-colors/outline` | `#747775` | `#747775`  |
| `desktop/sys/outline-colors/tonal-outline` | `#a8c7fa` | `#a8c7fa`  |

## 5\. Reference & Static Colors

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `CDDS/sys/light/surface` | `#FFFFFF` | `#FFFFFF`  |
| `desktop/ref/neutral-surface/neutral-surface-2` | `#f3f6fc` | `#f3f6fc`  |
| `desktop/ref/neutral-surface/neutral-surface-3` | `#eff3fa` | `#eff3fa`  |
| `desktop/ref/neutral/neutral-0` | `#000000` | `#000000`  |
| `desktop/ref/neutral/neutral-100` | `#ffffff` | `#ffffff`  |
| `desktop/ref/neutral/neutral-60` | `#8f8f8f` | `#8f8f8f`  |
| `desktop/ref/tertiary/tertiary-95` | `#e7f8ed` | `#e7f8ed`  |
| `desktop/sys/static-colors/black` | `#000000` | `#000000`  |
| `desktop/sys/static-colors/white` | `#ffffff` | `#ffffff`  |

## 6\. Typography (Fonts, Sizes, Weights, Line Heights)

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `GIC Icon Scale/Icon M` | `Font(family: "Google Symbols", style: Regular, size: Icon M, weight: 400, lineHeight: Icon M, letterSpacing: 0)` | View Definition`Font(family: "Google Symbols", style: Regular, size: Icon M, weight: 400, lineHeight: Icon M, letterSpacing: 0)` |
| `Icon M` | `16` | `16` |
| `desktop/font/body` | `Google Sans Text` | `Google Sans Text` |
| `desktop/font/headline` | `Google Sans` | `Google Sans` |
| `desktop/font_size/body-five` | `11` | `11` |
| `desktop/font_size/body-four` | `12` | `12` |
| `desktop/font_size/body-three` | `13` | `13` |
| `desktop/font_size/body-two` | `14` | `14` |
| `desktop/font_size/button` | `13` | `13` |
| `desktop/font_size/headline-five` | `14` | `14` |
| `desktop/font_size/headline-four` | `16` | `16` |
| `desktop/font_size/headline-three` | `18` | `18` |
| `desktop/font_size/label` | `9` | `9` |
| `desktop/font_weight/bold` | `Bold` | `Bold` |
| `desktop/font_weight/medium` | `Medium` | `Medium` |
| `desktop/font_weight/regular` | `Regular` | `Regular` |
| `desktop/line_height/body-five` | `16` | `16` |
| `desktop/line_height/body-four` | `18` | `18` |
| `desktop/line_height/body-three` | `20` | `20` |
| `desktop/line_height/body-two` | `20` | `20` |
| `desktop/line_height/button` | `20` | `20` |
| `desktop/line_height/headline-five` | `20` | `20` |
| `desktop/line_height/headline-four` | `24` | `24` |
| `desktop/line_height/headline-three` | `24` | `24` |
| `desktop/line_height/label` | `9` | `9` |
| `gem/icon-size/icon-M` | `16` | `16` |
| `gem/letter-spacing/regular` | `0` | `0` |

## 7\. Typography (Composite Text Styles)

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/body/five (medium)` | `Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-five, weight: 500, lineHeight: desktop/line_height/body-five, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-five, weight: 500, lineHeight: desktop/line_height/body-five, letterSpacing: 0)` |
| `desktop/body/five (regular)` | `Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-five, weight: 400, lineHeight: desktop/line_height/body-five, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-five, weight: 400, lineHeight: desktop/line_height/body-five, letterSpacing: 0)` |
| `desktop/body/four (medium)` | `Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-four, weight: 500, lineHeight: desktop/line_height/body-four, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-four, weight: 500, lineHeight: desktop/line_height/body-four, letterSpacing: 0)` |
| `desktop/body/four (regular)` | `Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-four, weight: 400, lineHeight: desktop/line_height/body-four, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-four, weight: 400, lineHeight: desktop/line_height/body-four, letterSpacing: 0)` |
| `desktop/body/three (bold)` | `Font(family: "desktop/font/body", style: desktop/font_weight/bold, size: desktop/font_size/body-three, weight: 700, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/bold, size: desktop/font_size/body-three, weight: 700, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` |
| `desktop/body/three (medium)` | `Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-three, weight: 500, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-three, weight: 500, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` |
| `desktop/body/three (regular)` | `Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-three, weight: 400, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-three, weight: 400, lineHeight: desktop/line_height/body-three, letterSpacing: 0)` |
| `desktop/body/two (medium)` | `Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-two, weight: 500, lineHeight: desktop/line_height/body-two, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/body-two, weight: 500, lineHeight: desktop/line_height/body-two, letterSpacing: 0)` |
| `desktop/body/two (regular)` | `Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-two, weight: 400, lineHeight: desktop/line_height/body-two, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/regular, size: desktop/font_size/body-two, weight: 400, lineHeight: desktop/line_height/body-two, letterSpacing: 0)` |
| `desktop/headline/five` | `Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-five, weight: 500, lineHeight: desktop/line_height/headline-five, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-five, weight: 500, lineHeight: desktop/line_height/headline-five, letterSpacing: 0)` |
| `desktop/headline/four` | `Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-four, weight: 500, lineHeight: desktop/line_height/headline-four, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-four, weight: 500, lineHeight: desktop/line_height/headline-four, letterSpacing: 0)` |
| `desktop/headline/three` | `Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-three, weight: 500, lineHeight: desktop/line_height/headline-three, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/headline", style: desktop/font_weight/medium, size: desktop/font_size/headline-three, weight: 500, lineHeight: desktop/line_height/headline-three, letterSpacing: 0)` |
| `desktop/special/button` | `Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/button, weight: 500, lineHeight: desktop/line_height/button, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/medium, size: desktop/font_size/button, weight: 500, lineHeight: desktop/line_height/button, letterSpacing: 0)` |
| `desktop/special/label` | `Font(family: "desktop/font/body", style: desktop/font_weight/bold, size: desktop/font_size/label, weight: 700, lineHeight: desktop/line_height/label, letterSpacing: 0)` | View Definition`Font(family: "desktop/font/body", style: desktop/font_weight/bold, size: desktop/font_size/label, weight: 700, lineHeight: desktop/line_height/label, letterSpacing: 0)` |

## 8\. Spacing Tokens

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/spacing/10` | `10` | `10` |
| `desktop/spacing/12` | `12` | `12` |
| `desktop/spacing/13` | `13` | `13` |
| `desktop/spacing/14` | `14` | `14` |
| `desktop/spacing/16` | `16` | `16` |
| `desktop/spacing/18` | `18` | `18` |
| `desktop/spacing/2` | `2` | `2` |
| `desktop/spacing/20` | `20` | `20` |
| `desktop/spacing/24` | `24` | `24` |
| `desktop/spacing/3` | `3` | `3` |
| `desktop/spacing/32` | `32` | `32` |
| `desktop/spacing/4` | `4` | `4` |
| `desktop/spacing/5` | `5` | `5` |
| `desktop/spacing/6` | `6` | `6` |
| `desktop/spacing/7` | `7` | `7` |
| `desktop/spacing/8` | `8` | `8` |
| `desktop/spacing/9` | `9` | `9` |

## 9\. Corner Radius Tokens

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/corner-radius/10` | `10` | `10` |
| `desktop/corner-radius/12` | `12` | `12` |
| `desktop/corner-radius/16` | `16` | `16` |
| `desktop/corner-radius/2` | `2` | `2` |
| `desktop/corner-radius/20` | `20` | `20` |
| `desktop/corner-radius/4` | `4` | `4` |
| `desktop/corner-radius/6` | `6` | `6` |
| `desktop/corner-radius/8` | `8` | `8` |
| `desktop/corner-radius/fully-rounded` | `999` | `999` |
| `gem/corner-radius/fully-rounded` | `100` | `100` |

## 10\. Elevation & Shadows

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/elevation/3` | `Effect(type: DROP_SHADOW, color: #0000004D, offset: (desktop/elevation/3/ambient/X, desktop/elevation/3/ambient/Y), radius: desktop/elevation/3/ambient/Blur, spread: desktop/elevation/3/ambient/Spread); Effect(type: DROP_SHADOW, color: #00000026, offset: (desktop/elevation/3/key/X, desktop/elevation/3/key/Y), radius: desktop/elevation/3/key/Blur, spread: desktop/elevation/3/key/Spread)` | View Definition`Effect(type: DROP_SHADOW, color: #0000004D, offset: (desktop/elevation/3/ambient/X, desktop/elevation/3/ambient/Y), radius: desktop/elevation/3/ambient/Blur, spread: desktop/elevation/3/ambient/Spread); Effect(type: DROP_SHADOW, color: #00000026, offset: (desktop/elevation/3/key/X, desktop/elevation/3/key/Y), radius: desktop/elevation/3/key/Blur, spread: desktop/elevation/3/key/Spread)` |
| `desktop/elevation/3/ambient/Blur` | `3` | `3` |
| `desktop/elevation/3/ambient/Spread` | `0` | `0` |
| `desktop/elevation/3/ambient/X` | `0` | `0` |
| `desktop/elevation/3/ambient/Y` | `1` | `1` |
| `desktop/elevation/3/key/Blur` | `8` | `8` |
| `desktop/elevation/3/key/Spread` | `3` | `3` |
| `desktop/elevation/3/key/X` | `0` | `0` |
| `desktop/elevation/3/key/Y` | `4` | `4` |
| `desktop/elevation/4` | `Effect(type: DROP_SHADOW, color: #0000004D, offset: (desktop/elevation/4/ambient/X, desktop/elevation/4/ambient/Y), radius: desktop/elevation/4/ambient/Blur, spread: desktop/elevation/4/ambient/Spread); Effect(type: DROP_SHADOW, color: #00000026, offset: (desktop/elevation/4/key/X, desktop/elevation/4/key/Y), radius: desktop/elevation/4/key/Blur, spread: desktop/elevation/4/key/Spread)` | View Definition`Effect(type: DROP_SHADOW, color: #0000004D, offset: (desktop/elevation/4/ambient/X, desktop/elevation/4/ambient/Y), radius: desktop/elevation/4/ambient/Blur, spread: desktop/elevation/4/ambient/Spread); Effect(type: DROP_SHADOW, color: #00000026, offset: (desktop/elevation/4/key/X, desktop/elevation/4/key/Y), radius: desktop/elevation/4/key/Blur, spread: desktop/elevation/4/key/Spread)` |
| `desktop/elevation/4/ambient/Blur` | `3` | `3` |
| `desktop/elevation/4/ambient/Spread` | `0` | `0` |
| `desktop/elevation/4/ambient/X` | `0` | `0` |
| `desktop/elevation/4/ambient/Y` | `2` | `2` |
| `desktop/elevation/4/key/Blur` | `10` | `10` |
| `desktop/elevation/4/key/Spread` | `4` | `4` |
| `desktop/elevation/4/key/X` | `0` | `0` |
| `desktop/elevation/4/key/Y` | `6` | `6` |

## 11\. Miscellaneous & Gem Tokens

| Variable Name | Value / Definition | Preview |
| :---- | :---- | :---- |
| `desktop/admin/g/blue` | `#4285f4` | `#4285f4`  |
| `desktop/admin/g/green` | `#34a853` | `#34a853`  |
| `desktop/admin/g/red` | `#ea4335` | `#ea4335`  |
| `desktop/admin/g/yellow` | `#fbbc05` | `#fbbc05`  |
| `desktop/admin/tab-groups/bookmarks-bar/blue` | `#e8f0fe` | `#e8f0fe`  |
| `desktop/admin/tab-groups/tab-strip/blue` | `#1a73e8` | `#1a73e8`  |
| `desktop/admin/tab-groups/tab-strip/green` | `#188038` | `#188038`  |
| `gem/gradients/angular` | \`\` | \`\` |
| `gem/gradients/tab-strip` | \`\` | \`\` |
