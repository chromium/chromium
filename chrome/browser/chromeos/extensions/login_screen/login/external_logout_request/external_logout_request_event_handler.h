// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A KeyedService which broadcasts external logout request events in Ash and
// Lacros.
class ExternalLogoutRequestEventHandler
    : public crosapi::mojom::ExternalLogoutRequestObserver,
      public KeyedService {
 public:
  explicit ExternalLogoutRequestEventHandler(
      content::BrowserContext* browser_context);

  ExternalLogoutRequestEventHandler(const ExternalLogoutRequestEventHandler&) =
      delete;
  ExternalLogoutRequestEventHandler& operator=(
      const ExternalLogoutRequestEventHandler&) = delete;

  ~ExternalLogoutRequestEventHandler() override;

  // crosapi::mojom::ExternalLogoutRequestObserver:
  void OnRequestExternalLogout() override;

  void SetEventRouterForTesting(EventRouter* event_router);

 private:
  raw_ptr<EventRouter, DanglingUntriaged> event_router_;
  mojo::Receiver<crosapi::mojom::ExternalLogoutRequestObserver> receiver_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_H_
