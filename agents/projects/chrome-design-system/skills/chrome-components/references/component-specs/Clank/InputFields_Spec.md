# Input Fields Spec (Clank)

This document maps the specifications of the **Input Fields** (Clank) component
across Figma and code.

______________________________________________________________________

## Overview

**Input Fields** is Clank's legacy "underline" text input: a small label above a
single line of input text, a 2px divider underneath, and an optional "required"
error message. Figma models three states (`Default`, `Default Filled/Focused`,
`Error Filled`) plus toggles for the label, input text, cursor, a dropdown arrow
and highlighted text. In code this is the app-wide default `TextInputLayout`
style (`Widget.BrowserUI.TextInputLayout`, based on the pre-M3
`Widget.Design.TextInputLayout`) wrapping a `TextInputEditText`.

______________________________________________________________________

## 1. Component Metadata & Source Files

| Feature            | Figma Component                                                                                                                                               | Clank (Android)                                                                                                                                                                                                                                                                              |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Component Name** | `Input Fields`                                                                                                                                                | `TextInputLayout`<br>`TextInputEditText`                                                                                                                                                                                                                                                     |
| **Source Files**   | [Figma Link: `Input Fields` (`46003:16658`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=46003-16658&m=dev) | [components/browser_ui/styles/android/java/res/values/styles.xml](//src/components/browser_ui/styles/android/java/res/values/styles.xml)<br>[components/browser_ui/theme/android/templates/res/values/themes.xml](//src/components/browser_ui/theme/android/templates/res/values/themes.xml) |

> Source page:
> [`Input & Text Fields` (`46765:112`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=46765-112)
> — documentation frame
> [`Input Fields` (`46765:113`)](https://www.figma.com/design/PQmS9rgYHrRTXaLTy0u0O2/Chrome-Android--Clank--Design-Kit?node-id=46765-113)
> with light and dark showcases. The same page also has a separate
> [`Text field`](./TextField_Spec_Clank.md) component (M3 outlined/filled),
> which has its own spec.
>
> `TextInputLayout` / `TextInputEditText` come from Material Components
> (`com.google.android.material:material` 1.14.0), not Chromium. The app theme
> sets `textInputStyle` → `@style/Widget.BrowserUI.TextInputLayout` (parent
> `Widget.Design.TextInputLayout`), with a `TODO(crbug.com/40764906)` to update
> it once the app design is updated. Its `materialThemeOverlay` sets
> `editTextStyle` → `Widget.BrowserUI.TextInputEditText` and
> `autoCompleteTextViewStyle` → `Widget.BrowserUI.AutoCompleteTextView`.
> Feature-specific subclasses (`BookmarkTextInputLayout`,
> `TabGroupVisualDataTextInputLayout`, `EditTextNoAutofillView`,
> `TextInputEditTextNoAutofill`) and the standalone `AlertDialogEditText` exist
> but aren't general-purpose widgets.

______________________________________________________________________

## 2. Variants & Properties

Figma variant axes:

- `Input Fields`: `Type` (Default | Default Filled/Focused | Error Filled) ×
  booleans `Label Title`, `Input Text`, `Cursor`, `Label Required`, `Dropdown`
  (default off), `Highlight text` (default off).

| Feature / Variant            | Figma Component                     | Clank (Android)                                                                                                              |
| :--------------------------- | :---------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- |
| **Default (unfocused)**      | `Type=Default`                      | `TextInputLayout` (default `textInputStyle`) not focused                                                                     |
| **Focused**                  | `Type=Default Filled/Focused`       | `TextInputLayout` focused (`state_focused`)                                                                                  |
| **Error**                    | `Type=Error Filled`                 | `TextInputLayout#setError(...)` (`app:errorEnabled="true"` reserves space)                                                   |
| **Label**                    | `Label Title=true`                  | `android:hint` / `TextInputLayout#setHint` (floating hint, collapsed above input)                                            |
| **Empty field (label only)** | `Input Text=false`                  | Empty `EditText`; hint stays collapsed only when focused, otherwise expands into the input line (*Figma shows it collapsed*) |
| **No label**                 | `Label Title=false`                 | No hint (`app:hintEnabled="false"`) or a bare `EditText`                                                                     |
| **Cursor**                   | `Cursor=true`                       | Native text cursor (`colorControlActivated`)                                                                                 |
| **Required/error message**   | `Label Required=true` (Error only)  | Error text from `setError(...)`                                                                                              |
| **Dropdown arrow**           | `Dropdown=true` (`arrow_drop_down`) | `AutoCompleteTextView` in the layout and/or `app:endIconMode="dropdown_menu"`                                                |
| **Selected text highlight**  | `Highlight text=true`               | Native text selection (`android:textColorHighlight`)                                                                         |
| **Disabled**                 | *(Not modeled)*                     | `android:enabled="false"` (hint/text at 38% via color state lists)                                                           |
| **Helper text / counter**    | *(Not modeled)*                     | `app:helperText`, `app:counterEnabled`                                                                                       |
| **Dark theme**               | "Dark" showcase (token mode switch) | Night-mode resolution of the same theme attributes                                                                           |

______________________________________________________________________

## 3. Design Token Comparison (Side-by-Side)

| Design Attribute             | Figma Design Token                                                                   | Clank (Android)                                                                                                                                                               |
| :--------------------------- | :----------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Label (default)**          | `--clank/sys/color/onSurfaceVariant, osv`<br>(light `#444746` / dark `#c4c7c5`)      | `android:textColorHint` → `@color/default_text_color_hint_list` → `?attr/colorOnSurfaceVariant` (disabled: `colorOnSurface` @ 38%)                                            |
| **Label (focused)**          | `--clank/sys/color/primary`<br>(light `#0b57d0` / dark `#a8c7fa`)                    | `hintTextAppearance` → `@style/TextAppearance.TextSmall.Accent1` → `@color/default_text_color_accent1_tint_list` → `@macro/default_text_color_accent1` → `?attr/colorPrimary` |
| **Label (error)**            | `--clank/macro/error/default_red_color`<br>(light `#b3261e` / dark `#f2b8b5`)        | Collapsed hint uses error text color while an error is shown → `@color/default_text_color_error` (`baseline_error_40` `#B3261E` / night `baseline_error_80` `#F2B8B5`)        |
| **Input text**               | `--clank/sys/color/onSurface, os`<br>(light `#1f1f1f` / dark `#e3e3e3`)              | `Widget.BrowserUI.TextInputEditText` → `@style/TextAppearance.TextLarge.Primary` → `@color/default_text_color_list` → `?attr/colorOnSurface`                                  |
| **Divider (default)**        | `--clank/sys/color/onSurface, os`, `2px`                                             | `EditText` background underline tinted with `colorControlNormal` → `@macro/default_control_color_normal` → `?attr/colorOnSurfaceVariant` (`#444746`)                          |
| **Divider (focused)**        | `--clank/sys/color/primary`, `2px`                                                   | `colorControlActivated` → `@macro/default_control_color_active` → `?attr/colorPrimary`                                                                                        |
| **Divider (error)**          | `--clank/macro/error/default_red_color`, `2px`                                       | `TextInputLayout` tints the `EditText` background with the error text color (`default_text_color_error`)                                                                      |
| **Cursor**                   | `primary` (focused) / `default_red_color` (error), `2px` × `24px`                    | Platform cursor drawable tinted with `colorControlActivated` (`colorPrimary`). Error-colored cursor *not verified* for the `Widget.Design` style                              |
| **Text highlight**           | `--clank/sys/state_layer/primary_20%` (`rgba(11,87,208,0.2)`)                        | Platform `textColorHighlight` *(not verified)*                                                                                                                                |
| **Dropdown icon**            | `arrow_drop_down`, 24px box, `onSurface`                                             | `endIconMode="dropdown_menu"` icon tint *(not verified)*                                                                                                                      |
| **Label typography**         | `textSmall`: Google Sans Text Regular 12/16, tracking 0                              | `TextAppearance.TextSmall.Accent1` → `@dimen/text_size_small` (`12sp`) / `@dimen/text_size_small_leading` (`16sp`)                                                            |
| **Input typography**         | `textLarge`: Google Sans Text Regular 16/24, tracking 0                              | `TextAppearance.TextLarge.Primary` → `@dimen/text_size_large` (`16sp`) / `text_size_large_leading`                                                                            |
| **Error message typography** | `textMedium`: Google Sans Text Regular **14/20**                                     | `errorTextAppearance` → `@style/TextAppearance.ErrorCaption` → `@dimen/text_size_small` (**`12sp`**) / `text_size_small_leading` (`16sp`)                                     |
| **Font family**              | `--clank/font/font_family/alternative` (Google Sans Text)                            | Not set by these text appearances; inherits theme/system font *(not verified)*                                                                                                |
| **Padding**                  | `16px` horizontal; `8px` top; `14px` bottom (Default/Focused) or `8px` (Error)       | Determined by each feature layout (no shared padding in `Widget.BrowserUI.TextInputLayout`) *(not verified per layout)*                                                       |
| **Spacing**                  | `8px` label→input (Default); `6px` between rows (Focused/Error); `4px` input→divider | `Widget.Design.TextInputLayout` internal hint/label offsets *(not verified)*                                                                                                  |
| **Width**                    | `412px` (full-width row)                                                             | `match_parent` in feature layouts                                                                                                                                             |

______________________________________________________________________

## 4. Architectural & Implementation Gaps

### 1. Legacy design style vs. M3

- The app-wide default `textInputStyle` intentionally keeps the pre-M3
  `Widget.Design.TextInputLayout` (`TODO(crbug.com/40764906)`). Settings screens
  (autofill editors via `ThemeOverlay.Chromium.Settings.InputFields`) and
  bookmark editors use the M3 outlined box instead, which is the separate
  [`Text field`](./TextField_Spec_Clank.md) component. The two styles are mixed
  across the app.

### 2. Unfocused divider color

- **Figma**: Default divider uses `onSurface` (`#1f1f1f`).
- **Clank**: The `EditText` underline is tinted with `colorControlNormal` →
  `colorOnSurfaceVariant` (`#444746`).

### 3. Error message size

- **Figma**: `[Label] required` uses `textMedium` (14/20).
- **Clank**: `TextAppearance.ErrorCaption` uses `text_size_small` (12sp / 16sp
  leading).

### 4. Divider thickness

- Figma uses a `2px` divider in every state. The AppCompat `EditText` underline
  drawable is thinner when unfocused than when focused. Exact thicknesses were
  *not verified* against the drawable.

______________________________________________________________________

## 5. Styling, Variants, Features and States Mismatches

### 1. Missing states in Figma

There's no Disabled, Hovered or Error-unfocused variant. There's also no helper
text or character counter, though `TextInputLayout` supports both.

### 2. Label position when empty and unfocused

`Type=Default` with `Input Text=false` shows the label collapsed above an empty
divider. In code, the floating hint expands into the input line when the field
is empty and unfocused, unless the hint is disabled or a placeholder is used.

### 3. "Default Filled/Focused" merges two states

Figma's `Default Filled/Focused` variant is the focused state. A filled but
unfocused field matches `Type=Default`. The variant name suggests otherwise.

### 4. Dropdown

The `Dropdown` boolean only adds an `arrow_drop_down` glyph. In code, a dropdown
needs an `AutoCompleteTextView` / exposed dropdown menu (for example
`Widget.Material3.TextInputLayout.OutlinedBox.ExposedDropdownMenu` in
`autofill_billing_address_dropdown.xml`), which is M3 outlined rather than this
underline style.

### 5. Not verified

- Paddings and spacing inside individual feature layouts.
- Font family resolution for `TextAppearance.TextSmall` / `TextLarge`.
- Error-colored cursor and highlight color at runtime.

______________________________________________________________________

## 6. Usage & UX Guidance

### 1. General Principles & Best Practices

- Use for single-line text entry in dialogs (for example HTTP auth, password
  edit, add-username, bookmark folder name) that use the default app theme.
- For new Settings-style forms, check whether the M3
  [`Text field`](./TextField_Spec_Clank.md) (outlined) is expected instead;
  don't mix both styles on one screen.
- **Labeling**: Use a short sentence-case label. Error messages should say
  what's wrong (for example "[Label] required").

### 2. Platform Consistency, Keyboard Controls & Accessibility (a11y)

- Put the label in `TextInputLayout`'s hint (not a separate `TextView`) so
  TalkBack announces it with the field.
- Use `setError(...)` for validation so TalkBack announces the error. Set
  `app:errorEnabled="true"` when errors are expected so the layout doesn't jump.
- The field row should be at least `48dp` tall (`@dimen/min_touch_target_size`).
- Set the right `android:inputType` / `android:autofillHints` (or use the
  `*NoAutofill` subclasses where autofill is unwanted).

### 3. Icon Usage Guidelines

- Only use the trailing `arrow_drop_down` for real dropdowns. Use
  `TextInputLayout` end-icon modes (`password_toggle`, `clear_text`,
  `dropdown_menu`, `custom`) instead of custom overlays.
