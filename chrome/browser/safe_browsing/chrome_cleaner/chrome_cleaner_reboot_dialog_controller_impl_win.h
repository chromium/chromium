// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_IMPL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_IMPL_WIN_H_

#include "base/sequence_checker.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_win.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace safe_browsing {

class ChromeCleanerRebootDialogControllerImpl
    : public ChromeCleanerRebootDialogController,
      public BrowserListObserver {
 public:
  class PromptDelegate {
   public:
    virtual ~PromptDelegate();
    virtual void ShowChromeCleanerRebootPrompt(
        Browser* browser,
        ChromeCleanerRebootDialogControllerImpl* controller) = 0;
    virtual void OnSettingsPageIsActiveTab() = 0;
  };

  // Creates a new controller object and either starts or schedules the reboot
  // prompt flow. The new controller object must be created and only accessed
  // on the UI thread.
  static ChromeCleanerRebootDialogControllerImpl* Create(
      ChromeCleanerController* cleaner_controller);

  // Same as Create() with a custom delegate.
  static ChromeCleanerRebootDialogControllerImpl* Create(
      ChromeCleanerController* cleaner_controller,
      std::unique_ptr<PromptDelegate> delegate);

  // ChromeCleanerRebootDialogController overrides.
  void Accept() override;
  void Cancel() override;
  void Close() override;

  // chrome::BrowserListObserver overrides.
  void OnBrowserSetLastActive(Browser* browser) override;

 protected:
  // Use Create() to create and initialize new objects.
  ChromeCleanerRebootDialogControllerImpl(
      ChromeCleanerController* cleaner_controller,
      std::unique_ptr<PromptDelegate> delegate);
  ~ChromeCleanerRebootDialogControllerImpl() override;

  // Initiate the reboot prompt flow if there is an active browser window.
  // Otherwise, registers to start the reboot prompt once a new window becomes
  // available.
  void MaybeStartRebootPrompt();

  // Shows the reboot prompt dialog in |browser| if the reboot prompt experiment
  // is on and the Settings page containing Chrome Cleanup UI is not the
  // currently active tab. Otherwise, this will reopen the Settings page on a
  // background tab.
  void StartRebootPromptForBrowser(Browser* browser);

  void OnInteractionDone();

  ChromeCleanerController* cleaner_controller_ = nullptr;

  std::unique_ptr<PromptDelegate> prompt_delegate_;

  bool waiting_for_browser_ = false;

  // Used to check that modifications to |profile_resetters_| are sequenced
  // correctly.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_REBOOT_DIALOG_CONTROLLER_IMPL_WIN_H_
