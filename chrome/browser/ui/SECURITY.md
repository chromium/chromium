# Browser UI Security Guidance

Consult [content/browser/SECURITY.md][content-browser-sec],
[ui/views/SECURITY.md][views-sec], and [Security for Agents][sec-agents] first
for canonical UI-thread message pump, nested loop, Views `Widget` lifetime,
threat-model, and report-validation rules.

## Browser UI Lifetime Hazards

`//chrome/browser/ui` coordinates browser windows (`Browser`, `BrowserView`),
tabs (`TabStripModel`), `WebContents`, dialogs, and bubbles on the browser UI
thread. Re-entrancy bugs in `//chrome/browser/ui` occur when UI code makes a
call that triggers a synchronous callback or nested message loop (under the
mechanisms and constraints described in
[content/browser/SECURITY.md][content-browser-sec]) that:

* closes a `Browser` or mutates `TabStripModel` (closing, detaching, or
  switching tabs),
* synchronously destroys a dialog or bubble `Widget` (note that
  `BubbleDialogDelegate` focus-loss deactivation calls
  `Widget::CloseWithReason()`, which only destroys the `Widget` synchronously
  under the conditions in [ui/views/SECURITY.md][views-sec], such as
  `MakeCloseSynchronous()`), or
* destroys a `WebContents` or its associated tab features/controllers

while outer stack frames still hold raw pointers to those objects.

## ChromeOS Locked Fullscreen (Out of Scope)

See [chrome/browser/ui/chromeos/locked_state/SECURITY.md][locked-state-sec]
for the full security policy covering ChromeOS locked fullscreen and
trusted-pinned mode (`chromeos::LockedStateController`,
`platform_util::IsBrowserLockedFullscreen()`, `lockWindowFullscreenPrivate`, and
OnTask locked states).

* **Locked fullscreen is not a security boundary:** ChromeOS locked fullscreen
  (`WindowPinType::kTrustedPinned`) is a UX/workflow confinement feature, not a
  security boundary or part of Chromium's security threat model. Confinement
  bypasses or escapes while a window is locked fullscreen (such as background
  tabs activating themselves via `beforeunload` / `ActivateContents()`, unclosed
  bubbles or UI surfaces like Tab Search or Glic, keyboard shortcuts/commands,
  active-user switches, or pre-staged user tabs) are **out of scope** and are
  **not** security vulnerabilities (file as functional bugs only if
  unintended).
* **Security features take precedence:** Existing browser security defenses
  (such as activating the requesting tab before showing an app-modal dialog to
  prevent cross-origin dialog spoofing) must not be disabled solely to maintain
  locked-fullscreen confinement.

[content-browser-sec]: /content/browser/SECURITY.md
[views-sec]: /ui/views/SECURITY.md
[sec-agents]: /docs/security/security-for-agents.md
[locked-state-sec]: /chrome/browser/ui/chromeos/locked_state/SECURITY.md
