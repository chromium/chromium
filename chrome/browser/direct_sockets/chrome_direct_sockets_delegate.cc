// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"

bool ChromeDirectSocketsDelegate::IsAPIAccessAllowed(
    content::RenderFrameHost& rfh) {
  const GURL& url = rfh.GetLastCommittedURL();
  return HostContentSettingsMapFactory::GetForProfile(rfh.GetBrowserContext())
             ->GetContentSetting(url, url,
                                 ContentSettingsType::DIRECT_SOCKETS) ==
         CONTENT_SETTING_ALLOW;
}

bool ChromeDirectSocketsDelegate::ValidateAddressAndPort(
    content::RenderFrameHost& rfh,
    const std::string& address,
    uint16_t port,
    ProtocolType protocol) {
  int32_t process_id = rfh.GetProcess()->GetID();
  auto* process_map = extensions::ProcessMap::Get(rfh.GetBrowserContext());

  if (!process_map->Contains(process_id)) {
    // Additional restrictions are imposed only on extension-like contexts.
    return true;
  }

  // If we're running an extension, follow the chrome.sockets.* permission
  // model.
  const extensions::Extension* extension =
      process_map->GetEnabledExtensionByProcessID(process_id);
  if (!extension) {
    return false;
  }

  switch (protocol) {
    case ProtocolType::kTcp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::TCP_CONNECT, address,
                       port});
    case ProtocolType::kConnectedUdp:
      return extensions::SocketsManifestData::CheckRequest(
          extension,
          /*request=*/{content::SocketPermissionRequest::UDP_SEND_TO, address,
                       port});
    case ProtocolType::kBoundUdp:
      // For kBoundUdp we check both UDP_BIND for the given |address| and
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
    case ProtocolType::kTcpServer:
      return extensions::SocketsManifestData::CheckRequest(
          extension, /*request=*/{content::SocketPermissionRequest::TCP_LISTEN,
                                  address, port});
  }
}
