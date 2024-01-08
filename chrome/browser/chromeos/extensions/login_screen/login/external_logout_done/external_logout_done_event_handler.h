// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A KeyedService which broadcasts external logout done events in Ash.
class ExternalLogoutDoneEventHandler
    : public crosapi::LoginAsh::ExternalLogoutDoneObserver,
      public KeyedService {
 public:
  explicit ExternalLogoutDoneEventHandler(
      content::BrowserContext* browser_context);

  ExternalLogoutDoneEventHandler(const ExternalLogoutDoneEventHandler&) =
      delete;
  ExternalLogoutDoneEventHandler& operator=(
      const ExternalLogoutDoneEventHandler&) = delete;

  ~ExternalLogoutDoneEventHandler() override;

  // crosapi::LoginAsh::ExternalLogoutDoneObserver:
  void OnExternalLogoutDone() override;

  void SetEventRouterForTesting(EventRouter* event_router);

 private:
  raw_ptr<EventRouter, DanglingUntriaged> event_router_;
  base::ScopedObservation<crosapi::LoginAsh,
                          crosapi::LoginAsh::ExternalLogoutDoneObserver>
      scoped_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_H_
