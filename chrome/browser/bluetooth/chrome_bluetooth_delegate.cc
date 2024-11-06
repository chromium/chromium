// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "components/permissions/bluetooth_delegate_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

ChromeBluetoothDelegate::ChromeBluetoothDelegate(std::unique_ptr<Client> client)
    : permissions::BluetoothDelegateImpl(std::move(client)) {}

bool ChromeBluetoothDelegate::MayUseBluetooth(content::RenderFrameHost* rfh) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // Because permission is scoped to profile, <webview> and <controlledframe>,
  // despite having isolated StoragePartition, will share bluetooth permission
  // with the rest of the profile. Therefore bluetooth is not allowed in these
  // contexts.
  if (extensions::WebViewGuest::FromRenderFrameHost(rfh)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

  // Disable any other non-default StoragePartition contexts, unless it has a
  // non-http/https scheme.
  if (rfh->GetStoragePartition() !=
      rfh->GetBrowserContext()->GetDefaultStoragePartition()) {
    return !rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}
