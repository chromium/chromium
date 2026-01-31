// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/guest_view/chrome_guest_view.h"

#include "chrome/browser/android/guest_view/chrome_guest_view_manager_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace android {

// static
void ChromeGuestView::Create(
    const content::GlobalRenderFrameHostId& frame_id,
    mojo::PendingAssociatedReceiver<guest_view::mojom::GuestViewHost>
        receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new ChromeGuestView(frame_id)), std::move(receiver));
}

ChromeGuestView::~ChromeGuestView() = default;

ChromeGuestView::ChromeGuestView(
    const content::GlobalRenderFrameHostId& frame_id)
    : GuestViewMessageHandler(frame_id) {}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ChromeGuestView::CreateGuestViewManagerDelegate() const {
  return std::make_unique<ChromeGuestViewManagerDelegate>();
}

}  // namespace android
