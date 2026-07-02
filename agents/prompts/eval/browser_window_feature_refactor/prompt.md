Refactor the fake QR browser-window method into a BrowserWindowFeatures-owned
feature controller, following the BrowserWindowFeatures conventions. This is a
behavior-preserving refactor.

The setup patch creates a small fake Bedrock-style surface under
`chrome/browser/ui/browser_window/test/`:

- `FakeBrowserWindow` currently owns `ShowQRCodeBubble()`, which records the
  message `"QR bubble shown by BrowserWindow"`.
- `FakeBrowserWindowFeatures` already has lifecycle hooks and a `TYPE_NORMAL`
  block, and currently calls `window->ShowQRCodeBubble()` from it.

Do the following:

- Create `FakeQRCodeWindowController` in new files
  `chrome/browser/ui/browser_window/test/fake_qrcode_window_controller_eval.h`
  and `chrome/browser/ui/browser_window/test/fake_qrcode_window_controller_eval.cc`.
- Move the QR behavior out of `FakeBrowserWindow` into the controller. The
  controller should record the message `"QR bubble shown by feature controller"`.
- Have `FakeBrowserWindowFeatures` own the controller and invoke it from the
  same `TYPE_NORMAL` initialization path that previously called the
  `FakeBrowserWindow` method, so the observable behavior is preserved.
