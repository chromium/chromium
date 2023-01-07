// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/common/extensions/api/login.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ExternalLogoutDoneEventHandler::ExternalLogoutDoneEventHandler(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)) {
  // CrosapiManager may not be initialized in tests.
  if (crosapi::CrosapiManager::IsInitialized()) {
    scoped_observation_.Observe(
        crosapi::CrosapiManager::Get()->crosapi_ash()->login_ash());
  }
}

ExternalLogoutDoneEventHandler::~ExternalLogoutDoneEventHandler() = default;

void ExternalLogoutDoneEventHandler::OnExternalLogoutDone() {
  std::unique_ptr<Event> event =
      std::make_unique<Event>(events::LOGIN_ON_EXTERNAL_LOGOUT_DONE,
                              api::login::OnExternalLogoutDone::kEventName,
                              api::login::OnExternalLogoutDone::Create());

  event_router_->BroadcastEvent(std::move(event));
}

void ExternalLogoutDoneEventHandler::SetEventRouterForTesting(
    EventRouter* event_router) {
  event_router_ = event_router;
}

}  // namespace extensions
