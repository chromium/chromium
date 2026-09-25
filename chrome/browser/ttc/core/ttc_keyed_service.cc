// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_keyed_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/app/public/make_conversation.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"

namespace ttc {

// static
TtcKeyedService* TtcKeyedService::Get(content::BrowserContext* context) {
  return TtcKeyedServiceFactory::GetTtcKeyedService(context);
}

TtcKeyedService::TtcKeyedService(Profile* profile,
                                 ConversationFactory conversation_factory)
    : profile_(profile),
      conversation_factory_(std::move(conversation_factory)),
      actor_ui_state_manager_(std::make_unique<TtcActorUiStateManager>()) {}

TtcKeyedService::~TtcKeyedService() = default;

void TtcKeyedService::Shutdown() {
  EndSession();
}

bool TtcKeyedService::IsEnabled() const {
  // TODO(b/555805204): Integrate Enterprise policy. For now return true as
  // the KeyedService exists.
  return true;
}

ServiceState TtcKeyedService::GetState() const {
  if (!IsEnabled()) {
    return ServiceState::kProfileIneligible;
  }
  return is_session_active() ? ServiceState::kSessionActive
                             : ServiceState::kSessionInactive;
}

void TtcKeyedService::StartSession() {
  CHECK(IsEnabled());
  CHECK(!session_controller_);
  session_controller_ = std::make_unique<SessionControllerImpl>(*this);
  state_changed_callbacks_.Notify(GetState());
}

void TtcKeyedService::EndSession() {
  if (!is_session_active()) {
    return;
  }
  session_controller_.reset();
  state_changed_callbacks_.Notify(GetState());
}

base::CallbackListSubscription TtcKeyedService::RegisterStateChangedCallback(
    base::RepeatingCallback<void(ServiceState)> callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

std::unique_ptr<Conversation> TtcKeyedService::MakeConversation(
    base::PassKey<SessionControllerImpl>,
    SessionController& session_controller) {
  std::unique_ptr<Conversation> conversation =
      conversation_factory_ ? conversation_factory_.Run(session_controller)
                            : MakeConversationImpl(session_controller);
  CHECK(conversation);
  return conversation;
}

TtcActorUiStateManager& TtcKeyedService::actor_ui_state_manager() {
  return CHECK_DEREF(actor_ui_state_manager_.get());
}

base::WeakPtr<TtcKeyedService> TtcKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ttc
