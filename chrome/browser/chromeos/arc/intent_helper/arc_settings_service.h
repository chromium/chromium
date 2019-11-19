// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/connection_observer.h"
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
      public ArcSessionManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSettingsService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcSettingsService(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcSettingsService() override;

  // ConnectionObserver<mojom::IntentHelperInstance>
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcInitialStart() override;

 private:
  void SetInitialSettingsPending(bool pending);
  bool IsInitialSettingsPending() const;

  Profile* const profile_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<ArcSettingsServiceImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
