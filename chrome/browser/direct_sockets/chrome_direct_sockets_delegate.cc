// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"

namespace {

using ProtocolType = content::DirectSocketsDelegate::ProtocolType;
using RequestDetails = content::DirectSocketsDelegate::RequestDetails;

bool ValidateAddressAndPortForChromeApp(const extensions::Extension* extension,
                                        const RequestDetails& request) {
  switch (request.protocol) {
    case ProtocolType::kTcp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::TCP_CONNECT,
                       request.address, request.port});
    case ProtocolType::kConnectedUdp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::UDP_SEND_TO,
                       request.address, request.port});
    case ProtocolType::kBoundUdp:
      // For kBoundUdp we check both UDP_BIND for the given |address| and
      // |port| as well as ensure that UDP_SEND_TO allows routing packets
      // anywhere. '*' is the wildcard address, 0 is the wildcard port.
      return extensions::SocketsManifestData::CheckRequest(
                 extension,
                 /*request=*/{content::SocketPermissionRequest::UDP_BIND,
                              request.address, request.port}) &&
             extensions::SocketsManifestData::CheckRequest(
                 extension,
                 /*request=*/{content::SocketPermissionRequest::UDP_SEND_TO,
                              /*host=*/"*", /*port=*/0});
    case ProtocolType::kTcpServer:
      return extensions::SocketsManifestData::CheckRequest(
          extension, /*request=*/{content::SocketPermissionRequest::TCP_LISTEN,
                                  request.address, request.port});
  }
}

bool ValidateAddressAndPortForIwa(const RequestDetails& request) {
  switch (request.protocol) {
    case ProtocolType::kTcp:
    case ProtocolType::kConnectedUdp:
      return true;
    case ProtocolType::kBoundUdp:
      // Port 0 indicates automatic port allocation.
      // Ports below 1024 are usually system ports and should not be exposed.
      return request.port == 0 || request.port >= 1024;
    case ProtocolType::kTcpServer:
      // Port 0 indicates automatic port allocation.
      // Ports below 1024 are usually system ports and should not be exposed.
      // Port numbers between 1024 and 32767 are usually specific to selected
      // apps (which predominantly communicate over TCP).
      return request.port == 0 || request.port >= 32768;
  }
}

}  // namespace

bool ChromeDirectSocketsDelegate::ValidateRequest(
    content::RenderFrameHost& rfh,
    const RequestDetails& request) {
  // If we're running an extension, follow the chrome.sockets.* permission
  // model.
  if (const extensions::Extension* extension =
          extensions::ProcessMap::Get(rfh.GetBrowserContext())
              ->GetEnabledExtensionByProcessID(
                  rfh.GetProcess()->GetDeprecatedID())) {
    return ValidateAddressAndPortForChromeApp(extension, request);
  }

  const GURL& url = rfh.GetMainFrame()->GetLastCommittedURL();
  if (HostContentSettingsMapFactory::GetForProfile(rfh.GetBrowserContext())
          ->GetContentSetting(url, url, ContentSettingsType::DIRECT_SOCKETS) !=
      CONTENT_SETTING_ALLOW) {
    return false;
  }

  if (url.SchemeIs(chrome::kIsolatedAppScheme)) {
    return ValidateAddressAndPortForIwa(request);
  }

  return false;
}

void ChromeDirectSocketsDelegate::RequestPrivateNetworkAccess(
    content::RenderFrameHost& rfh,
    base::OnceCallback<void(bool)> callback) {
  // No additional rules for Chrome Apps.
  if (extensions::ProcessMap::Get(rfh.GetBrowserContext())
          ->Contains(rfh.GetProcess()->GetDeprecatedID())) {
    std::move(callback).Run(/*allow_access=*/true);
    return;
  }

  // TODO(crbug.com/368266657): Show a permission prompt for DS-PNA & ponder
  // whether this requires transient activation.
  const GURL& url = rfh.GetMainFrame()->GetLastCommittedURL();
  std::move(callback).Run(
      HostContentSettingsMapFactory::GetForProfile(rfh.GetBrowserContext())
          ->GetContentSetting(
              url, url,
              ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS) ==
      CONTENT_SETTING_ALLOW);
}
