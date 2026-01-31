// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_CONTENT_BROWSER_CLIENT_GUEST_VIEW_PART_H_
#define CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_CONTENT_BROWSER_CLIENT_GUEST_VIEW_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace android {

// Exposes the GuestView interfaces to the renderer in Android.
class ChromeContentBrowserClientGuestViewPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientGuestViewPart();
  ChromeContentBrowserClientGuestViewPart(
      const ChromeContentBrowserClientGuestViewPart&) = delete;
  ChromeContentBrowserClientGuestViewPart& operator=(
      const ChromeContentBrowserClientGuestViewPart&) = delete;

  ~ChromeContentBrowserClientGuestViewPart() override;

  void ExposeInterfacesToRendererForRenderFrameHost(
      content::RenderFrameHost& frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_CONTENT_BROWSER_CLIENT_GUEST_VIEW_PART_H_
