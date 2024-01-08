// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_

#include <memory>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcSettingsServiceImpl;

class ArcSettingsService
    : public KeyedService,
      public ConnectionObserver<mojom::IntentHelperInstance>,
      public ArcSessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSettingsService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcSettingsService(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ArcSettingsService(const ArcSettingsService&) = delete;
  ArcSettingsService& operator=(const ArcSettingsService&) = delete;
  ~ArcSettingsService() override;

  // ConnectionObserver<mojom::IntentHelperInstance>
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcInitialStart() override;

  static void EnsureFactoryBuilt();

 private:
  void SetInitialSettingsPending(bool pending);
  bool IsInitialSettingsPending() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<ArcSettingsServiceImpl> impl_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
