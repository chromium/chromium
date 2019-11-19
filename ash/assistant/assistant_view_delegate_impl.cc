// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_view_delegate_impl.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

AssistantViewDelegateImpl::AssistantViewDelegateImpl(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AssistantViewDelegateImpl::~AssistantViewDelegateImpl() = default;

const AssistantInteractionModel*
AssistantViewDelegateImpl::GetInteractionModel() const {
  return assistant_controller_->interaction_controller()->model();
}

const AssistantNotificationModel*
AssistantViewDelegateImpl::GetNotificationModel() const {
  return assistant_controller_->notification_controller()->model();
}

const AssistantSuggestionsModel*
AssistantViewDelegateImpl::GetSuggestionsModel() const {
  return assistant_controller_->suggestions_controller()->model();
}

const AssistantUiModel* AssistantViewDelegateImpl::GetUiModel() const {
  return assistant_controller_->ui_controller()->model();
}

void AssistantViewDelegateImpl::AddObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}

void AssistantViewDelegateImpl::RemoveObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

void AssistantViewDelegateImpl::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::AddNotificationModelObserver(
    AssistantNotificationModelObserver* observer) {
  assistant_controller_->notification_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveNotificationModelObserver(
    AssistantNotificationModelObserver* observer) {
  assistant_controller_->notification_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::AddSuggestionsModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  assistant_controller_->suggestions_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveSuggestionsModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  assistant_controller_->suggestions_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::AddUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->RemoveModelObserver(observer);
}

CaptionBarDelegate* AssistantViewDelegateImpl::GetCaptionBarDelegate() {
  return assistant_controller_->ui_controller();
}

void AssistantViewDelegateImpl::DownloadImage(
    const GURL& url,
    AssistantImageDownloader::DownloadCallback callback) {
  assistant_controller_->DownloadImage(url, std::move(callback));
}

::wm::CursorManager* AssistantViewDelegateImpl::GetCursorManager() {
  return Shell::Get()->cursor_manager();
}

void AssistantViewDelegateImpl::GetNavigableContentsFactoryForView(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  assistant_controller_->GetNavigableContentsFactory(std::move(receiver));
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return Shell::Get()->GetRootWindowForDisplayId(display_id);
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForNewWindows() {
  return Shell::Get()->GetRootWindowForNewWindows();
}

bool AssistantViewDelegateImpl::IsTabletMode() const {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
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

void AssistantViewDelegateImpl::OnMiniViewPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnMiniViewPressed();
}

void AssistantViewDelegateImpl::OnNotificationButtonPressed(
    const std::string& notification_id,
    int notification_button_index) {
  assistant_controller_->notification_controller()->OnNotificationClicked(
      notification_id, notification_button_index, /*reply=*/base::nullopt);
}

void AssistantViewDelegateImpl::OnOptInButtonPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnOptInButtonPressed();
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsCloseButtonPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsCloseButtonPressed();
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsViewHoverChanged(
    bool is_hovering) {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsViewHoverChanged(is_hovering);
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsViewPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsViewPressed();
}

void AssistantViewDelegateImpl::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnSuggestionChipPressed(suggestion);
}

void AssistantViewDelegateImpl::OpenUrlFromView(const GURL& url) {
  assistant_controller_->OpenUrl(url);
}

void AssistantViewDelegateImpl::NotifyDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnDeepLinkReceived(type, params);
}

}  // namespace ash
