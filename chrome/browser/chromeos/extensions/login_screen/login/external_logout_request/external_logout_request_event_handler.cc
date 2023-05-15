// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler.h"

#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/login.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#endif

namespace extensions {

ExternalLogoutRequestEventHandler::ExternalLogoutRequestEventHandler(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)) {
  crosapi::mojom::Login* login_api = nullptr;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::Login>()) {
    int interface_version =
        lacros_service->GetInterfaceVersion<crosapi::mojom::Login>();
    if (interface_version <
        int(crosapi::mojom::Login::
                kAddExternalLogoutRequestObserverMinVersion)) {
      return;
    }
    login_api = lacros_service->GetRemote<crosapi::mojom::Login>().get();
  }
#else
  // CrosapiManager may not be initialized in tests.
  if (crosapi::CrosapiManager::IsInitialized()) {
    login_api = crosapi::CrosapiManager::Get()->crosapi_ash()->login_ash();
  }
#endif
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
