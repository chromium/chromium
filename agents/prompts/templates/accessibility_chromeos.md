# ChromeOS accessibility instructions

Follow these guidelines when developing on accessibility for ChromeOS. The
person you are assisting works primarily on the accessibility feature
implementations (ChromeVox, Face control, Dictation, etc.) and the accessibility
platform/frameworks that support these features.

## Background
Before starting any task, you ** MUST READ ** the following files to better
understand the history and existing principles for accessibility on Chrome and
ChromeOS. Read the following files to familiarize yourself with Chrome browser
accessibility, which is the foundation for accessibility on ChromeOS:
  *  `//docs/accessibility/overview.md`
  *  `//docs/accessibility/browser/how_a11y_works.md`
  *  `//docs/accessibility/browser/how_a11y_works_2.md`
  *  `//docs/accessibility/browser/how_a11y_works_3.md`

Also read and understand `//ui/accessibility/ax_enums.mojom`, which defines the
accessibility API on Chrome.

Read the following files to familiarize yourself with ChromeOS accessibility:
  *  `//docs/accessibility/os/how_a11y_works.md`
  *  `//docs/accessibility/os/chromevox.md`
  *  `//docs/accessibility/os/dictation.md`
  *  `//docs/accessibility/os/facegaze.md`
  *  `//docs/accessibility/os/select_to_speak.md`
  *  `//docs/accessibility/os/switch_access.md`
  *  `//docs/accessibility/os/autoclick.md`

###  Accessibility feature implementation
Accessibility features are primarily implemented as Chrome extensions in
TypeScript and JavaScript, which can be found in the directory
`//chrome/browser/resources/chromeos/accessibility`. You may find subfolders
named `mv2/` and `mv3/`; this is because the team is migrating the extension
implementation from manifest v2 to manifest v3. The intention is to eventually
remove the `mv2/` code once the migrations have been completed. Please reference
`mv3/` code for the most accurate responses.

### Supporting code in the browser
Accessibility features on ChromeOS have special privileges since they are
developed by Google, and thus can communicate with the browser process via
private extension APIs (more information below). See
`//chrome/browser/ash/accessibility`,
`//ash/accessibility/accessibility_controller.cc`, and
`//ash/system/accessibility` for additional accessibility code in the browser.

###  Extension APIs
Extension APIs can be used by accessibility features to communicate with the
browser process. These are usually defined in `.idl` or `.json` files in
`//extensions/common/api/` and `//chrome/common/extensions/api/`.

The most important extension API for accessibility is the automation API, which
is the ChromeOS-specific implementation of the Chrome accessibility API. See
`//extensions/common/api/automation.idl` for the interface definition and
`//ui/accessibility/platform/automation/` for the implementation.

Another important extension API is the accessibility private API, which is
defined at `//chrome/common/extensions/api/accessibility_private.json` and
implemented in
`//chrome/browser/accessibility/accessibility_extension_api_ash.cc`.
