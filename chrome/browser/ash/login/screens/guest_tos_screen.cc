// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/guest_tos_screen.h"

#include "ash/constants/ash_switches.h"
#include "ash/constants/url_constants.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/metrics/cros_pre_consent_metrics_manager.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_service.h"
namespace ash {
namespace {

constexpr const char kUserActionBackClicked[] = "back-button";
constexpr const char kUserActionCancelClicked[] = "cancel";
constexpr const char kUserActionGuestToSAccept[] = "guest-tos-accept";

std::string GetGoogleEulaOnlineUrl(const std::string& application_locale) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(ash::external_urls::kGoogleEulaOnlineURLPath,
                            application_locale.c_str());
}

std::string GetCrosEulaOnlineUrl(const std::string& application_locale) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(ash::external_urls::kCrosEulaOnlineURLPath,
                            application_locale.c_str());
}

}  // namespace

// static
std::string GuestTosScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::ACCEPT:
      return "Accept";
    case Result::BACK:
      return "Back";
    case Result::CANCEL:
      return "Cancel";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

GuestTosScreen::GuestTosScreen(
    PrefService* local_state,
    const ApplicationLocaleStorage* application_locale_storage,
    base::WeakPtr<GuestTosScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(GuestTosScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

GuestTosScreen::~GuestTosScreen() = default;

void GuestTosScreen::ShowImpl() {
  if (!view_)
    return;
  const std::string& locale = application_locale_storage_->Get();
  view_->Show(GetGoogleEulaOnlineUrl(locale), GetCrosEulaOnlineUrl(locale));
}

void GuestTosScreen::HideImpl() {}

void GuestTosScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBackClicked) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancelClicked) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionGuestToSAccept) {
    CHECK_EQ(args.size(), 2u);
    const bool enable_usage_stats = args[1].GetBool();
    OnAccept(enable_usage_stats);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void GuestTosScreen::OnAccept(bool enable_usage_stats) {
  // Disable pre-consent metrics for guest as guest has expressed consent. This
  // is critical as crash_reportor is depending on the disable operation of
  // CrOSPreConsentMetricsManager to end pre-consent stage.
  if (metrics::CrOSPreConsentMetricsManager::Get()) {
    metrics::CrOSPreConsentMetricsManager::Get()->Disable();
  }

  // Store guest consent to local state so that correct metrics consent can be
  // loaded after browser restart.
  local_state_->SetBoolean(prefs::kOobeGuestMetricsEnabled, enable_usage_stats);
  local_state_->CommitPendingWrite(
      base::BindOnce(&GuestTosScreen::OnOobeGuestPrefWriteDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GuestTosScreen::OnOobeGuestPrefWriteDone() {
  DCHECK(exit_callback_);

  exit_callback_.Run(Result::ACCEPT);
}

}  // namespace ash
