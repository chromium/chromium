// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/guest_view/chrome_content_browser_client_guest_view_part.h"

#include "base/feature_list.h"
#include "chrome/browser/android/guest_view/chrome_guest_view.h"
#include "chrome/common/chrome_features.h"
#include "components/guest_view/common/guest_view.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace android {

ChromeContentBrowserClientGuestViewPart::
    ChromeContentBrowserClientGuestViewPart() = default;
ChromeContentBrowserClientGuestViewPart::
    ~ChromeContentBrowserClientGuestViewPart() = default;

void ChromeContentBrowserClientGuestViewPart::
    ExposeInterfacesToRendererForRenderFrameHost(
        content::RenderFrameHost& frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  if (base::FeatureList::IsEnabled(features::kGlic)) {
    associated_registry.AddInterface<guest_view::mojom::GuestViewHost>(
        base::BindRepeating(&ChromeGuestView::Create,
                            frame_host.GetGlobalId()));
  }
}

}  // namespace android
