// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"
#include "chrome/common/extensions/api/login_state.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#endif

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
    : browser_context_(browser_context),
      event_router_(EventRouter::Get(browser_context)) {
  crosapi::mojom::LoginState* login_state_api = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CrosapiManager may not be initialized in tests.
  if (crosapi::CrosapiManager::IsInitialized()) {
    login_state_api = GetLoginStateApi();
  }
#else
  if (chromeos::LacrosService::Get()
          ->IsAvailable<crosapi::mojom::LoginState>()) {
    login_state_api = GetLoginStateApi();
  }
#endif
  if (login_state_api) {
    login_state_api->AddObserver(receiver_.BindNewPipeAndPassRemote());
  }
}

SessionStateChangedEventDispatcher::~SessionStateChangedEventDispatcher() =
    default;

void SessionStateChangedEventDispatcher::Shutdown() {}

void SessionStateChangedEventDispatcher::OnSessionStateChanged(
    crosapi::mojom::SessionState state) {
  api::login_state::SessionState new_state = ToApiEnum(state);

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
