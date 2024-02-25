// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOCLICK_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOCLICK_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/autoclick.mojom.h"

namespace ash {

// The AutoclickClientImpl requests information on behalf of the
// Autoclick feature from the Accessibility Service and sends
// results to the Chrome OS Autoclick feature which is implemented
// in Ash.
class AutoclickClientImpl : public ax::mojom::AutoclickClient {
 public:
  AutoclickClientImpl();
  AutoclickClientImpl(const AutoclickClientImpl&) = delete;
  AutoclickClientImpl& operator=(const AutoclickClientImpl&) = delete;
  ~AutoclickClientImpl() override;

  void Bind(
      mojo::PendingReceiver<ax::mojom::AutoclickClient> autoclick_receiver);

  // ax::mojom::AutoclickClient:
  void HandleScrollableBoundsForPointFound(const gfx::Rect& bounds) override;
  void BindAutoclick(BindAutoclickCallback callback) override;

  void RequestScrollableBoundsForPoint(const gfx::Point& point);

 private:
  mojo::ReceiverSet<ax::mojom::AutoclickClient> autoclick_client_receivers_;
  mojo::RemoteSet<ax::mojom::Autoclick> autoclick_remotes_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOCLICK_CLIENT_IMPL_H_
