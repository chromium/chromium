// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/website_telemetry_reporting_nudge_controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

using ::session_manager::SessionManager;
using ::session_manager::SessionState;

namespace reporting {
namespace {

using std::literals::string_view_literals::operator""sv;

constexpr std::string_view kWebsiteTelemetryReportingHelpArticleUrl =
    "https://support.google.com/chrome/a?p=chromeos_telemetry_reporting"sv;

}  // namespace

WebsiteTelemetryReportingNudgeController::
    WebsiteTelemetryReportingNudgeController(
        base::WeakPtr<Profile> profile,
        ReportingSettings* reporting_settings)
    : profile_(profile), reporting_settings_(reporting_settings) {
  CHECK(reporting_settings_);
  Initialize();
}

WebsiteTelemetryReportingNudgeController::
    ~WebsiteTelemetryReportingNudgeController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
}

void WebsiteTelemetryReportingNudgeController::OnSessionStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SessionManager::Get()->session_state() == SessionState::ACTIVE) {
    MaybeShowNudge();
    session_manager_observer_.Reset();
  }
}

void WebsiteTelemetryReportingNudgeController::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_) {
    return;
  }
  change_subscription_ = reporting_settings_->AddSettingsObserver(
      kReportWebsiteTelemetry,
      base::BindRepeating(
          &WebsiteTelemetryReportingNudgeController::MaybeShowNudge,
          weak_ptr_factory_.GetWeakPtr()));

  // If the current session is not active yet, delay processing nudge
  // notifications until after the session is active.
  auto* const session_manager = SessionManager::Get();
  if (session_manager->session_state() != SessionState::ACTIVE) {
    session_manager_observer_.Observe(session_manager);
    return;
  }
  MaybeShowNudge();
}

void WebsiteTelemetryReportingNudgeController::MaybeShowNudge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_) {
    return;
  }

  bool report_website_telemetry_enabled;
  if (!reporting_settings_->GetReportingEnabled(
          kReportWebsiteTelemetry, &report_website_telemetry_enabled)) {
    // Failed to fetch current policy setting value.
    return;
  }

  auto* const user_prefs = profile_->GetPrefs();
  const bool report_website_telemetry_previously_enabled =
      user_prefs->GetBoolean(kReportWebsiteTelemetryPreviouslyEnabled);
  if (report_website_telemetry_enabled &&
      !report_website_telemetry_previously_enabled) {
    // Show nudge.
    ::ash::AnchoredNudgeData nudge_data(
        kWebsiteTelemetryReportingNudgeId,
        ::ash::NudgeCatalogName::kWebsiteTelemetryReportingNudge,
        l10n_util::GetStringUTF16(
            IDS_CHROMEOS_WEBSITE_TELEMETRY_REPORTING_NUDGE_TEXT));
    nudge_data.primary_button_text = l10n_util::GetStringUTF16(
        IDS_CHROMEOS_WEBSITE_TELEMETRY_REPORTING_NUDGE_PRIMARY_BUTTON);
    nudge_data.primary_button_callback = base::BindRepeating(
        &WebsiteTelemetryReportingNudgeController::ShowHelpArticle,
        weak_ptr_factory_.GetWeakPtr());
    ::ash::AnchoredNudgeManager::Get()->Show(nudge_data);
  }

  // Update pref entry with current value.
  user_prefs->SetBoolean(kReportWebsiteTelemetryPreviouslyEnabled,
                         report_website_telemetry_enabled);
}

void WebsiteTelemetryReportingNudgeController::ShowHelpArticle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_) {
    return;
  }
  ShowSingletonTab(profile_.get(),
                   GURL(kWebsiteTelemetryReportingHelpArticleUrl));
}
}  // namespace reporting
