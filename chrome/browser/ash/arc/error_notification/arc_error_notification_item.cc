// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/error_notification/arc_error_notification_item.h"

namespace arc {

// static
mojo::PendingRemote<mojom::ErrorNotificationItem>
ArcErrorNotificationItem::Create() {
  // This object will be deleted when the mojo connection is closed.
  auto* item = new ArcErrorNotificationItem();
  mojo::PendingRemote<arc::mojom::ErrorNotificationItem> remote;
  item->Bind(&remote);
  return remote;
}

void ArcErrorNotificationItem::CloseErrorNotification() {
  delete this;
  // TODO(b/332459217): Add implementation.
}

ArcErrorNotificationItem::ArcErrorNotificationItem() {}

ArcErrorNotificationItem::~ArcErrorNotificationItem() {}

void ArcErrorNotificationItem::Bind(
    mojo::PendingRemote<arc::mojom::ErrorNotificationItem>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(base::BindOnce(
      &ArcErrorNotificationItem::Close, weak_ptr_factory_.GetWeakPtr()));
}

// Deletes this object when the mojo connection is closed.
void ArcErrorNotificationItem::Close() {
  delete this;
}

}  // namespace arc
