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
  content::SocketPermissionRequest param(
      protocol == blink::mojom::DirectSocketProtocolType::kTcp
          ? content::SocketPermissionRequest::TCP_CONNECT
          : content::SocketPermissionRequest::UDP_SEND_TO,
      address, port);
  return extensions::SocketsManifestData::CheckRequest(extension, param);
}
