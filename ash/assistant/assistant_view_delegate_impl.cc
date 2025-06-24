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

void AssistantViewDelegateImpl::OnLauncherSearchChipPressed(
    std::u16string_view query) {
  for (auto& observer : view_delegate_observers_) {
    observer.OnLauncherSearchChipPressed(query);
  }
}

}  // namespace ash
