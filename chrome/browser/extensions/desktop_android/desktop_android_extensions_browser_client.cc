// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/user_agent.h"
#include "extensions/browser/api/core_extensions_browser_api_provider.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"
#include "services/network/public/mojom/url_loader.mojom.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

class DesktopAndroidKioskDelegate : public KioskDelegate {
 public:
  DesktopAndroidKioskDelegate() = default;
  ~DesktopAndroidKioskDelegate() override = default;

  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override {
    // Desktop-android does not support kiosk apps.
    return false;
  }
};

}  // namespace

DesktopAndroidExtensionsBrowserClient::DesktopAndroidExtensionsBrowserClient()
    : extension_cache_(std::make_unique<NullExtensionCache>()),
      kiosk_delegate_(std::make_unique<DesktopAndroidKioskDelegate>()),
      api_client_(std::make_unique<ExtensionsAPIClient>()) {
  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
}

DesktopAndroidExtensionsBrowserClient::
    ~DesktopAndroidExtensionsBrowserClient() {}

bool DesktopAndroidExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsValidContext(void* context) {
  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process";
    NOTREACHED();
  }
  return g_browser_process->profile_manager() &&
         g_browser_process->profile_manager()->IsValidProfile(context);
}

bool DesktopAndroidExtensionsBrowserClient::IsSameContext(
    BrowserContext* first,
    BrowserContext* second) {
  Profile* first_profile = Profile::FromBrowserContext(first);
  Profile* second_profile = Profile::FromBrowserContext(second);
  return first_profile->IsSameOrParent(second_profile);
}

bool DesktopAndroidExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->HasPrimaryOTRProfile();
}

BrowserContext* DesktopAndroidExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
}

BrowserContext* DesktopAndroidExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetOriginalProfile();
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextRedirectedToOriginal(
    content::BrowserContext* context,
    bool force_guest_profile) {
  return context;
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextOwnInstance(
    content::BrowserContext* context,
    bool force_guest_profile) {
  return context;
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextForOriginalOnly(
    content::BrowserContext* context,
    bool force_guest_profile) {
  return context;
}

bool DesktopAndroidExtensionsBrowserClient::AreExtensionsDisabledForContext(
    content::BrowserContext* context) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

base::FilePath DesktopAndroidExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  *resource_id = 0;
  return base::FilePath();
}

void DesktopAndroidExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  NOTREACHED() << "Load resources from bundles not supported.";
}

bool DesktopAndroidExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    const GURL& upstream_url) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          request, destination, page_transition, child_id, is_incognito,
          extension, extensions, process_map, upstream_url, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* DesktopAndroidExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrefs();
}

void DesktopAndroidExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate*
DesktopAndroidExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return nullptr;
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DesktopAndroidExtensionsBrowserClient::GetControlledFrameEmbedderURLLoader(
    const url::Origin& app_origin,
    content::FrameTreeNodeId frame_tree_node_id,
    content::BrowserContext* browser_context) {
  return mojo::PendingRemote<network::mojom::URLLoaderFactory>();
}

std::unique_ptr<ExtensionHostDelegate>
DesktopAndroidExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return nullptr;
}

bool DesktopAndroidExtensionsBrowserClient::DidVersionUpdate(
    BrowserContext* context) {
  return false;
}

void DesktopAndroidExtensionsBrowserClient::PermitExternalProtocolHandler() {}

bool DesktopAndroidExtensionsBrowserClient::IsInDemoMode() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
DesktopAndroidExtensionsBrowserClient::GetExtensionSystemFactory() {
  return DesktopAndroidExtensionSystem::GetFactory();
}

void DesktopAndroidExtensionsBrowserClient::
    RegisterBrowserInterfaceBindersForFrame(
        mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
        content::RenderFrameHost* render_frame_host,
        const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);
}

std::unique_ptr<RuntimeAPIDelegate>
DesktopAndroidExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return nullptr;
}

const ComponentExtensionResourceManager*
DesktopAndroidExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return nullptr;
}

void DesktopAndroidExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args,
    bool dispatch_to_off_the_record_profiles) {}

ExtensionCache* DesktopAndroidExtensionsBrowserClient::GetExtensionCache() {
  return extension_cache_.get();
}

bool DesktopAndroidExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool DesktopAndroidExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

void DesktopAndroidExtensionsBrowserClient::CreateExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  DesktopAndroidExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

ExtensionWebContentsObserver*
DesktopAndroidExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return DesktopAndroidExtensionWebContentsObserver::FromWebContents(
      web_contents);
}

KioskDelegate* DesktopAndroidExtensionsBrowserClient::GetKioskDelegate() {
  return kiosk_delegate_.get();
}

bool DesktopAndroidExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
  return false;
}

std::string DesktopAndroidExtensionsBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string DesktopAndroidExtensionsBrowserClient::GetUserAgent() const {
  return content::BuildUserAgentFromProduct(
      std::string(version_info::GetProductNameAndVersionForUserAgent()));
}

}  // namespace extensions
