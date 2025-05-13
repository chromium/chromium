// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extension_host_delegate.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"
#include "chrome/browser/extensions/chrome_kiosk_delegate.h"
#include "chrome/browser/extensions/chrome_process_manager_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/url_constants.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/common/mojom/view_type.mojom-shared.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/core/common/features.h"
#endif

namespace extensions {

void ChromeExtensionsBrowserClient::Init() {
  process_manager_delegate_ = std::make_unique<ChromeProcessManagerDelegate>();
  api_client_ = std::make_unique<ChromeExtensionsAPIClient>();
  resource_manager_ =
      std::make_unique<ChromeComponentExtensionResourceManager>();

  // Must occur after g_browser_process is initialized.
  user_script_listener_ = std::make_unique<UserScriptListener>();
}

void ChromeExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {
  observers->push_back(ContentSettingsService::Get(context));
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

std::unique_ptr<ExtensionHostDelegate>
ChromeExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return std::make_unique<ChromeExtensionHostDelegate>();
}

void ChromeExtensionsBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);
  PopulateChromeFrameBindersForExtension(binder_map, render_frame_host,
                                         extension);
}

const ComponentExtensionResourceManager*
ChromeExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return resource_manager_.get();
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
  // to be |MSG_ROUTING_NONE| here.
  menu_manager->RemoveAllContextItems(MenuItem::ExtensionKey(
      "", embedder_process_id, /*webview_embedder_frame_id=*/MSG_ROUTING_NONE,
      view_instance_id));
}

void ChromeExtensionsBrowserClient::AttachExtensionTaskManagerTag(
    content::WebContents* web_contents,
    mojom::ViewType view_type) {
  switch (view_type) {
    case mojom::ViewType::kAppWindow:
    case mojom::ViewType::kComponent:
    case mojom::ViewType::kExtensionBackgroundPage:
    case mojom::ViewType::kExtensionPopup:
    case mojom::ViewType::kOffscreenDocument:
    case mojom::ViewType::kExtensionSidePanel:
      // These are the only types that are tracked by the ExtensionTag.
      task_manager::WebContentsTags::CreateForExtension(web_contents,
                                                        view_type);
      return;

    case mojom::ViewType::kBackgroundContents:
    case mojom::ViewType::kExtensionGuest:
    case mojom::ViewType::kTabContents:
    case mojom::ViewType::kDeveloperTools:
      // Those types are tracked by other tags:
      // BACKGROUND_CONTENTS --> task_manager::BackgroundContentsTag.
      // GUEST --> ChromeGuestViewManagerDelegate.
      // PANEL --> task_manager::PanelTag.
      // TAB_CONTENTS --> task_manager::TabContentsTag.
      // DEVELOPER_TOOLS --> task_manager::DevToolsTag.
      // These tags are created and attached to the web_contents in other
      // locations, and they must be ignored here.
      return;

    case mojom::ViewType::kInvalid:
      NOTREACHED();
  }
}

KioskDelegate* ChromeExtensionsBrowserClient::GetKioskDelegate() {
  if (!kiosk_delegate_) {
    kiosk_delegate_ = std::make_unique<ChromeKioskDelegate>();
  }
  return kiosk_delegate_.get();
}

ScriptExecutor* ChromeExtensionsBrowserClient::GetScriptExecutorForTab(
    content::WebContents& web_contents) {
  TabHelper* tab_helper = TabHelper::FromWebContents(&web_contents);
  return tab_helper ? tab_helper->script_executor() : nullptr;
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  auto signal = std::make_unique<safe_browsing::TabsExecuteScriptSignal>(
      extension_id, code);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

bool ChromeExtensionsBrowserClient::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return telemetry_service && telemetry_service->enabled();
#else
  return false;
#endif
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiDeclarativeNetRequest(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::vector<api::declarative_net_request::Rule>& rules) const {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = std::make_unique<safe_browsing::DeclarativeNetRequestSignal>(
      extension_id, rules);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

void ChromeExtensionsBrowserClient::
    NotifyExtensionDeclarativeNetRequestRedirectAction(
        content::BrowserContext* context,
        const ExtensionId& extension_id,
        const GURL& request_url,
        const GURL& redirect_url) const {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::
              kExtensionTelemetryDeclarativeNetRequestActionSignal)) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = safe_browsing::DeclarativeNetRequestActionSignal::
      CreateDeclarativeNetRequestRedirectActionSignal(extension_id, request_url,
                                                      redirect_url);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

void ChromeExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
    content::BrowserContext* browser_context,
    content::SiteInstance* owner_site_instance,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
        callback) {
  const GURL& owner_site_url = owner_site_instance->GetSiteURL();
  if (owner_site_url.SchemeIs(chrome::kIsolatedAppScheme)) {
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

void ChromeExtensionsBrowserClient::CreatePasswordReuseDetectionManager(
    content::WebContents* web_contents) const {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
#endif
}

}  // namespace extensions
