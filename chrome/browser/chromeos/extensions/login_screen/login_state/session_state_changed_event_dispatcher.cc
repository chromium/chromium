// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"
#include "chrome/common/extensions/api/login_state.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

namespace extensions {

BrowserContextKeyedAPIFactory<SessionStateChangedEventDispatcher>*
SessionStateChangedEventDispatcher::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<SessionStateChangedEventDispatcher>>
      instance;
  return instance.get();
}

SessionStateChangedEventDispatcher::SessionStateChangedEventDispatcher(
    content::BrowserContext* browser_context)
    : session_manager_observer_(this),
      browser_context_(browser_context),
      event_router_(EventRouter::Get(browser_context)),
      session_state_(api::login_state::SESSION_STATE_UNKNOWN) {
  session_manager_observer_.Add(session_manager::SessionManager::Get());
}

SessionStateChangedEventDispatcher::~SessionStateChangedEventDispatcher() =
    default;

void SessionStateChangedEventDispatcher::Shutdown() {}

void SessionStateChangedEventDispatcher::OnSessionStateChanged() {
  api::login_state::SessionState new_state = SessionStateToApiEnum(
      session_manager::SessionManager::Get()->session_state());

  // |session_manager::SessionState| changed but the mapped
  // |api::login_state::SessionState| did not.
  if (session_state_ == new_state)
    return;

  session_state_ = new_state;

  std::unique_ptr<Event> event = std::make_unique<Event>(
      events::LOGIN_STATE_ON_SESSION_STATE_CHANGED,
      api::login_state::OnSessionStateChanged::kEventName,
      api::login_state::OnSessionStateChanged::Create(new_state));

  event_router_->BroadcastEvent(std::move(event));
}

void SessionStateChangedEventDispatcher::SetEventRouterForTesting(
    EventRouter* event_router) {
  event_router_ = event_router;
}

}  // namespace extensions
