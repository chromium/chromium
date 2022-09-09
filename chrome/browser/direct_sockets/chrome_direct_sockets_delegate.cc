// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace {

bool IsExtensionFrame(content::RenderFrameHost* frame) {
  return frame->GetLastCommittedURL().SchemeIs(extensions::kExtensionScheme);
}

const extensions::Extension* GetExtensionFromFrame(
    content::RenderFrameHost* frame) {
  return extensions::ExtensionRegistry::Get(frame->GetBrowserContext())
      ->enabled_extensions()
      .GetExtensionOrAppByURL(frame->GetLastCommittedURL());
}

}  // namespace

bool ChromeDirectSocketsDelegate::ValidateAddressAndPort(
    content::RenderFrameHost* frame,
    const std::string& address,
    uint16_t port,
    blink::mojom::DirectSocketProtocolType protocol) const {
  if (!IsExtensionFrame(frame)) {
    return true;
  }

  // If we're running an extension, follow the chrome.sockets.* permission
  // model.
  auto* extension = GetExtensionFromFrame(frame);
  DCHECK(extension);
  content::SocketPermissionRequest param(
      protocol == blink::mojom::DirectSocketProtocolType::kTcp
          ? content::SocketPermissionRequest::TCP_CONNECT
          : content::SocketPermissionRequest::UDP_SEND_TO,
      address, port);
  return extensions::SocketsManifestData::CheckRequest(extension, param);
}

bool ChromeDirectSocketsDelegate::ShouldSkipPostResolveChecks(
    content::RenderFrameHost* frame) const {
  // chrome.sockets.* doesn't have any restrictions on resolved addresses, so we
  // may skip these checks.
  return IsExtensionFrame(frame);
}
