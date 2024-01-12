// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
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
  static ArcUserSessionService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcUserSessionService(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);

  ArcUserSessionService(const ArcUserSessionService&) = delete;
  ArcUserSessionService& operator=(const ArcUserSessionService&) = delete;

  ~ArcUserSessionService() override;

  // ConnectionObserver<mojom::IntentHelperInstance> override.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService> arc_bridge_service_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_USER_SESSION_ARC_USER_SESSION_SERVICE_H_
