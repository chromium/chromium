// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/hardware_data_collection_screen_handler.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionAcceptButtonClicked[] = "accept-button";
constexpr const char kUserActionShowLearnMore[] = "show-learn-more";
constexpr const char kUserActionUnselectHWDataUsage[] =
    "unselect-hw-data-usage";
constexpr const char kUserActionSelectHWDataUsage[] = "select-hw-data-usage";

}  // namespace

// static
std::string HWDataCollectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED_WITH_HW_DATA_USAGE:
      return "AcceptedWithHWDataUsage";
    case Result::ACCEPTED_WITHOUT_HW_DATA_USAGE:
      return "AcceptedWithoutHWDataUsage";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

HWDataCollectionScreen::HWDataCollectionScreen(
    HWDataCollectionView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(HWDataCollectionView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view);
  if (view_)
    view_->Bind(this);
}

HWDataCollectionScreen::~HWDataCollectionScreen() {
  if (view_)
    view_->Unbind();
}

void HWDataCollectionScreen::SetHWDataUsageEnabled(bool enabled) {
  hw_data_usage_enabled_ = enabled;
}

bool HWDataCollectionScreen::IsHWDataUsageEnabled() const {
  return hw_data_usage_enabled_;
}

void HWDataCollectionScreen::OnViewDestroyed(HWDataCollectionView* view) {
  if (view_ == view)
    view_ = nullptr;
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
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    auto* user_manager = user_manager::UserManager::Get();
    is_owner = !connector->IsDeviceEnterpriseManaged() &&
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
  if (view_)
    view_->Show();
}

void HWDataCollectionScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void HWDataCollectionScreen::OnUserActionDeprecated(
    const std::string& action_id) {
  if (action_id == kUserActionAcceptButtonClicked) {
    HWDataUsageController::Get()->Set(ProfileManager::GetActiveUserProfile(),
                                      base::Value(hw_data_usage_enabled_));
    exit_callback_.Run(hw_data_usage_enabled_
                           ? Result::ACCEPTED_WITH_HW_DATA_USAGE
                           : Result::ACCEPTED_WITHOUT_HW_DATA_USAGE);
  } else if (action_id == kUserActionShowLearnMore) {
    ShowHWDataUsageLearnMore();
  } else if (action_id == kUserActionUnselectHWDataUsage) {
    SetHWDataUsageEnabled(false /* enabled */);
  } else if (action_id == kUserActionSelectHWDataUsage) {
    SetHWDataUsageEnabled(true /* enabled */);
  } else {
    BaseScreen::OnUserActionDeprecated(action_id);
  }
}

void HWDataCollectionScreen::ShowHWDataUsageLearnMore() {
  if (view_)
    view_->ShowHWDataUsageLearnMore();
}

}  // namespace ash
