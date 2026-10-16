# List Search Suggestions Spec (Clank)

This document maps the specifications of the **List Search Suggestions** (Clank)
component across Figma and code.

______________________________________________________________________

## Overview

**List-search** is the omnibox suggestion row: a leading decoration (search
icon, favicon, entity image, or answer icon circle), one or two lines of text,
and trailing actions (the "refine"/append arrow, or share / copy / edit
buttons). In Clank it's implemented by the omnibox suggestion view hierarchy:
`BaseSuggestionView` (a `SuggestionLayout`) wraps a content view
(`omnibox_basic_suggestion.xml` / `omnibox_answer_suggestion.xml`) and is bound
through MVC `PropertyModel`s by the Basic, Entity, and Answer suggestion
processors.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                            | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| :----------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `List-search`                                                                                                                                              | `BaseSuggestionView`<br>`SuggestionLayout`                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Source Files**   | [Figma Link: `List-search` (`32196:9626`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=32196-9626&m=dev) | [chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/base/BaseSuggestionView.java](//src/chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/base/BaseSuggestionView.java)<br>[chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/base/SuggestionLayout.java](//src/chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/base/SuggestionLayout.java) |

> Source page:
> [`↳ Lists` (`6371:41566`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=6371-41566),
> showcase frame `Components/List/Search-suggestions` (`40052:4620`), grouped
> into "SEARCH QUERY", "Answers in Suggest", "SRO", and "RICH ENTITY".
>
> MVC bindings: `BaseSuggestionViewBinder`, `SuggestionViewViewBinder` (basic),
> `EntitySuggestionViewBinder`, `AnswerSuggestionViewBinder`. Decoration state
> comes from `OmniboxDrawableState` (`forSmallIcon`, `forLargeIcon`,
> `forFavIcon`, `forImage`, `forColor`). Sizes and colors come from
> `OmniboxResourceProvider` and
> [omnibox dimens.xml](//src/chrome/browser/ui/android/omnibox/java/res/values/dimens.xml).

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `List-search`: `Variant` (Entity | Basic) × `Lines` (1-line | 2-line). Boolean
  properties: `LHS Icon Circle` (default true), `LHS Icon` (default false),
  `LHS Favicon` (default false), `LHS Image` (default false), `RHS Append Icon`
  (default true), `RHS icons` (default false), `Bottom Divider` (default false).

| Feature / Variant                           | Figma Component                                      | Clank (Android)                                                                                                                           |
| :------------------------------------------ | :--------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------- |
| **Search query (basic)**                    | `Variant=Basic`, `LHS Icon=true` (`search`)          | `BasicSuggestionProcessor` + `omnibox_basic_suggestion.xml`; `OmniboxDrawableState.forSmallIcon` (24dp)                                   |
| **URL / navigation with favicon**           | `LHS Favicon=true`                                   | `OmniboxDrawableState.forFavIcon` (24dp, rounded `omnibox_small_icon_rounding_radius` 4dp)                                                |
| **Answer in Suggest**                       | `LHS Icon Circle=true` (`sunny` on `primary` circle) | `AnswerSuggestionProcessor` → `OmniboxDrawableState.forLargeIcon` (36dp); `omnibox_answer_suggestion.xml` (line 2 up to 3 lines)          |
| **Rich entity**                             | `Variant=Entity`, `LHS Image=true`                   | `EntitySuggestionProcessor` → `OmniboxDrawableState.forImage` / `forColor` (dominant-color placeholder), 36dp, rounded 8dp                |
| **1-line**                                  | `Lines=1-line`                                       | `line_2` `visibility="gone"`                                                                                                              |
| **2-line**                                  | `Lines=2-line`                                       | `line_2` visible (`TextAppearance.TextMedium.Secondary`)                                                                                  |
| **Append / refine arrow**                   | `RHS Append Icon=true` (`arrow_insert`)              | `BaseSuggestionViewProcessor` refine action → `R.drawable.btn_suggestion_refine_up`, `ActionButtonView` 48dp wide                         |
| **Share / copy / edit actions**             | `RHS icons=true` (gap `24px`)                        | Multiple `ActionButtonView`s (e.g. edit-URL suggestion) — exact set per processor not verified                                            |
| **Bottom divider**                          | `Bottom Divider=true`                                | `SuggestionHorizontalDivider` / `GroupSeparatorDecoration` (group separators, not per-row)                                                |
| **Pressed / Focused / Selected (keyboard)** | *(Not modeled)*                                      | `SuggestionLayout` rounded background + hover overlay (`omnibox_suggestion_bg_hover_overlay_fraction`); `RecyclerViewSelectionController` |
| **Incognito**                               | *(Not modeled)*                                      | `search_suggestion_bg_color_incognito` → `gm3_baseline_surface_container_low_dark`                                                        |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute                  | Figma Design Token                                                                      | Clank (Android)                                                                                                                                                                                                                                                                                                                                                                 |
| :-------------------------------- | :-------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Row background**                | `--clank/macro/list_items/search_suggestion_bg_color` (white)                           | `OmniboxResourceProvider.getStandardSuggestionBackgroundColor` → `R.color.search_suggestion_bg_color` ([color/](//src/components/omnibox/browser/android/java/res/color/search_suggestion_bg_color.xml) `?attr/colorSurface`; [color-night/](//src/components/omnibox/browser/android/java/res/color-night/search_suggestion_bg_color.xml) `?attr/colorSurfaceContainerLow`) ✅ |
| **Row corner radius**             | `2px` (hardcoded)                                                                       | `@dimen/omnibox_suggestion_bg_round_corner_radius` **`16dp`** (`SuggestionLayout` outline)                                                                                                                                                                                                                                                                                      |
| **Row height (1-line)**           | `56px`                                                                                  | `@dimen/omnibox_suggestion_content_height` `56dp` ✅                                                                                                                                                                                                                                                                                                                            |
| **Line 1 typography**             | Basic/Entity line 1: **Roboto Regular `16px` / `24px`** (hard-coded, not a clank token) | `TextAppearance.TextLarge.Primary` (`16sp`, app font) with `@dimen/text_size_large_leading` `24sp`                                                                                                                                                                                                                                                                              |
| **Line 1 color**                  | `--clank/sys/color/onSurface` (`#1f1f1f`)                                               | `default_text_color_list` → `?attr/colorOnSurface` ✅                                                                                                                                                                                                                                                                                                                           |
| **Line 2 typography / color**     | `textMedium` 14/20; `onSurfaceVariant` (`#444746`)                                      | `TextAppearance.TextMedium.Secondary` → `?attr/colorOnSurfaceVariant` ✅                                                                                                                                                                                                                                                                                                        |
| **Basic: start padding + icon**   | `pl 16px` + 24px icon + `16px` gap (text starts at 56px)                                | `@dimen/omnibox_suggestion_icon_area_size` `56dp`, small icon `omnibox_suggestion_24dp_icon_size` `24dp` ✅                                                                                                                                                                                                                                                                     |
| **Entity: start padding + image** | `pl 10px` + 36px image + `10px` gap (text starts at 56px)                               | 56dp icon area; large icon `omnibox_suggestion_36dp_icon_size` `36dp` ✅                                                                                                                                                                                                                                                                                                        |
| **Entity image radius**           | `--clank/shape/corner_radius/8` (`8px`)                                                 | `omnibox_large_icon_rounding_radius` → `default_rounded_corner_radius` `8dp` ✅                                                                                                                                                                                                                                                                                                 |
| **Answer icon circle**            | `--clank/sys/color/primary` (`#0b57d0`), `100px` radius, 36px; glyph `onPrimary`        | Pre-composed answer drawable via `forLargeIcon` (36dp, no rounding applied) — drawable colors not verified                                                                                                                                                                                                                                                                      |
| **Favicon**                       | 24px, no rounding                                                                       | 24dp, `omnibox_small_icon_rounding_radius` `4dp`                                                                                                                                                                                                                                                                                                                                |
| **Leading icon tint**             | `onSurface` (`#1f1f1f`)                                                                 | `BaseSuggestionViewBinder` decoration → `ChromeColors.getSecondaryIconTintRes` → `default_icon_color_secondary_tint_list` → `?attr/colorOnSurfaceVariant` (only when `OmniboxDrawableState.allowTint`) — **mismatch**                                                                                                                                                           |
| **Trailing icon color / size**    | `onSurface`, 24px; `pr 12px`                                                            | Action buttons → `ChromeColors.getPrimaryIconTintRes` → `default_icon_color_tint_list` → `?attr/colorOnSurface` ✅; `ActionButtonView` `omnibox_suggestion_action_button_width` `48dp`; `omnibox_suggestion_end_padding_no_action_button` `12dp`                                                                                                                                |
| **Vertical padding**              | Basic: (height-driven); Entity 1-line `10px`, 2-line `6px`                              | Content height driven (`omnibox_suggestion_minimum_content_height` `48dp`, `omnibox_suggestion_content_padding` `8dp`)                                                                                                                                                                                                                                                          |
| **Divider color**                 | `--clank/macro/divider/divider_color` (`#dde3ea`)                                       | `SuggestionHorizontalDivider` color — not verified                                                                                                                                                                                                                                                                                                                              |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Row shape

- **Figma**: A nearly square row (`2px` radius) on a white background, with rows
  placed edge to edge.
- **Clank**: `SuggestionLayout` clips each row to a `16dp` rounded rect
  (`omnibox_suggestion_bg_round_corner_radius`). Rows are spaced by
  `SpacingRecyclerViewItemDecoration` /
  `DynamicSpacingRecyclerViewItemDecoration` and grouped into cards. Figma
  doesn't reflect this current grouped-card treatment.

### 2. Typography source

- Figma line 1 uses hardcoded **Roboto** 16/24 rather than the `clank/font`
  tokens. Code uses `TextAppearance.TextLarge.Primary` (app font, Google Sans
  Text where available), so the font family differs.

### 3. Background token

- The Figma macro `search_suggestion_bg_color` (white) maps directly to
  `R.color.search_suggestion_bg_color`, defined in
  `components/omnibox/browser/android/java/res/color{,-night}/`. Light mode
  resolves to `?attr/colorSurface` (matches the Figma white). Night mode
  resolves to `?attr/colorSurfaceContainerLow`, and Figma's dark showcase value
  was not checked. Incognito uses `search_suggestion_bg_color_incognito`.

### 4. Answer icon composition

- Figma draws the answer icon as a `primary` circle with an `onPrimary` glyph.
  Code loads pre-composed answer drawables through
  `OmniboxDrawableState.forLargeIcon` without runtime rounding. The colors live
  in the drawable assets, which weren't checked.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing states

There are no pressed, focused, hovered, or keyboard-selected variants. Code
applies an `omnibox_suggestion_bg_hover_overlay_fraction` overlay and a
selection highlight for keyboard navigation (`RecyclerViewSelectionController`).

### 2. Leading icon tint

Figma tints the search/history glyph `onSurface`. Code tints decoration icons
with the secondary icon tint (`default_icon_color_secondary_tint_list` →
`colorOnSurfaceVariant`), which is lighter. Trailing action icons do match
(`colorOnSurface`).

### 3. Dividers

Figma has a per-row `Bottom Divider`. Code uses group-level separators
(`GroupSeparatorDecoration`, `SuggestionHorizontalDivider`), not one per row.

### 4. Desktop-form-factor sizing

Code shrinks icons on desktop-class Android
(`omnibox_desktop_small/large_decoration_icon_size` 20/32dp,
`omnibox_suggestion_content_height_desktop` 48dp). Figma only models the phone
sizes.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Pick the variant to match the suggestion type: `Basic` for queries and URLs,
  `Entity` for rich entities that have an image, and the icon circle for
  answers.
- Keep line 1 to a single line with ellipsis. Line 2 holds the URL, description,
  or answer detail.
- Show the append (refine) arrow only on query-like suggestions where filling
  the omnibox makes sense.

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Each action button needs its own content description (for example,
  `accessibility_omnibox_btn_refine`).
- Suggestions must support arrow-key and Tab navigation through the selection
  controller, and the selected row needs a visible highlight.
- The row and each action button need at least a `48dp` touch target
  (`omnibox_suggestion_action_button_width`).

### 3. Icon Usage Guidelines

- Use 24px glyphs for basic suggestions and 36px images or answer icons for
  entity/answer rows.
- Show favicons as-is. Don't tint them.
