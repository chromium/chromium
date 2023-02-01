// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"

#include "content/public/browser/browser_context.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace {

bool IsLockedToExtension(const GURL& lock_url) {
  return lock_url.SchemeIs(extensions::kExtensionScheme);
}

const extensions::Extension* GetExtensionByLockUrl(
    content::BrowserContext* browser_context,
    const GURL& lock_url) {
  return extensions::ExtensionRegistry::Get(browser_context)
      ->enabled_extensions()
      .GetExtensionOrAppByURL(lock_url);
}

}  // namespace

bool ChromeDirectSocketsDelegate::ValidateAddressAndPort(
    content::BrowserContext* browser_context,
    const GURL& lock_url,
    const std::string& address,
    uint16_t port,
    blink::mojom::DirectSocketProtocolType protocol) const {
  if (!IsLockedToExtension(lock_url)) {
    return true;
  }

  // If we're running an extension, follow the chrome.sockets.* permission
  // model.
  auto* extension = GetExtensionByLockUrl(browser_context, lock_url);
  DCHECK(extension);

  switch (protocol) {
    case blink::mojom::DirectSocketProtocolType::kTcp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::TCP_CONNECT, address,
                       port});
    case blink::mojom::DirectSocketProtocolType::kUdp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::UDP_SEND_TO, address,
                       port});
    case blink::mojom::DirectSocketProtocolType::kUdpServer:
      // For kUdpServer we check both UDP_BIND for the given |address| and
      // |port| as well as ensure that UDP_SEND_TO allows routing packets
      // anywhere. '*' is the wildcard address, 0 is the wildcard port.
      return extensions::SocketsManifestData::CheckRequest(
                 extension,
                 /*request=*/{content::SocketPermissionRequest::UDP_BIND,
                              address, port}) &&
             extensions::SocketsManifestData::CheckRequest(
                 extension,
                 /*request=*/{content::SocketPermissionRequest::UDP_SEND_TO,
                              /*host=*/"*", /*port=*/0});
  }
}
