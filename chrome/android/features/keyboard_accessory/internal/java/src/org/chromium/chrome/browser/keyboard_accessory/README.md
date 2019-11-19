# Manual filling component

This folder contains all ui components that are necessary to display the
keyboard accessory bar and the accessory bottom sheet. They are mainly used
for autofill- and password-related tasks.

## Use cases

1. Act as an autofill popup substitute by displaying all autofill suggestions as
   as chips above the keyboard.
2. Provide an entry point to password generation (automatic and manual).
3. Provide fallback sheets to fill single form fields with stored password,
   address or payments data.

## Structure

The ManualFillingCoordinator in this package uses the `bar_component.*` to
display a bar above an open keyboard. This bar shows suggestions and holds a
number of tabs in a `tab_layout_component.*` which allows to open an accessory
sheet with fallback data and options.
The sheet is located in the `sheet_component.*` and shows one of the tabs as
defined in `sheet_tabs.*`.
The responsibility of the ManualFillingCoordinator is to integrate the active
sub components with the rest of chromium (e.g. Infobars, popups, etc.) and
ensure that they are perceived as extension or replacement of the keyboard.

The `data.*` package provides helper classes that define the data format used by
all components. They support data exchange by providing generic `Provider`s and
simple implementations thereof.

## Development

Ideally, components only communicate by interacting with the coordinator of one
another. Their inner structure (model, view, view binder and properties) should
remain package-private. For some classes, this is still an ongoing rework.

## Versioning

There are two versions of the accessory that share a most components.

### V1 - Keyboard Accessory for Passwords

Version V1 of the manual filling component supports only filling password fields
and introduces password generation. Enable it with these flags:
- #passwords-keyboard-accessory and
- #automatic-password-generation

### V2 Keyboard Accessory with Autofill Suggestions

Version V2 of the manual filling component supports filling every form field
where autofill is available with autofill suggestions or fallback data.
It applies Chrome's modern design. To enable it, set these flags:
- #passwords-keyboard-accessory and
- #automatic-password-generation and
- #autofill-keyboard-accessory-view
