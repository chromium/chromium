// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

namespace {

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum PromptDialogResponseHistogramValue {
  PROMPT_DIALOG_RESPONSE_ACCEPTED = 0,
  PROMPT_DIALOG_RESPONSE_DETAILS = 1,
  PROMPT_DIALOG_RESPONSE_CANCELLED = 2,
  PROMPT_DIALOG_RESPONSE_DISMISSED = 3,
  PROMPT_DIALOG_RESPONSE_CLOSED_WITHOUT_USER_INTERACTION = 4,

  PROMPT_DIALOG_RESPONSE_MAX,
};

void RecordPromptDialogResponseHistogram(
    PromptDialogResponseHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.PromptDialogResponse", value,
                            PROMPT_DIALOG_RESPONSE_MAX);
}

}  // namespace

ChromeCleanerPromptDelegate::~ChromeCleanerPromptDelegate() = default;

class ChromeCleanerPromptDelegateImpl : public ChromeCleanerPromptDelegate {
 public:
  void ShowChromeCleanerPrompt(
      Browser* browser,
      ChromeCleanerDialogController* dialog_controller,
      ChromeCleanerController* cleaner_controller) override {
    chrome::ShowChromeCleanerPrompt(browser, dialog_controller,
                                    cleaner_controller);
  }
};

ChromeCleanerDialogControllerImpl::ChromeCleanerDialogControllerImpl(
    ChromeCleanerController* cleaner_controller)
    : cleaner_controller_(cleaner_controller),
      prompt_delegate_impl_(
          std::make_unique<ChromeCleanerPromptDelegateImpl>()) {
  DCHECK(cleaner_controller_);
  DCHECK_EQ(ChromeCleanerController::State::kScanning,
            cleaner_controller_->state());

  cleaner_controller_->AddObserver(this);
  prompt_delegate_ = prompt_delegate_impl_.get();
}

ChromeCleanerDialogControllerImpl::~ChromeCleanerDialogControllerImpl() =
    default;

void ChromeCleanerDialogControllerImpl::DialogShown() {
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.PromptDialog_Shown"));
}

void ChromeCleanerDialogControllerImpl::Accept(bool logs_enabled) {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_ACCEPTED);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.PromptDialog_Accepted"));

  Profile* profile = browser_->profile();

  cleaner_controller_->ReplyWithUserResponse(
      profile,
      logs_enabled
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
  chrome_cleaner_util::OpenCleanupPage(
      browser_, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Cancel() {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_CANCELLED);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.PromptDialog_Canceled"));

  Profile* profile = browser_->profile();

  cleaner_controller_->ReplyWithUserResponse(
      profile, ChromeCleanerController::UserResponse::kDenied);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Close() {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_DISMISSED);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.PromptDialog_Dismissed"));

  Profile* profile = browser_->profile();

  cleaner_controller_->ReplyWithUserResponse(
      profile, ChromeCleanerController::UserResponse::kDismissed);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::ClosedWithoutUserInteraction() {
  RecordPromptDialogResponseHistogram(
      PROMPT_DIALOG_RESPONSE_CLOSED_WITHOUT_USER_INTERACTION);
  base::RecordAction(base::UserMetricsAction(
      "SoftwareReporter.PromptDialog_ClosedWithoutUserInteraction"));
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::DetailsButtonClicked(
    bool logs_enabled) {
  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_DETAILS);
  base::RecordAction(base::UserMetricsAction(
      "SoftwareReporter.PromptDialog_DetailsButtonClicked"));

  cleaner_controller_->SetLogsEnabled(browser_->profile(), logs_enabled);
  chrome_cleaner_util::OpenCleanupPage(
      browser_, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::SetLogsEnabled(bool logs_enabled) {
  cleaner_controller_->SetLogsEnabled(browser_->profile(), logs_enabled);
  if (logs_enabled) {
    base::RecordAction(base::UserMetricsAction(
        "SoftwareReporter.PromptDialog.LogsPermissionCheckbox_Enabled"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "SoftwareReporter.PromptDialog.LogsPermissionCheckbox_Disabled"));
  }
}

bool ChromeCleanerDialogControllerImpl::LogsEnabled() {
  return cleaner_controller_->logs_enabled(browser_->profile());
}

bool ChromeCleanerDialogControllerImpl::LogsManaged() {
  return cleaner_controller_->IsReportingManagedByPolicy(browser_->profile());
}

void ChromeCleanerDialogControllerImpl::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnScanning() {
  // This notification is received when the object is first added as an observer
  // of cleaner_controller_.
  DCHECK(!dialog_shown_);

  // TODO(alito): Close the dialog in case it has been kept open until the next
  // time the prompt is going to be shown. http://crbug.com/734689
}

void ChromeCleanerDialogControllerImpl::OnInfected(
    bool is_powered_by_partner,
    const ChromeCleanerScannerResults& reported_results) {
  DCHECK(!dialog_shown_);

  browser_ = chrome_cleaner_util::FindBrowser();
  if (!browser_) {
    RecordPromptNotShownWithReasonHistogram(
        NO_PROMPT_REASON_WAITING_FOR_BROWSER);
    prompt_pending_ = true;
    BrowserList::AddObserver(this);
    return;
  }
  ShowChromeCleanerPrompt();
}

void ChromeCleanerDialogControllerImpl::OnCleaning(
    bool is_powered_by_partner,
    const ChromeCleanerScannerResults& reported_results) {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnRebootRequired() {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnBrowserSetLastActive(
    Browser* browser) {
  DCHECK(prompt_pending_);
  DCHECK(browser);
  DCHECK(!browser_);

  browser_ = browser;
  ShowChromeCleanerPrompt();
  prompt_pending_ = false;
  BrowserList::RemoveObserver(this);
}

void ChromeCleanerDialogControllerImpl::SetPromptDelegateForTests(
    ChromeCleanerPromptDelegate* delegate) {
  prompt_delegate_ = delegate;
}

void ChromeCleanerDialogControllerImpl::ShowChromeCleanerPrompt() {
  DCHECK(browser_);
  Profile* profile = browser_->profile();
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Don't show the prompt again if it's been shown before for this profile and
  // for the current variations seed.
  const std::string incoming_seed =
      cleaner_controller_->GetIncomingPromptSeed();
  const std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
  if (!incoming_seed.empty() && incoming_seed != old_seed)
    prefs->SetString(prefs::kSwReporterPromptSeed, incoming_seed);

  prompt_delegate_->ShowChromeCleanerPrompt(browser_, this,
                                            cleaner_controller_);
  dialog_shown_ = true;
}

void ChromeCleanerDialogControllerImpl::OnInteractionDone() {
  if (prompt_pending_) {
    BrowserList::RemoveObserver(this);
    prompt_pending_ = false;
  }

  cleaner_controller_->RemoveObserver(this);
  delete this;
}

}  // namespace safe_browsing
