// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/capture_mode_tour/capture_mode_tour_controller.h"
#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/user_education/user_education_feature_controller.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller.h"
#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by `Shell`.
UserEducationController* g_instance = nullptr;

}  // namespace

// UserEducationController -----------------------------------------------------

UserEducationController::UserEducationController(
    std::unique_ptr<UserEducationDelegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  if (features::IsCaptureModeTourEnabled()) {
    feature_controllers_.emplace(std::make_unique<CaptureModeTourController>());
  }

  if (features::IsHoldingSpaceTourEnabled()) {
    feature_controllers_.emplace(
        std::make_unique<HoldingSpaceTourController>());
  }

  if (features::IsWelcomeTourEnabled()) {
    feature_controllers_.emplace(std::make_unique<WelcomeTourController>());
  }

  auto* session_controller = Shell::Get()->session_controller();
  session_observation_.Observe(session_controller);
  for (const auto& user_session : session_controller->GetUserSessions()) {
    OnUserSessionAdded(user_education_util::GetAccountId(user_session.get()));
  }
}

UserEducationController::~UserEducationController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationController* UserEducationController::Get() {
  return g_instance;
}

// static
void UserEducationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  welcome_tour_prefs::RegisterProfilePrefs(registry);
}

absl::optional<ui::ElementIdentifier>
UserEducationController::GetElementIdentifierForAppId(
    const std::string& app_id) const {
  return delegate_->GetElementIdentifierForAppId(app_id);
}

const absl::optional<bool>& UserEducationController::IsNewUser(
    UserEducationPrivateApiKey) const {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  return delegate_->IsNewUser(account_id);
}

void UserEducationController::LaunchSystemWebAppAsync(
    UserEducationPrivateApiKey,
    SystemWebAppType system_web_app_type,
    int64_t display_id) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  delegate_->LaunchSystemWebAppAsync(account_id, system_web_app_type,
                                     display_id);
}

void UserEducationController::OnChromeTerminating() {
  session_observation_.Reset();
}

void UserEducationController::OnUserSessionAdded(const AccountId& account_id) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  if (!user_education_util::IsPrimaryAccountId(account_id)) {
    return;
  }

  session_observation_.Reset();

  // User education services are not currently supported for irregular profiles.
  if (user_education_util::GetUserType(account_id) !=
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  // TODO(http://b/289292432): Move to `UserEducationTutorialController`.
  // Register all tutorials with user education services in the browser.
  for (auto& feature_controller : feature_controllers_) {
    for (auto& [tutorial_id, tutorial_description] :
         feature_controller->GetTutorialDescriptions()) {
      delegate_->RegisterTutorial(account_id, tutorial_id,
                                  std::move(tutorial_description));
    }
  }
}

}  // namespace ash
