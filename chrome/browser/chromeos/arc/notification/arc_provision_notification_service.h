// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Watches for ARC provisioning status and displays a notification during
// provision when ARC opt-in flow happens silently due to configured policies.
class ArcProvisionNotificationService : public KeyedService,
                                        public ArcSessionManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcProvisionNotificationService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcProvisionNotificationService(content::BrowserContext* context,
                                  ArcBridgeService* bridge_service);
  ~ArcProvisionNotificationService() override;

 private:
  // Shows/hides the notification.
  void ShowNotification();
  void HideNotification();

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcStarted() override;
  void OnArcOptInManagementCheckStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcErrorShowRequested(ArcSupportHost::Error error) override;

  content::BrowserContext* const context_;

  DISALLOW_COPY_AND_ASSIGN(ArcProvisionNotificationService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
