// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/mojom/kiosk.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// TODO(hidehiko): Consider to migrate this class into ArcKioskAppService.
class ArcKioskBridge : public KeyedService,
                       public mojom::KioskHost {
 public:
  // Received IPCs are passed to this delegate.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnMaintenanceSessionCreated() = 0;
    virtual void OnMaintenanceSessionFinished() = 0;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcKioskBridge* GetForBrowserContext(content::BrowserContext* context);

  // Returns a created instance for testing.
  static std::unique_ptr<ArcKioskBridge> CreateForTesting(
      ArcBridgeService* bridge_service,
      Delegate* delegate);

  ArcKioskBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcKioskBridge() override;

  // mojom::KioskHost overrides.
  void OnMaintenanceSessionCreated(int32_t session_id) override;
  void OnMaintenanceSessionFinished(int32_t session_id, bool success) override;

 private:
  // |delegate| should be alive while the ArcKioskBridge instance is alive.
  ArcKioskBridge(ArcBridgeService* bridge_service, Delegate* delegate);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  Delegate* const delegate_;

  // Tracks current maintenance session id.
  int32_t session_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_KIOSK_ARC_KIOSK_BRIDGE_H_
