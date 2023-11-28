// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_view_delegate_impl.h"

#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller_impl.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/cpp/switches.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

using assistant::ui::kOnboardingMaxSessionsShown;

}  // namespace

AssistantViewDelegateImpl::AssistantViewDelegateImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AssistantViewDelegateImpl::~AssistantViewDelegateImpl() = default;

const AssistantNotificationModel*
AssistantViewDelegateImpl::GetNotificationModel() const {
  return assistant_controller_->notification_controller()->model();
}

void AssistantViewDelegateImpl::AddObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}

void AssistantViewDelegateImpl::RemoveObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

void AssistantViewDelegateImpl::DownloadImage(
    const GURL& url,
    ImageDownloader::DownloadCallback callback) {
  assistant_controller_->DownloadImage(url, std::move(callback));
}

::wm::CursorManager* AssistantViewDelegateImpl::GetCursorManager() {
  return Shell::Get()->cursor_manager();
}

std::string AssistantViewDelegateImpl::GetPrimaryUserGivenName() const {
  return Shell::Get()
      ->session_controller()
      ->GetPrimaryUserSession()
      ->user_info.given_name;
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return Shell::Get()->GetRootWindowForDisplayId(display_id);
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForNewWindows() {
  return Shell::Get()->GetRootWindowForNewWindows();
}

bool AssistantViewDelegateImpl::IsTabletMode() const {
  return display::Screen::GetScreen()->InTabletMode();
}

void AssistantViewDelegateImpl::OnDialogPlateButtonPressed(
    AssistantButtonId id) {
  for (auto& observer : view_delegate_observers_)
    observer.OnDialogPlateButtonPressed(id);
}

void AssistantViewDelegateImpl::OnDialogPlateContentsCommitted(
    const std::string& text) {
  for (auto& observer : view_delegate_observers_)
    observer.OnDialogPlateContentsCommitted(text);
}

void AssistantViewDelegateImpl::OnNotificationButtonPressed(
    const std::string& notification_id,
    int notification_button_index) {
  assistant_controller_->notification_controller()->OnNotificationClicked(
      notification_id, notification_button_index, /*reply=*/std::nullopt);
}

void AssistantViewDelegateImpl::OnOnboardingShown() {
  for (auto& observer : view_delegate_observers_)
    observer.OnOnboardingShown();
}

void AssistantViewDelegateImpl::OnOptInButtonPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnOptInButtonPressed();
}

void AssistantViewDelegateImpl::OnSuggestionPressed(
    const base::UnguessableToken& suggestion_id) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnSuggestionPressed(suggestion_id);
}

bool AssistantViewDelegateImpl::ShouldShowOnboarding() const {
  // UI developers need to be able to force the onboarding flow.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          assistant::switches::kForceAssistantOnboarding)) {
    return true;
  }

  if (!assistant::features::IsOnboardingEnabled()) {
    return false;
  }

  // Once a user has had an interaction with Assistant, we will no longer show
  // onboarding in that user session.
  auto* interaction_controller = AssistantInteractionController::Get();
  const bool has_had_interaction = interaction_controller->HasHadInteraction();
  if (has_had_interaction)
    return false;

  // If we do show onboarding to a user in a session, we will keep showing it
  // for that session until an Assistant interaction takes place.
  auto* ui_controller = AssistantUiController::Get();
  const bool has_shown_onboarding = ui_controller->HasShownOnboarding();
  if (has_shown_onboarding)
    return true;

  // Once a user has seen onboarding in any session, they will continue to see
  // onboarding each session until the maximum number of sessions is reached.
  const int number_of_sessions_where_onboarding_shown =
      ui_controller->GetNumberOfSessionsWhereOnboardingShown();
  if (number_of_sessions_where_onboarding_shown > 0) {
    return number_of_sessions_where_onboarding_shown <
           kOnboardingMaxSessionsShown;
  }

  // The feature will start to show only for new users which we define as users
  // who haven't had an interaction with Assistant in the last 28 days.
  return interaction_controller->GetTimeDeltaSinceLastInteraction() >=
         base::Days(28);
}

void AssistantViewDelegateImpl::OnLauncherSearchChipPressed(
    const std::u16string& query) {
  for (auto& observer : view_delegate_observers_) {
    observer.OnLauncherSearchChipPressed(query);
  }
}

}  // namespace ash
