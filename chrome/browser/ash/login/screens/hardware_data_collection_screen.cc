// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

constexpr const char kUserActionAcceptButtonClicked[] = "accept-button";
constexpr const char kUserActionSelectHWDataUsage[] = "select-hw-data-usage";

}  // namespace

// static
std::string HWDataCollectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::ACCEPTED_WITH_HW_DATA_USAGE:
      return "AcceptedWithHWDataUsage";
    case Result::ACCEPTED_WITHOUT_HW_DATA_USAGE:
      return "AcceptedWithoutHWDataUsage";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

HWDataCollectionScreen::HWDataCollectionScreen(
    base::WeakPtr<HWDataCollectionView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(HWDataCollectionView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

HWDataCollectionScreen::~HWDataCollectionScreen() = default;

void HWDataCollectionScreen::SetHWDataUsageEnabled(bool enabled) {
  hw_data_usage_enabled_ = enabled;
}

bool HWDataCollectionScreen::MaybeSkip(WizardContext& context) {
  if (!switches::IsRevenBranding() || !context.is_branded_build) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  bool is_owner = false;
  // Taking device ownership can take some time, so we can't rely on it here.
  // However it can be already checked during ConsolidateConsentScreen.
  if (context.is_owner_flow.has_value()) {
    is_owner = context.is_owner_flow.value();
  } else {
    // If no, check that the device is not managed and user is either already
    // marked as an owner in user_manager or is the first on the device.
    auto* user_manager = user_manager::UserManager::Get();
    is_owner = !ash::InstallAttributes::Get()->IsEnterpriseManaged() &&
               (user_manager->IsCurrentUserOwner() ||
                user_manager->GetUsers().size() == 1);
  }
  if (!is_owner) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  if (context.skip_post_login_screens_for_tests) {
    // Set a default value if the screen should be shown, but is skipped because
    // of the test flow. This value is important, as we rely on it during update
    // flow from CloudReady to Chrome OS Flex and it should be set after owner
    // of the device has already logged in.
    HWDataUsageController::Get()->Set(ProfileManager::GetActiveUserProfile(),
                                      base::Value(false));
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void HWDataCollectionScreen::ShowImpl() {
  if (view_) {
    view_->Show(hw_data_usage_enabled_);
  }
  if (context()->extra_factors_token) {
    session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
        context()->extra_factors_token.value());
  }
}

void HWDataCollectionScreen::HideImpl() {
  session_refresher_.reset();
}

void HWDataCollectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionAcceptButtonClicked) {
    HWDataUsageController::Get()->Set(ProfileManager::GetActiveUserProfile(),
                                      base::Value(hw_data_usage_enabled_));
    exit_callback_.Run(hw_data_usage_enabled_
                           ? Result::ACCEPTED_WITH_HW_DATA_USAGE
                           : Result::ACCEPTED_WITHOUT_HW_DATA_USAGE);
  } else if (action_id == kUserActionSelectHWDataUsage) {
    CHECK_EQ(args.size(), 2u);
    const bool enabled = args[1].GetBool();
    SetHWDataUsageEnabled(enabled);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
