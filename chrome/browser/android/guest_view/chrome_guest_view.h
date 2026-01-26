// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_H_
#define CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_H_

#include "components/guest_view/browser/guest_view_message_handler.h"

namespace android {

// A GuestViewMessageHandler for Chrome on Android that knows how to create a
// ChromeGuestViewManagerDelegate.
class ChromeGuestView : public guest_view::GuestViewMessageHandler {
 public:
  static void Create(
      const content::GlobalRenderFrameHostId& frame_id,
      mojo::PendingAssociatedReceiver<guest_view::mojom::GuestViewHost>
          receiver);

  ChromeGuestView(const ChromeGuestView&) = delete;
  ChromeGuestView& operator=(const ChromeGuestView&) = delete;

  ~ChromeGuestView() override;

 private:
  explicit ChromeGuestView(const content::GlobalRenderFrameHostId& frame_id);

  // guest_view::GuestViewMessageHandler:
  std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate() const override;
};
}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_H_
