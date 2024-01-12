# Virtual Keyboard

## Enabled or Disabled

The virtual keyboard can be enabled or disabled. When it is enabled, it shows and hides depending where the focus is.

Whether the virtual keyboard is enabled depends on a [sequence of criteria](https://source.chromium.org/search?q=symbol:KeyboardUIController::IsKeyboardEnableRequested&sq=&ss=chromium%2Fchromium%2Fsrc) that are listed in order below:

1. **Accessibility Setting**: When the user enables the virtual keyboard via the accessibility settings, then the virtual keyboard is enabled. The setting can also be forcibly overridden by the [VirtualKeyboardEnabled policy](https://crsrc.org/c/components/policy/resources/templates/policy_definitions/Accessibility/VirtualKeyboardEnabled.yaml).
1. **Shelf (Temporary)**: The virtual keyboard may be temporarily enabled via entry points in the shelf input method menu. It is disabled as soon as the virtual keyboard hides.
1. **Android  IME**: Users can install custom Android input methods that run in ARC++. When using an Android input method, the ChromeOS virtual keyboard is disabled.
1. **Enterprise Policy**: Explicitly setting the [TouchVirtualKeyboardEnabled policy](https://crsrc.org/c/components/policy/resources/templates/policy_definitions/Miscellaneous/TouchVirtualKeyboardEnabled.yaml) to true or false will enable or disable the virtual keyboard.
1. **Command Line Switches**: The `--enable-virtual-keyboard` and `--disable-virtual-keyboard` command line switches (and their corresponding flags in `about://flags`) enables and disables the virtual keyboard.
1. **Extension API**: Certain first-party extensions may enable or disable the virtual keyboard via the `chrome.virtualKeyboardPrivate.setKeyboardState` API.
1. **Touch**: Finally, if none of the above applies, then the virtual keyboard is only enabled if *all* the following are true:
    * There is at least one touchscreen.
    * The internal keyboard (if it exists) is ignored. An internal keyboard can be ignored by, for example, detaching it (detachable) or folding a device into tablet mode (convertible).
    * Any external keyboards (if they exist) are ignored. External keyboards can be ignored by a user toggle in the shelf input method menu.
