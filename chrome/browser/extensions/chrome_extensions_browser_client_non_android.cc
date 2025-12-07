// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/chrome_process_manager_delegate.h"
#include "chrome/browser/extensions/chrome_safe_browsing_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/url_loading/url_loader_factory.h"
#include "content/public/browser/site_instance.h"
#include "ipc/constants.mojom.h"

namespace extensions {

void ChromeExtensionsBrowserClient::Init() {
  process_manager_delegate_ = std::make_unique<ChromeProcessManagerDelegate>();

  // Must occur after g_browser_process is initialized.
  user_script_listener_ = std::make_unique<UserScriptListener>();

  // Full safe browsing is supported so use the Chrome delegate.
  safe_browsing_delegate_ = std::make_unique<ChromeSafeBrowsingDelegate>();
}

ProcessManagerDelegate*
ChromeExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return process_manager_delegate_.get();
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ChromeExtensionsBrowserClient::GetControlledFrameEmbedderURLLoader(
    const url::Origin& app_origin,
    content::FrameTreeNodeId frame_tree_node_id,
    content::BrowserContext* browser_context) {
  return web_app::IsolatedWebAppURLLoaderFactory::CreateForFrame(
      browser_context, app_origin, frame_tree_node_id);
}

void ChromeExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  ErrorConsole::Get(context)->ReportError(std::move(error));
}

void ChromeExtensionsBrowserClient::CleanUpWebView(
    content::BrowserContext* browser_context,
    int embedder_process_id,
    int view_instance_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          profile)) {
    return;
  }

  // Clean up context menus for the WebView.
  auto* menu_manager = MenuManager::Get(profile);
  DCHECK(menu_manager);
  // The |webview_embedder_frame_id| parameter of ExtensionKey is not used to
  // identify the context menu items that belong to a WebView so it is OK for it
  // to be |IPC::mojom::kRoutingIdNone| here.
  menu_manager->RemoveAllContextItems(MenuItem::ExtensionKey(
      "", embedder_process_id,
      /*webview_embedder_frame_id=*/IPC::mojom::kRoutingIdNone,
      view_instance_id));
}

void ChromeExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
    content::BrowserContext* browser_context,
    content::SiteInstance* owner_site_instance,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
        callback) {
  const GURL& owner_site_url = owner_site_instance->GetSiteURL();
  if (owner_site_url.SchemeIs(webapps::kIsolatedAppScheme)) {
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(owner_site_url);
    DCHECK(url_info.has_value()) << url_info.error();

    auto* profile = Profile::FromBrowserContext(browser_context);
    auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile);
    CHECK(web_app_provider);
    web_app_provider->scheduler().GetControlledFramePartition(
        *url_info, partition_name, in_memory, std::move(callback));
    return;
  }

  ExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
      browser_context, owner_site_instance, partition_name, in_memory,
      std::move(callback));
}

custom_handlers::ProtocolHandlerRegistry*
ChromeExtensionsBrowserClient::GetProtocolHandlerRegistry(
    content::BrowserContext* context) {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
}

}  // namespace extensions
