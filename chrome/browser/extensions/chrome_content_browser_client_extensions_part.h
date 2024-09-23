// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
class VpnServiceProxy;
class WebContents;
}

namespace url {
class Origin;
}

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace service_manager {
template <typename...>
class BinderRegistryWithArgs;
using BinderRegistry = BinderRegistryWithArgs<>;
}  // namespace service_manager

class Profile;

namespace extensions {

// Implements the extensions portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientExtensionsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientExtensionsPart();

  ChromeContentBrowserClientExtensionsPart(
      const ChromeContentBrowserClientExtensionsPart&) = delete;
  ChromeContentBrowserClientExtensionsPart& operator=(
      const ChromeContentBrowserClientExtensionsPart&) = delete;

  ~ChromeContentBrowserClientExtensionsPart() override;

  // Corresponds to the ChromeContentBrowserClient function of the same name.
  static GURL GetEffectiveURL(Profile* profile, const GURL& url);
  static bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      content::BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_outermost_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url);
  static bool ShouldUseProcessPerSite(Profile* profile, const GURL& site_url);
  static bool ShouldUseSpareRenderProcessHost(Profile* profile,
                                              const GURL& site_url);
  static bool DoesSiteRequireDedicatedProcess(
      content::BrowserContext* browser_context,
      const GURL& effective_site_url);
  static bool ShouldAllowCrossProcessSandboxedFrameForPrecursor(
      content::BrowserContext* browser_context,
      const GURL& precursor,
      const GURL& url);
  static bool CanCommitURL(content::RenderProcessHost* process_host,
                           const GURL& url);
  static bool IsSuitableHost(Profile* profile,
                             content::RenderProcessHost* process_host,
                             const GURL& site_url);
  static size_t GetProcessCountToIgnoreForLimit();
  static bool ShouldEmbeddedFramesTryToReuseExistingProcess(
      content::RenderFrameHost* outermost_main_frame);
  static bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url);
  static bool AllowServiceWorker(const GURL& scope,
                                 const GURL& first_party_url,
                                 const GURL& script_url,
                                 content::BrowserContext* context);
  static bool MayDeleteServiceWorkerRegistration(
      const GURL& scope,
      content::BrowserContext* browser_context);
  static bool ShouldTryToUpdateServiceWorkerRegistration(
      const GURL& scope,
      content::BrowserContext* browser_context);
  static std::vector<url::Origin> GetOriginsRequiringDedicatedProcess();

  // Helper function to call InfoMap::SetSigninProcess().
  static void SetSigninProcess(content::SiteInstance* site_instance);

  // Creates a new VpnServiceProxy. The caller owns the returned value. It's
  // valid to return nullptr.
  static std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy(
      content::BrowserContext* browser_context);

  static void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params);

  // Checks if the component is a loaded component extension or the ODFS
  // external component extension.
  static bool IsBuiltinComponent(content::BrowserContext* browser_context,
                                 const url::Origin& origin);

  // Checks if the given context support extensions, based on the profile type.
  static bool AreExtensionsDisabledForProfile(
      content::BrowserContext* browser_context);

  // Temporarily allows unregistration of the service worker with the given
  // `scope` for testing purposes; unregistration is allowed while the returned
  // AutoReset remains in scope.
  static base::AutoReset<const GURL*>
  AllowServiceWorkerUnregistrationForScopeForTesting(const GURL* scope);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentBrowserClientExtensionsPartTest,
                           IsolatedOriginsAndHostedAppWebExtents);

  // ChromeContentBrowserClientParts:
  void SiteInstanceGotProcessAndSite(
      content::SiteInstance* site_instance) override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
  bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_allowed_schemes) override;
  void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) override;
  void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      download::QuarantineConnectionCallback quarantine_connection_callback,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) override;
  void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost& process) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void ExposeInterfacesToRendererForServiceWorker(
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void ExposeInterfacesToRendererForRenderFrameHost(
      content::RenderFrameHost& frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
