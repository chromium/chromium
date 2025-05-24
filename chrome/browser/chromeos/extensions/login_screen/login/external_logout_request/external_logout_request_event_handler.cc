// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/common/extensions/api/login.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ExternalLogoutRequestEventHandler::ExternalLogoutRequestEventHandler(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)) {
  crosapi::mojom::Login* login_api = nullptr;
  // CrosapiManager may not be initialized in tests.
  if (crosapi::CrosapiManager::IsInitialized()) {
    login_api = crosapi::CrosapiManager::Get()->crosapi_ash()->login_ash();
  }
  if (login_api) {
    login_api->AddExternalLogoutRequestObserver(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

ExternalLogoutRequestEventHandler::~ExternalLogoutRequestEventHandler() =
    default;

void ExternalLogoutRequestEventHandler::OnRequestExternalLogout() {
  std::unique_ptr<Event> event =
      std::make_unique<Event>(events::LOGIN_ON_REQUEST_EXTERNAL_LOGOUT,
                              api::login::OnRequestExternalLogout::kEventName,
                              api::login::OnRequestExternalLogout::Create());

  event_router_->BroadcastEvent(std::move(event));
}

void ExternalLogoutRequestEventHandler::SetEventRouterForTesting(
    EventRouter* event_router) {
  event_router_ = event_router;
}

}  // namespace extensions
