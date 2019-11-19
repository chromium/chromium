// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_impl_win.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

namespace {

// These values are used to send UMA information and are replicated in the
// enums.xml, so the order MUST NOT CHANGE
enum SettingsPageActiveOnRebootRequiredHistogramValue {
  SETTINGS_PAGE_ON_REBOOT_REQUIRED_NO_BROWSER = 0,
  SETTINGS_PAGE_ON_REBOOT_REQUIRED_NOT_ACTIVE_TAB = 1,
  SETTINGS_PAGE_ON_REBOOT_REQUIRED_ACTIVE_TAB = 2,

  SETTINGS_PAGE_ON_REBOOT_REQUIRED_MAX,
};

class PromptDelegateImpl
    : public ChromeCleanerRebootDialogControllerImpl::PromptDelegate {
 public:
  void ShowChromeCleanerRebootPrompt(
      Browser* browser,
      ChromeCleanerRebootDialogControllerImpl* controller) override;
  void OnSettingsPageIsActiveTab() override;
};

void PromptDelegateImpl::ShowChromeCleanerRebootPrompt(
    Browser* browser,
    ChromeCleanerRebootDialogControllerImpl* controller) {
  DCHECK(browser);
  DCHECK(controller);

  chrome::ShowChromeCleanerRebootPrompt(browser, controller);
}

void PromptDelegateImpl::OnSettingsPageIsActiveTab() {}

void RecordSettingsPageActiveOnRebootRequired(
    SettingsPageActiveOnRebootRequiredHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SoftwareReporter.Cleaner.SettingsPageActiveOnRebootRequired", value,
      SETTINGS_PAGE_ON_REBOOT_REQUIRED_MAX);
}

}  // namespace

ChromeCleanerRebootDialogControllerImpl::PromptDelegate::~PromptDelegate() =
    default;

// static
ChromeCleanerRebootDialogControllerImpl*
ChromeCleanerRebootDialogControllerImpl::Create(
    ChromeCleanerController* cleaner_controller) {
  return Create(cleaner_controller, std::make_unique<PromptDelegateImpl>());
}

// static
ChromeCleanerRebootDialogControllerImpl*
ChromeCleanerRebootDialogControllerImpl::Create(
    ChromeCleanerController* cleaner_controller,
    std::unique_ptr<PromptDelegate> prompt_delegate) {
  ChromeCleanerRebootDialogControllerImpl* controller =
      new ChromeCleanerRebootDialogControllerImpl(cleaner_controller,
                                                  std::move(prompt_delegate));
  controller->MaybeStartRebootPrompt();
  return controller;
}

void ChromeCleanerRebootDialogControllerImpl::Accept() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cleaner_controller_->Reboot();

  // If reboot fails, we don't want this object to continue existing.
  OnInteractionDone();
}

void ChromeCleanerRebootDialogControllerImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnInteractionDone();
}

void ChromeCleanerRebootDialogControllerImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnInteractionDone();
}

void ChromeCleanerRebootDialogControllerImpl::OnBrowserSetLastActive(
    Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(waiting_for_browser_);
  DCHECK(browser);

  waiting_for_browser_ = false;
  BrowserList::RemoveObserver(this);
  StartRebootPromptForBrowser(browser);
}

ChromeCleanerRebootDialogControllerImpl::
    ChromeCleanerRebootDialogControllerImpl(
        ChromeCleanerController* cleaner_controller,
        std::unique_ptr<PromptDelegate> prompt_delegate)
    : cleaner_controller_(cleaner_controller),
      prompt_delegate_(std::move(prompt_delegate)) {
  DCHECK(cleaner_controller_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(ChromeCleanerController::State::kRebootRequired,
            cleaner_controller_->state());
}

ChromeCleanerRebootDialogControllerImpl::
    ~ChromeCleanerRebootDialogControllerImpl() = default;

void ChromeCleanerRebootDialogControllerImpl::MaybeStartRebootPrompt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = chrome_cleaner_util::FindBrowser();

  if (browser == nullptr) {
    RecordSettingsPageActiveOnRebootRequired(
        SETTINGS_PAGE_ON_REBOOT_REQUIRED_NO_BROWSER);

    waiting_for_browser_ = true;
    BrowserList::AddObserver(this);

    return;
  }

  StartRebootPromptForBrowser(browser);
}

void ChromeCleanerRebootDialogControllerImpl::StartRebootPromptForBrowser(
    Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chrome_cleaner_util::CleanupPageIsActiveTab(browser)) {
    RecordSettingsPageActiveOnRebootRequired(
        SETTINGS_PAGE_ON_REBOOT_REQUIRED_ACTIVE_TAB);

    prompt_delegate_->OnSettingsPageIsActiveTab();
    OnInteractionDone();
    return;
  }

  RecordSettingsPageActiveOnRebootRequired(
      SETTINGS_PAGE_ON_REBOOT_REQUIRED_NOT_ACTIVE_TAB);
  prompt_delegate_->ShowChromeCleanerRebootPrompt(browser, this);
}

void ChromeCleanerRebootDialogControllerImpl::OnInteractionDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete this;
}

}  // namespace safe_browsing
