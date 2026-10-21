---
name: chrome/no-browser-window-host-calls-in-app-shims
rubric: correctness
severity: should_fix
---

**Rule.** In macOS Remote Cocoa architecture, do not call methods, access
instances, or set static state on `NativeWidgetMacNSWindowHost` from app shim
processes (`chrome/app_shim/` or `components/remote_cocoa/app_shim/`). App shims
run in separate out-of-process helpers where browser-process host objects do not
exist; coordinate host state and window restoration centrally in the browser
process (e.g., in `chrome_browser_application_mac.mm` or
`chrome_browser_main_mac.mm`) or over Mojo via `NativeWidgetNSWindowBridge`.

**Why.** App shims execute in separate helper processes from Chrome's browser
process. Setting static host state or calling host methods in an app shim
process has no effect on the browser process and violates Remote Cocoa process
boundaries.

## Examples

Bad:

```cpp
// In chrome/app_shim/app_shim_application.mm:
if ([event paramDescriptorForKeyword:keyAERestoreAppState]) {
  views::NativeWidgetMacNSWindowHost::
      SetMoveWindowsToOriginalSpacesUponRestoration(true);
}
```

Good:

```cpp
// In chrome/browser/chrome_browser_application_mac.mm:
// Window restoration and host state are managed centrally in the browser process.
if ([event paramDescriptorForKeyword:keyAERestoreAppState]) {
  views::NativeWidgetMacNSWindowHost::
      SetMoveWindowsToOriginalSpacesUponRestoration(true);
}
```
