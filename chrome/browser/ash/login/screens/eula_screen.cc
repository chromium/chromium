// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/eula_screen.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

namespace chromeos {
namespace {

constexpr const char kUserActionAcceptButtonClicked[] = "accept-button";
constexpr const char kUserActionBackButtonClicked[] = "back-button";
constexpr const char kUserActionShowAdditionalTos[] = "show-additional-tos";
constexpr const char kUserActionShowSecuritySettings[] =
    "show-security-settings";
constexpr const char kUserActionShowStatsUsageLearnMore[] =
    "show-stats-usage-learn-more";
constexpr const char kUserActionUnselectStatsUsage[] = "unselect-stats-usage";
constexpr const char kUserActionSelectStatsUsage[] = "select-stats-usage";

struct EulaUserAction {
  const char* name_;
  EulaScreen::UserAction uma_name_;
};

const EulaUserAction actions[] = {
    {kUserActionAcceptButtonClicked,
     EulaScreen::UserAction::kAcceptButtonClicked},
    {kUserActionBackButtonClicked, EulaScreen::UserAction::kBackButtonClicked},
    {kUserActionShowAdditionalTos, EulaScreen::UserAction::kShowAdditionalTos},
    {kUserActionShowSecuritySettings,
     EulaScreen::UserAction::kShowSecuritySettings},
    {kUserActionShowStatsUsageLearnMore,
     EulaScreen::UserAction::kShowStatsUsageLearnMore},
    {kUserActionUnselectStatsUsage,
     EulaScreen::UserAction::kUnselectStatsUsage},
    {kUserActionSelectStatsUsage, EulaScreen::UserAction::kSelectStatsUsage},
};

void RecordEulaScreenAction(EulaScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.EulaScreen.UserActions", value);
}

bool IsEulaUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_)
      return true;
  }
  return false;
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordEulaScreenAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED() << "Unexpected action id: " << action_id;
}

// Reflects the value of usage statistics reporting checkbox shown in eula
// UI. The value is expected to survive EulaScreen res-hows within a single
// session. For example if a user unchecks the checkbox, goes back, and then
// gets to EULA screen again, the checkbox should be unchecked.
bool g_usage_statistics_reporting_enabled = true;

}  // namespace

// static
std::string EulaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED_WITH_USAGE_STATS_REPORTING:
      return "AcceptedWithStats";
    case Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING:
      return "AcceptedWithoutStats";
    case Result::BACK:
      return "Back";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

EulaScreen::EulaScreen(EulaView* view, const ScreenExitCallback& exit_callback)
    : BaseScreen(EulaView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

EulaScreen::~EulaScreen() {
  if (view_)
    view_->Unbind();
}

bool EulaScreen::MaybeSkip(WizardContext* context) {
  // Remora (CfM) devices are enterprise only. To enroll device it is required
  // to accept ToS on the server side. Thus for such devices it is not needed to
  // accept EULA on the client side.
  // TODO(https://crbug.com/1190897): Refactor to use `context` instead.
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void EulaScreen::SetUsageStatsEnabled(bool enabled) {
  g_usage_statistics_reporting_enabled = enabled;
}

bool EulaScreen::IsUsageStatsEnabled() const {
  return g_usage_statistics_reporting_enabled;
}

void EulaScreen::OnViewDestroyed(EulaView* view) {
  if (view_ == view)
    view_ = NULL;
}

void EulaScreen::ShowImpl() {
  // Command to own the TPM.
  TpmManagerClient::Get()->TakeOwnership(::tpm_manager::TakeOwnershipRequest(),
                                         base::DoNothing());
  if (WizardController::UsingHandsOffEnrollment())
    OnUserAction(kUserActionAcceptButtonClicked);
  else if (view_)
    view_->Show();
}

void EulaScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void EulaScreen::OnUserAction(const std::string& action_id) {
  if (!IsEulaUserAction(action_id)) {
    BaseScreen::OnUserAction(action_id);
    return;
  }
  RecordUserAction(action_id);
  if (action_id == kUserActionShowStatsUsageLearnMore) {
    ShowStatsUsageLearnMore();
  } else if (action_id == kUserActionShowAdditionalTos) {
    ShowAdditionalTosDialog();
  } else if (action_id == kUserActionShowSecuritySettings) {
    ShowSecuritySettingsDialog();
  } else if (action_id == kUserActionSelectStatsUsage) {
    SetUsageStatsEnabled(true);
  } else if (action_id == kUserActionUnselectStatsUsage) {
    SetUsageStatsEnabled(false);
  } else if (action_id == kUserActionAcceptButtonClicked) {
    exit_callback_.Run(g_usage_statistics_reporting_enabled
                           ? Result::ACCEPTED_WITH_USAGE_STATS_REPORTING
                           : Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);
  } else if (action_id == kUserActionBackButtonClicked) {
    exit_callback_.Run(Result::BACK);
  }
}

bool EulaScreen::HandleAccelerator(ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kStartEnrollment) {
    context()->enrollment_triggered_early = true;
    return true;
  }
  return false;
}

void EulaScreen::ShowStatsUsageLearnMore() {
  if (view_)
    view_->ShowStatsUsageLearnMore();
}

void EulaScreen::ShowAdditionalTosDialog() {
  if (view_)
    view_->ShowAdditionalTosDialog();
}

void EulaScreen::ShowSecuritySettingsDialog() {
  if (view_)
    view_->ShowSecuritySettingsDialog();
}

}  // namespace chromeos
