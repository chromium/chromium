// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/autoclick_client_impl.h"

#include "ash/accessibility/accessibility_controller.h"
#include "base/debug/stack_trace.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/public/mojom/autoclick.mojom.h"

namespace ash {

AutoclickClientImpl::AutoclickClientImpl() = default;

AutoclickClientImpl::~AutoclickClientImpl() = default;

void AutoclickClientImpl::Bind(mojo::PendingReceiver<ax::mojom::AutoclickClient>
                                   autoclick_client_receiver) {
  autoclick_client_receivers_.Add(this, std::move(autoclick_client_receiver));
}

void AutoclickClientImpl::HandleScrollableBoundsForPointFound(
    const gfx::Rect& bounds) {
  AccessibilityController::Get()->HandleAutoclickScrollableBoundsFound(bounds);
}

void AutoclickClientImpl::BindAutoclick(BindAutoclickCallback callback) {
  mojo::Remote<ax::mojom::Autoclick> remote;
  std::move(callback).Run(remote.BindNewPipeAndPassReceiver());
  autoclick_remotes_.Add(std::move(remote));
}

void AutoclickClientImpl::RequestScrollableBoundsForPoint(
    const gfx::Point& point) {
  for (auto& remote : autoclick_remotes_) {
    remote->RequestScrollableBoundsForPoint(point);
  }
}

}  // namespace ash
