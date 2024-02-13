# Keyboard Accessory and Accessory Fallback Sheets
This folder contains the keyboard accessory and its sheets. These surfaces
allow users to manually fill forms with their stored data if automatic systems
like [TouchToFill](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/touch_to_fill/) cannot assist.

The data that users can choose to fill are for example:
 * passwords and passkeys
 * address profiles
 * credit cards

For each datatype, the accessory serves as entry point for assistive
functionality that Desktop surfaces show in dropdowns, for example
 * password generation
 * scanning credit cards

## Structure

This folder should be consistently split into three parts:

 * `/` containing public, x-platform C++ code other components depend on
 * `android/` containing public android C++ code to depend on
 * `android/java/` containing public android java code to depend on
 * `internal/` containing x-platform C++ implementations
 * `internal/android` containing android C++ implementations
 * `internal/android/java` containing android java implementations
 * `test_utils/{,android/{,java/}}` containing test support tools (NO TESTS!)

## Note for Contributors

No Android code in this directory may depend on `chrome_java`. Some classes
have yet to be moved into this folder (most notably the core java components of
the [keyboard accessory](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/features/keyboard_accessory).
Existing dependencies on `chrome_java` have to be removed. Even for "temporary
fixes", they are not acceptable.

It is fine to depend on everything that isn't in `internal/` outside
this component.
