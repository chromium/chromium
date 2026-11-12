# Browser UI Re-entrancy and Lifetime Guidance

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

[content-browser-sec]: /content/browser/SECURITY.md
[views-sec]: /ui/views/SECURITY.md
[sec-agents]: /docs/security/security-for-agents.md
