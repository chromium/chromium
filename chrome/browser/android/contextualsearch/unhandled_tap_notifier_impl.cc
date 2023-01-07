// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"

#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace contextual_search {

UnhandledTapNotifierImpl::UnhandledTapNotifierImpl(
    UnhandledTapCallback callback)
    : unhandled_tap_callback_(std::move(callback)) {}

UnhandledTapNotifierImpl::~UnhandledTapNotifierImpl() {}

void UnhandledTapNotifierImpl::ShowUnhandledTapUIIfNeeded(
    blink::mojom::UnhandledTapInfoPtr unhandled_tap_info) {
  float x_px = unhandled_tap_info->tapped_position_in_viewport.x();
  float y_px = unhandled_tap_info->tapped_position_in_viewport.y();

  // Call back through the callback if possible.  (The callback uses a weakptr
  // that might make this a NOP).
  unhandled_tap_callback_.Run(x_px, y_px);
}

// static
void CreateUnhandledTapNotifierImpl(
    UnhandledTapCallback callback,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<UnhandledTapNotifierImpl>(std::move(callback)),
      std::move(receiver));
}

}  // namespace contextual_search
