// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Watches for ARC boot errors and show notifications.
class ArcBootErrorNotification : public KeyedService,
                                 public ArcSessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcBootErrorNotification* GetForBrowserContext(
      content::BrowserContext* context);

  ArcBootErrorNotification(content::BrowserContext* context,
                           ArcBridgeService* bridge_service);

  ArcBootErrorNotification(const ArcBootErrorNotification&) = delete;
  ArcBootErrorNotification& operator=(const ArcBootErrorNotification&) = delete;

  ~ArcBootErrorNotification() override;

  // ArcSessionManagerObserver:
  void OnArcSessionStopped(ArcStopReason reason) override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<content::BrowserContext> context_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
