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

bool HWDataCollectionScreen::MaybeSkip(WizardContext* context) {
  bool is_owner = false;
  if (features::IsOobeConsolidatedConsentEnabled()) {
    is_owner = context->is_owner_flow.value_or(false);
  } else {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    // Taking device ownership can take some time, so we can't rely on it here.
    // Check that the user is first and not managed instead.
    is_owner = !connector->IsDeviceEnterpriseManaged();
    auto* user_manager = user_manager::UserManager::Get();
    if (user_manager->GetUsers().size() > 1) {
      is_owner = is_owner && user_manager->IsCurrentUserOwner();
    }
  }
  if (!switches::IsRevenBranding() || !is_owner || !context->is_branded_build) {
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
