// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_web_ui_controller.h"

#include "ash/assistant/ui/assistant_web_container_view.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
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
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget->GetNativeWindow(),
            {ui::EventType::kKeyPressed})) {}

  AssistantWebContainerEventObserver(
      const AssistantWebContainerEventObserver&) = delete;
  AssistantWebContainerEventObserver& operator=(
      const AssistantWebContainerEventObserver&) = delete;

  ~AssistantWebContainerEventObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    DCHECK(event.type() == ui::EventType::kKeyPressed);

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
  raw_ptr<AssistantWebUiController> owner_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

// -----------------------------------------------------------------------------
// AssistantWebUiController:

AssistantWebUiController::AssistantWebUiController() {
  assistant_controller_observation_.Observe(AssistantController::Get());
}

AssistantWebUiController::~AssistantWebUiController() {
  CloseUi();
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void AssistantWebUiController::OnWidgetDestroying(views::Widget* widget) {
  ResetWebContainerView();
}

void AssistantWebUiController::OnAssistantControllerConstructed() {
  AssistantState::Get()->AddObserver(this);
}

void AssistantWebUiController::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantWebUiController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (assistant::util::IsWebDeepLinkType(type, params))
    ShowUi(assistant::util::GetWebUrl(type, params).value());
}

void AssistantWebUiController::OnAssistantSettingsEnabled(bool enabled) {
  if (!enabled)
    CloseUi();
}

void AssistantWebUiController::ShowUi(const GURL& url) {
  if (!web_container_view_)
    CreateWebContainerView();

  web_container_view_->GetWidget()->Show();
  web_container_view_->OpenUrl(url);
}

void AssistantWebUiController::CloseUi() {
  if (!web_container_view_)
    return;

  web_container_view_->GetWidget()->CloseNow();
  DCHECK_EQ(nullptr, web_container_view_);
}

void AssistantWebUiController::OnBackButtonPressed() {
  DCHECK(web_container_view_);
  web_container_view_->GoBack();
}

AssistantWebContainerView* AssistantWebUiController::GetViewForTest() {
  return web_container_view_;
}

void AssistantWebUiController::CreateWebContainerView() {
  DCHECK(!web_container_view_);

  web_container_view_ = new AssistantWebContainerView(&view_delegate_);
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
