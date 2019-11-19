// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Watches for ARC boot errors and show notifications.
class ArcBootErrorNotification : public KeyedService,
                                 public ArcSessionManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcBootErrorNotification* GetForBrowserContext(
      content::BrowserContext* context);

  ArcBootErrorNotification(content::BrowserContext* context,
                           ArcBridgeService* bridge_service);
  ~ArcBootErrorNotification() override;

  // ArcSessionManager::Observer:
  void OnArcSessionStopped(ArcStopReason reason) override;

 private:
  content::BrowserContext* const context_;

  DISALLOW_COPY_AND_ASSIGN(ArcBootErrorNotification);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
