// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_BRIDGE_H_

#include <string>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/error_notification.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace arc {

class ArcBridgeService;

// Manages ARC ErrorNotification mojo behavior. Creates Chrome notifications for
// ARC errors, relays user interactions (clicks, closes) back to ARC.
class ArcErrorNotificationBridge
    : public KeyedService,
      public mojom::ErrorNotificationHost {  // Inherit from mojom interface
 public:
  // Factory methods
  static ArcErrorNotificationBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcErrorNotificationBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static void EnsureFactoryBuilt();

  ArcErrorNotificationBridge(content::BrowserContext* context,
                             ArcBridgeService* bridge_service);
  ArcErrorNotificationBridge(const ArcErrorNotificationBridge&) = delete;
  ArcErrorNotificationBridge& operator=(const ArcErrorNotificationBridge&) =
      delete;
  ~ArcErrorNotificationBridge() override;

  // mojom::ErrorNotificationHost implementation
  void SendErrorDetails(
      mojom::ErrorDetailsPtr details,
      mojo::PendingRemote<mojom::ErrorNotificationActionHandler> action_handler,
      SendErrorDetailsCallback callback) override;

  void CloseNotification(const std::string& notification_id);

 private:
  std::string GenerateNotificationId(mojom::ErrorType type,
                                     const std::string& app_name);

  friend class ArcErrorNotificationBridgeFactory;

  raw_ptr<ArcBridgeService> arc_bridge_service_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ArcErrorNotificationBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_BRIDGE_H_
