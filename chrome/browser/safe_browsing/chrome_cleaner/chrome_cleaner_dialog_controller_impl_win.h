// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_IMPL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_IMPL_WIN_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_win.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace safe_browsing {

class ChromeCleanerPromptDelegate {
 public:
  virtual void ShowChromeCleanerPrompt(
      Browser* browser,
      safe_browsing::ChromeCleanerDialogController* dialog_controller,
      safe_browsing::ChromeCleanerController* cleaner_controller) = 0;
  virtual ~ChromeCleanerPromptDelegate();
};

class ChromeCleanerDialogControllerImpl
    : public ChromeCleanerDialogController,
      public ChromeCleanerController::Observer,
      public BrowserListObserver {
 public:
  // An instance should only be created when |cleaner_controller| is in the
  // kScanning state.
  explicit ChromeCleanerDialogControllerImpl(
      ChromeCleanerController* cleaner_controller);

  ChromeCleanerDialogControllerImpl(const ChromeCleanerDialogControllerImpl&) =
      delete;
  ChromeCleanerDialogControllerImpl& operator=(
      const ChromeCleanerDialogControllerImpl&) = delete;

  // ChromeCleanerDialogController overrides.
  void DialogShown() override;
  void Accept(bool logs_enabled) override;
  void Cancel() override;
  void Close() override;
  void ClosedWithoutUserInteraction() override;
  void DetailsButtonClicked(bool logs_enabled) override;
  void SetLogsEnabled(bool logs_enabled) override;
  bool LogsEnabled() override;
  bool LogsManaged() override;

  // ChromeCleanerController::Observer overrides.
  void OnIdle(ChromeCleanerController::IdleReason idle_reason) override;
  void OnScanning() override;
  void OnInfected(bool is_powered_by_partner,
                  const ChromeCleanerScannerResults& reported_results) override;
  void OnCleaning(bool is_powered_by_partner,
                  const ChromeCleanerScannerResults& reported_results) override;
  void OnRebootRequired() override;

  // BrowserListObserver overrides.
  void OnBrowserSetLastActive(Browser* browser) override;

  // Test specific methods.
  void SetPromptDelegateForTests(ChromeCleanerPromptDelegate* delegate);

 protected:
  ~ChromeCleanerDialogControllerImpl() override;

 private:
  void OnInteractionDone();
  void ShowChromeCleanerPrompt();

  raw_ptr<ChromeCleanerController, LeakedDanglingUntriaged>
      cleaner_controller_ = nullptr;
  bool dialog_shown_ = false;

  // In case there is no browser available to prompt a user
  // signal it, this way we can prompt it once a browser gets available..
  bool prompt_pending_ = false;
  raw_ptr<Browser, LeakedDanglingUntriaged> browser_ = nullptr;
  std::unique_ptr<ChromeCleanerPromptDelegate> prompt_delegate_impl_;
  raw_ptr<ChromeCleanerPromptDelegate, DanglingUntriaged> prompt_delegate_ =
      nullptr;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_IMPL_WIN_H_
