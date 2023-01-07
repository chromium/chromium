// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_

namespace safe_browsing {

// Provides functions, such as |Accept()| and |Cancel()|, that should
// be called by the Chrome Cleaner UI in response to user actions.
//
// Implementations manage their own lifetimes and delete themselves once the
// Cleaner dialog has been dismissed and either of |Accept()|, |Cancel()|,
// |Close()| or |ClosedWithoutUserInteraction()| have been called.
class ChromeCleanerDialogController {
 public:
  // Called by the Cleaner dialog when the dialog has been shown. Used for
  // reporting metrics.
  virtual void DialogShown() = 0;
  // Called by the Cleaner dialog when user accepts the prompt. Once |Accept()|
  // has been called, the controller will eventually delete itself and no member
  // functions should be called after that.
  virtual void Accept(bool logs_enabled) = 0;
  // Called by the Cleaner dialog when the dialog is closed via the cancel
  // button. Once |Cancel()| has been called, the controller will eventually
  // delete itself and no member functions should be called after that.
  virtual void Cancel() = 0;
  // Called by the Cleaner dialog when the dialog is closed by some other means
  // than the cancel button (for example, by pressing Esc or clicking the 'x' on
  // the top of the dialog). After a call to |Dismiss()|, the controller will
  // eventually delete itself and no member functions should be called after
  // that.
  virtual void Close() = 0;
  // Called by the Cleaner dialog when the dialog is closed, without user
  // interaction, when the ChromeCleanerController leaves the kInfected
  // state. This can happen due to errors when communicating with the Chrome
  // Cleaner process or if the user interacts with the Chrome Cleaner webui page
  // in a different browser window.
  virtual void ClosedWithoutUserInteraction() = 0;
  // Called when the details button is clicked, after which the dialog will
  // close. After a call to |DetailsButtonClicked()|, the controller will
  // eventually delete itself and no member functions should be called after
  // that.
  virtual void DetailsButtonClicked(bool logs_enabled) = 0;

  // To be called by the dialog when the user changes the state of the logs
  // upload permission checkbox.
  virtual void SetLogsEnabled(bool logs_enabled) = 0;
  // Returns whether logs upload is currently enabled. Used to set the dialog's
  // default permission checkbox state.
  virtual bool LogsEnabled() = 0;
  // Returns whether logs upload is currently managed by policy.
  virtual bool LogsManaged() = 0;

 protected:
  virtual ~ChromeCleanerDialogController() {}
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_
