// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/base/page_transition_types.h"

namespace content {
struct Referrer;
class RenderProcessHost;
class ResourceContext;
class VpnServiceProxy;
}

namespace url {
class Origin;
}

namespace extensions {

class URLPatternSet;

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
  static bool ShouldUseProcessPerSite(Profile* profile,
                                      const GURL& effective_url);
  static bool ShouldUseSpareRenderProcessHost(Profile* profile,
                                              const GURL& site_url);
  static bool DoesSiteRequireDedicatedProcess(
      content::BrowserContext* browser_context,
      const GURL& effective_site_url);
  static bool ShouldLockToOrigin(content::BrowserContext* browser_context,
                                 const GURL& effective_site_url);
  static bool CanCommitURL(content::RenderProcessHost* process_host,
                           const GURL& url);
  static bool IsSuitableHost(Profile* profile,
                             content::RenderProcessHost* process_host,
                             const GURL& site_url);
  static bool ShouldTryToUseExistingProcessHost(Profile* profile,
                                                const GURL& url);
  static bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url);
  static bool AllowServiceWorker(const GURL& scope,
                                 const GURL& first_party_url,
                                 content::ResourceContext* context);
  static void OverrideNavigationParams(content::SiteInstance* site_instance,
                                       ui::PageTransition* transition,
                                       bool* is_renderer_initiated,
                                       content::Referrer* referrer);

  // Similiar to ChromeContentBrowserClient::ShouldAllowOpenURL(), but the
  // return value indicates whether to use |result| or not.
  static bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                                 const GURL& to_url,
                                 bool* result);

  // Helper function to call InfoMap::SetSigninProcess().
  static void SetSigninProcess(content::SiteInstance* site_instance);

  // Creates a new VpnServiceProxy. The caller owns the returned value. It's
  // valid to return nullptr.
  static std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy(
      content::BrowserContext* browser_context);

  static void LogInitiatorSchemeBypassingDocumentBlocking(
      const url::Origin& initiator_origin,
      int render_process_id,
      content::ResourceType resource_type);

  static network::mojom::URLLoaderFactoryPtrInfo
  CreateURLLoaderFactoryForNetworkRequests(
      content::RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      const url::Origin& request_initiator);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentBrowserClientExtensionsPartTest,
                           ShouldAllowOpenURLMetricsForEmptySiteURL);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentBrowserClientExtensionsPartTest,
                           ShouldAllowOpenURLMetricsForKnownSchemes);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentBrowserClientExtensionsPartTest,
                           IsolatedOriginsAndHostedAppWebExtents);

  // Specifies reasons why web-accessible resource checks in ShouldAllowOpenURL
  // might fail.
  //
  // This enum backs an UMA histogram.  The order of existing values
  // should not be changed, and new values should only be added before
  // FAILURE_LAST.
  enum ShouldAllowOpenURLFailureReason {
    FAILURE_FILE_SYSTEM_URL = 0,
    FAILURE_BLOB_URL,
    FAILURE_SCHEME_NOT_HTTP_OR_HTTPS_OR_EXTENSION,
    FAILURE_RESOURCE_NOT_WEB_ACCESSIBLE,
    FAILURE_LAST,
  };

  // Records metrics when ShouldAllowOpenURL blocks a load.  |site_url|
  // corresponds to the SiteInstance that initiated the blocked load.
  static void RecordShouldAllowOpenURLFailure(
      ShouldAllowOpenURLFailureReason reason,
      const GURL& site_url);

  // Returns true if all URLs matched by |web_extent| have the same origin as
  // |origin|, or have an origin which is a subdomain of |origin|.
  //
  // When |origin| requires a dedicated process, this helps determine whether
  // all URLs in |web_extent| are ok to go into |origin|'s process.
  static bool DoesOriginMatchAllURLsInWebExtent(
      const url::Origin& origin,
      const URLPatternSet& web_extent);

  // ChromeContentBrowserClientParts:
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override;
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
  void ResourceDispatcherHostCreated() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientExtensionsPart);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_BROWSER_CLIENT_EXTENSIONS_PART_H_

