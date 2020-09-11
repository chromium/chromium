// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
class ResourceContext;
class VpnServiceProxy;
}

namespace url {
class Origin;
}

namespace extensions {

// Implements the extensions portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientExtensionsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientExtensionsPart();
  ~ChromeContentBrowserClientExtensionsPart() override;

  // Corresponds to the ChromeContentBrowserClient function of the same name.
  static GURL GetEffectiveURL(Profile* profile, const GURL& url);
  static bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      content::BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url);
  static bool ShouldUseProcessPerSite(Profile* profile, const GURL& site_url);
  static bool ShouldUseSpareRenderProcessHost(Profile* profile,
                                              const GURL& site_url);
  static bool DoesSiteRequireDedicatedProcess(
      content::BrowserContext* browser_context,
      const GURL& effective_site_url);
  static bool ShouldLockProcess(content::BrowserContext* browser_context,
                                const GURL& effective_site_url);
  static bool CanCommitURL(content::RenderProcessHost* process_host,
                           const GURL& url);
  static bool IsSuitableHost(Profile* profile,
                             content::RenderProcessHost* process_host,
                             const GURL& site_url);
  static bool ShouldTryToUseExistingProcessHost(Profile* profile,
                                                const GURL& url);
  static bool ShouldSubframesTryToReuseExistingProcess(
      content::RenderFrameHost* main_frame);
  static bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url);
  // TODO(crbug.com/824858): Remove the OnIO method.
  static bool AllowServiceWorkerOnIO(const GURL& scope,
                                     const GURL& first_party_url,
                                     const GURL& script_url,
                                     content::ResourceContext* context);
  static bool AllowServiceWorkerOnUI(const GURL& scope,
                                     const GURL& first_party_url,
                                     const GURL& script_url,
                                     content::BrowserContext* context);
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

  static bool IsBuiltinComponent(content::BrowserContext* browser_context,
                                 const url::Origin& origin);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentBrowserClientExtensionsPartTest,
                           IsolatedOriginsAndHostedAppWebExtents);

  // ChromeContentBrowserClientParts:
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           blink::web_pref::WebPreferences* web_prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_allowed_schemes) override;
  void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) override;
  void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) override;
  void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost* process,
      Profile* profile) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientExtensionsPart);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
