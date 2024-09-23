// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_ITEM_H_

#include "ash/components/arc/mojom/error_notification.mojom.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/error_notification/arc_error_notification_bridge.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace arc {

// Implements ErrorNotificationItem interface, which is provided to ARC to
// perform actions such as close the notification.
class ArcErrorNotificationItem
    : public mojom::ErrorNotificationItem {  // Inherit from mojom interface
 public:
  static mojo::PendingRemote<mojom::ErrorNotificationItem> Create(
      base::WeakPtr<ArcErrorNotificationBridge> bridge,
      const std::string& notification_id);

  // mojom::ErrorNotificationHost implementation
  void CloseErrorNotification() override;

  ArcErrorNotificationItem(base::WeakPtr<ArcErrorNotificationBridge> bridge,
                           const std::string& notification_id);
  ArcErrorNotificationItem(const ArcErrorNotificationItem&) = delete;
  ArcErrorNotificationItem& operator=(const ArcErrorNotificationItem&) = delete;
  ~ArcErrorNotificationItem() override;

 private:
  ArcErrorNotificationItem();

  void Bind(mojo::PendingRemote<arc::mojom::ErrorNotificationItem>* remote);

  void Close();

  base::WeakPtr<ArcErrorNotificationBridge> bridge_;

  const std::string notification_id_;

  mojo::Receiver<arc::mojom::ErrorNotificationItem> receiver_{this};

  base::WeakPtrFactory<ArcErrorNotificationItem> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ERROR_NOTIFICATION_ARC_ERROR_NOTIFICATION_ITEM_H_
