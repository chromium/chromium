// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_WIN_H_

namespace safe_browsing {

// Decides if and when a reboot prompt dialog should be shown. If a dialog is
// shown, provides functions, such as |Accept()| and |Cancel()|, that should
// be called by the Chrome Cleaner Reboot UI in response to user actions.
//
// Implementations manage their own lifetimes and delete themselves once they
// decide that no dialog should be shown or, if the dialog has been shown, once
// the user interacts with it.
class ChromeCleanerRebootDialogController {
 public:
  // Called by the reboot dialog when user accepts the reboot prompt. Once
  // |Accept()| has been called, the controller will eventually delete itself
  // and no member functions should be called after that.
  virtual void Accept() = 0;
  // Called by the reboot dialog when the dialog is closed via the cancel
  // button. Once |Cancel()| has been called, the controller will eventually
  // delete itself and no member functions should be called after that.
  virtual void Cancel() = 0;
  // Called by the reboot dialog when the dialog is closed by some other means
  // than the cancel button (for example, by pressing Esc or clicking the 'x' on
  // the top of the dialog). After a call to |Close()|, the controller will
  // eventually delete itself and no member functions should be called after
  // that.
  virtual void Close() = 0;

 protected:
  virtual ~ChromeCleanerRebootDialogController() = default;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_WIN_H_
