// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#endif

namespace {

using ProtocolType = content::DirectSocketsDelegate::ProtocolType;
using RequestDetails = content::DirectSocketsDelegate::RequestDetails;

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
#endif

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

bool IsContentSettingAllowedForUrl(content::BrowserContext* browser_context,
                                   const GURL& url,
                                   ContentSettingsType content_setting) {
  return HostContentSettingsMapFactory::GetForProfile(browser_context)
             ->GetContentSetting(url, url, content_setting) ==
         CONTENT_SETTING_ALLOW;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Returns true if |extension_id| is allowed to use the Direct Sockets API.
bool IsExtensionIdAllowedToUseDirectSockets(
    const extensions::Extension* extension) {
  constexpr auto kAllowedDirectSocketsExtensionIds =
      base::MakeFixedFlatSet<std::string_view>({
          "algkcnfjnajfhgimadimbjhmpaeohhln",  // Secure Shell Extension (dev)
          "iodihamcpbpeioajjeobimgagajmlibd",  // Secure Shell Extension
                                               // (stable)
      });
  return kAllowedDirectSocketsExtensionIds.contains(extension->id());
}
#endif

}  // namespace

bool ChromeDirectSocketsDelegate::AreDirectSocketsAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const GURL& url = origin.GetURL();

#if BUILDFLAG(IS_CHROMEOS)
  // Allow restricted context APIs in special pages.
  if (url.SchemeIs("chrome-untrusted") && url.host() == "terminal") {
    return true;
  }
#endif

  // This function might be called for profiles that do not support extensions.
  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return false;
  }

  // Allow Direct Sockets in Chrome Apps and selected extensions.
  auto* extension = registry->enabled_extensions().GetExtensionOrAppByURL(url);
  return extension &&
         (IsExtensionIdAllowedToUseDirectSockets(extension) ||
          extension->is_platform_app()) &&
         extensions::SocketsManifestData::Get(extension);
#else
  return false;
#endif
}

bool ChromeDirectSocketsDelegate::ValidateRequest(
    content::RenderFrameHost& rfh,
    const RequestDetails& request) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If we're running an extension, follow the chrome.sockets.* permission
  // model.
  if (const extensions::Extension* extension =
          extensions::ProcessMap::Get(rfh.GetBrowserContext())
              ->GetEnabledExtensionByProcessID(rfh.GetProcess()->GetID())) {
    return ValidateAddressAndPortForChromeApp(extension, request);
  }
#endif

  const GURL& url = rfh.GetMainFrame()->GetLastCommittedURL();
  if (!IsContentSettingAllowedForUrl(rfh.GetBrowserContext(), url,
                                     ContentSettingsType::DIRECT_SOCKETS)) {
    return false;
  }

  if (url.SchemeIs(webapps::kIsolatedAppScheme)) {
    return ValidateAddressAndPortForIwa(request);
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Allow restricted context APIs in special pages.
  if (url.SchemeIs("chrome-untrusted") && url.host() == "terminal") {
    return true;
  }
#endif

  return false;
}

bool ChromeDirectSocketsDelegate::ValidateRequestForSharedWorker(
    content::BrowserContext* browser_context,
    const GURL& shared_worker_url,
    const RequestDetails& request) {
  return IsContentSettingAllowedForUrl(browser_context, shared_worker_url,
                                       ContentSettingsType::DIRECT_SOCKETS) &&
         ValidateAddressAndPortForIwa(request);
}

bool ChromeDirectSocketsDelegate::ValidateRequestForServiceWorker(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const RequestDetails& request) {
  return IsContentSettingAllowedForUrl(browser_context, origin.GetURL(),
                                       ContentSettingsType::DIRECT_SOCKETS) &&
         ValidateAddressAndPortForIwa(request);
}
