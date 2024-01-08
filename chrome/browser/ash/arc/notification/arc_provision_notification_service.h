// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Watches for ARC provisioning status and displays a notification during
// provision when ARC opt-in flow happens silently due to configured policies.
class ArcProvisionNotificationService
    : public KeyedService,
      public ArcSessionManagerObserver,
      public session_manager::SessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcProvisionNotificationService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcProvisionNotificationService(content::BrowserContext* context,
                                  ArcBridgeService* bridge_service);

  ArcProvisionNotificationService(const ArcProvisionNotificationService&) =
      delete;
  ArcProvisionNotificationService& operator=(
      const ArcProvisionNotificationService&) = delete;

  ~ArcProvisionNotificationService() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  static void EnsureFactoryBuilt();

 private:
  // Shows/hides the notification.
  void MaybeShowNotification();
  void ShowNotification();
  void HideNotification();

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcStarted() override;
  void OnArcOptInManagementCheckStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcErrorShowRequested(ArcSupportHost::ErrorInfo error_info) override;

  const raw_ptr<content::BrowserContext> context_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  // Indicates whether notification should be shown right after session starts.
  bool show_on_session_starts_ = false;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
