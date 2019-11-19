// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_web_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_web_container_view.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/services/assistant/public/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"

namespace ash {

// -----------------------------------------------------------------------------
// AssistantWebContainerEventObserver:

class AssistantWebContainerEventObserver : public ui::EventObserver {
 public:
  AssistantWebContainerEventObserver(AssistantWebUiController* owner,
                                     views::Widget* widget)
      : owner_(owner),
        widget_(widget),
        event_monitor_(
            views::EventMonitor::CreateWindowMonitor(this,
                                                     widget->GetNativeWindow(),
                                                     {ui::ET_KEY_PRESSED})) {}

  ~AssistantWebContainerEventObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    DCHECK(event.type() == ui::ET_KEY_PRESSED);

    const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
    switch (key_event.key_code()) {
      case ui::VKEY_BROWSER_BACK:
        owner_->OnBackButtonPressed();
        break;
      case ui::VKEY_W:
        if (!key_event.IsControlDown())
          break;

        event_monitor_.reset();
        widget_->Close();
        break;
      default:
        // No action necessary.
        break;
    }
  }

 private:
  AssistantWebUiController* owner_ = nullptr;
  views::Widget* widget_ = nullptr;

  std::unique_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerEventObserver);
};

// -----------------------------------------------------------------------------
// AssistantWebUiController:

AssistantWebUiController::AssistantWebUiController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  DCHECK(chromeos::assistant::features::IsAssistantWebContainerEnabled());
  assistant_controller_->AddObserver(this);
}

AssistantWebUiController::~AssistantWebUiController() {
  assistant_controller_->RemoveObserver(this);
}

void AssistantWebUiController::OnWidgetDestroying(views::Widget* widget) {
  ResetWebContainerView();
}

void AssistantWebUiController::OnAssistantControllerDestroying() {
  if (!web_container_view_)
    return;

  // The view should not outlive the controller.
  web_container_view_->GetWidget()->CloseNow();
  DCHECK_EQ(nullptr, web_container_view_);
}

void AssistantWebUiController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type, params))
    return;

  ShowUi();
}

void AssistantWebUiController::ShowUi() {
  if (!web_container_view_)
    CreateWebContainerView();

  web_container_view_->GetWidget()->Show();
}

void AssistantWebUiController::OnBackButtonPressed() {
  DCHECK(web_container_view_);
  web_container_view_->OnBackButtonPressed();
}

AssistantWebContainerView* AssistantWebUiController::GetViewForTest() {
  return web_container_view_;
}

void AssistantWebUiController::CreateWebContainerView() {
  DCHECK(!web_container_view_);

  web_container_view_ = new AssistantWebContainerView(
      assistant_controller_->view_delegate(), &view_delegate_);
  auto* widget = web_container_view_->GetWidget();
  widget->AddObserver(this);
  event_observer_ =
      std::make_unique<AssistantWebContainerEventObserver>(this, widget);

  // Associate the window for Assistant Web UI with the active user in order to
  // not leak across user sessions.
  auto* window_manager = MultiUserWindowManagerImpl::Get();
  if (!window_manager)
    return;

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  if (!active_user_session)
    return;

  auto* native_window = widget->GetNativeWindow();
  native_window->SetProperty(aura::client::kCreatedByUserGesture, true);
  window_manager->SetWindowOwner(native_window,
                                 active_user_session->user_info.account_id);
}

void AssistantWebUiController::ResetWebContainerView() {
  DCHECK(web_container_view_);

  event_observer_.reset();
  web_container_view_->GetWidget()->RemoveObserver(this);
  web_container_view_ = nullptr;
}

}  // namespace ash
