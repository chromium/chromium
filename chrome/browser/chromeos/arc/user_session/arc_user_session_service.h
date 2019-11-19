// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_

#include "base/macros.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcUserSessionService
    : public KeyedService,
      public ConnectionObserver<mojom::IntentHelperInstance>,
      public session_manager::SessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcUserSessionService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcUserSessionService(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);
  ~ArcUserSessionService() override;

  // ConnectionObserver<mojom::IntentHelperInstance> override.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  ArcBridgeService* const arc_bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcUserSessionService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
