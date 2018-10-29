// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/guest_view/browser/guest_view_message_filter.h"
#include "components/rappor/public/rappor_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_service_worker_message_filter.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserContext;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using content::WebPreferences;

namespace extensions {

namespace {

// Used by the GetPrivilegeRequiredByUrl() and GetProcessPrivilege() functions
// below.  Extension, and isolated apps require different privileges to be
// granted to their RenderProcessHosts.  This classification allows us to make
// sure URLs are served by hosts with the right set of privileges.
enum RenderProcessHostPrivilege {
  PRIV_NORMAL,
  PRIV_HOSTED,
  PRIV_ISOLATED,
  PRIV_EXTENSION,
};

// Specifies the scheme of the SiteInstance responsible for a failed
// web-accessible resource check in ShouldAllowOpenURL.
//
// This enum backs an UMA histogram.  The order of existing values
// should not be changed.  Add any new values before SCHEME_LAST, and also run
// update_should_allow_open_url_histograms.py to update the corresponding enum
// in histograms.xml.  This enum must also be synchronized to kSchemeNames in
// RecordShouldAllowOpenURLFailure.
enum ShouldAllowOpenURLFailureScheme {
  SCHEME_UNKNOWN,
  SCHEME_EMPTY,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FILE,
  SCHEME_FTP,
  SCHEME_DATA,
  SCHEME_JAVASCRIPT,
  SCHEME_ABOUT,
  SCHEME_CHROME,
  SCHEME_DEVTOOLS,
  SCHEME_GUEST,
  SCHEME_VIEWSOURCE,
  SCHEME_CHROME_SEARCH,
  SCHEME_CHROME_NATIVE,
  SCHEME_DOM_DISTILLER,
  SCHEME_CHROME_EXTENSION,
  SCHEME_CONTENT,
  SCHEME_BLOB,
  SCHEME_FILESYSTEM,
  // Add new entries above and make sure to update histograms.xml by running
  // update_should_allow_open_url_histograms.py.
  SCHEME_LAST,
};

RenderProcessHostPrivilege GetPrivilegeRequiredByUrl(
    const GURL& url,
    ExtensionRegistry* registry) {
  // Default to a normal renderer cause it is lower privileged. This should only
  // occur if the URL on a site instance is either malformed, or uninitialized.
  // If it is malformed, then there is no need for better privileges anyways.
  // If it is uninitialized, but eventually settles on being an a scheme other
  // than normal webrenderer, the navigation logic will correct us out of band
  // anyways.
  if (!url.is_valid())
    return PRIV_NORMAL;

  if (!url.SchemeIs(kExtensionScheme))
    return PRIV_NORMAL;

  const Extension* extension =
      registry->enabled_extensions().GetByID(url.host());
  if (extension && AppIsolationInfo::HasIsolatedStorage(extension))
    return PRIV_ISOLATED;
  if (extension && extension->is_hosted_app())
    return PRIV_HOSTED;
  return PRIV_EXTENSION;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    content::RenderProcessHost* process_host,
    ProcessMap* process_map,
    ExtensionRegistry* registry) {
  std::set<std::string> extension_ids =
      process_map->GetExtensionsInProcess(process_host->GetID());
  if (extension_ids.empty())
    return PRIV_NORMAL;

  for (const std::string& extension_id : extension_ids) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(extension_id);
    if (extension && AppIsolationInfo::HasIsolatedStorage(extension))
      return PRIV_ISOLATED;
    if (extension && extension->is_hosted_app())
      return PRIV_HOSTED;
  }

  return PRIV_EXTENSION;
}

// Determines whether the extension |origin| is legal to use in an Origin header
// from the process identified by |child_id|.  Returns CONTINUE if so, FAIL if
// the extension is not recognized (and may recently have been uninstalled), and
// KILL if the origin is from a platform app but the request does not come from
// that app.
content::HeaderInterceptorResult CheckOriginHeader(
    content::ResourceContext* resource_context,
    int child_id,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Consider non-extension URLs safe; they will be checked elsewhere.
  if (!origin.SchemeIs(kExtensionScheme))
    return content::HeaderInterceptorResult::CONTINUE;

  // If there is no extension installed for the origin, it may be from a
  // recently uninstalled extension.  The tabs of such extensions are
  // automatically closed, but subframes and content scripts may stick around.
  // Fail such requests without killing the process.
  // TODO(rdevlin.cronin, creis): Track which extensions have been uninstalled
  // and use HeaderInterceptorResult::KILL for anything not on the list.
  // See https://crbug.com/705128.
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
  const Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(origin);
  if (!extension)
    return content::HeaderInterceptorResult::FAIL;

  // Check for platform app origins.  These can only be committed by the app
  // itself, or by one if its guests if it has the webview permission.
  // Processes that incorrectly claim to be an app should be killed.
  const ProcessMap& process_map = extension_info_map->process_map();
  if (extension->is_platform_app() &&
      !process_map.Contains(extension->id(), child_id)) {
    // This is a platform app origin not in the app's own process.  If it cannot
    // create webviews, this is illegal.
    if (!extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kWebView)) {
      return content::HeaderInterceptorResult::KILL;
    }

    // If there are accessible resources, the origin is only legal if the
    // given process is a guest of the app.
    std::string owner_extension_id;
    int owner_process_id;
    WebViewRendererState::GetInstance()->GetOwnerInfo(
        child_id, &owner_process_id, &owner_extension_id);
    const Extension* owner_extension =
        extension_info_map->extensions().GetByID(owner_extension_id);
    if (!owner_extension || owner_extension != extension)
      return content::HeaderInterceptorResult::KILL;

    // It's a valid guest of the app, so allow it to proceed.
    return content::HeaderInterceptorResult::CONTINUE;
  }

  // With only the origin and not the full URL, we don't have enough
  // information to validate hosted apps or web_accessible_resources in normal
  // extensions. Assume they're legal.
  return content::HeaderInterceptorResult::CONTINUE;
}

// This callback is registered on the ResourceDispatcherHost for the chrome
// extension Origin scheme. We determine whether the extension origin is
// valid. Please see the CheckOriginHeader() function.
void OnHttpHeaderReceived(const std::string& header,
                          const std::string& value,
                          int child_id,
                          content::ResourceContext* resource_context,
                          content::OnHeaderProcessedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GURL origin(value);
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));

  callback.Run(CheckOriginHeader(resource_context, child_id, origin));
}

const Extension* GetEnabledExtensionFromEffectiveURL(
    BrowserContext* context,
    const GURL& effective_url) {
  if (!effective_url.SchemeIs(kExtensionScheme))
    return nullptr;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return nullptr;

  return registry->enabled_extensions().GetByID(effective_url.host());
}

bool HasEffectiveUrl(content::BrowserContext* browser_context,
                     const GURL& url) {
  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
             Profile::FromBrowserContext(browser_context), url) != url;
}

}  // namespace

ChromeContentBrowserClientExtensionsPart::
    ChromeContentBrowserClientExtensionsPart() {
}

ChromeContentBrowserClientExtensionsPart::
    ~ChromeContentBrowserClientExtensionsPart() {
}

// static
GURL ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
    Profile* profile,
    const GURL& url) {
  // If the input |url| is part of an installed app, the effective URL is an
  // extension URL with the ID of that extension as the host. This has the
  // effect of grouping apps together in a common SiteInstance.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!registry)
    return url;

  const Extension* extension =
      registry->enabled_extensions().GetHostedAppByURL(url);
  if (!extension)
    return url;

  // Bookmark apps do not use the hosted app process model, and should be
  // treated as normal URLs.
  if (extension->from_bookmark())
    return url;

  // If |url| corresponds to both an isolated origin and a hosted app,
  // determine whether to use the effective URL, which also determines whether
  // the isolated origin should take precedence over a matching hosted app:
  // - Chrome Web Store should always be resolved to its effective URL, so that
  //   the CWS process gets proper bindings.
  // - for other hosted apps, if the isolated origin covers the app's entire
  //   web extent (i.e., *all* URLs matched by the hosted app will have this
  //   isolated origin), allow the hosted app to take effect and return an
  //   effective URL.
  // - for other cases, disallow effective URLs, as otherwise this would allow
  //   the isolated origin to share the hosted app process with other origins
  //   it does not trust, due to https://crbug.com/791796.
  //
  // TODO(alexmos): Revisit and possibly remove this once
  // https://crbug.com/791796 is fixed.
  url::Origin isolated_origin;
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  bool is_isolated_origin = policy->GetMatchingIsolatedOrigin(
      url::Origin::Create(url), &isolated_origin);
  if (is_isolated_origin && extension->id() != kWebStoreAppId &&
      !DoesOriginMatchAllURLsInWebExtent(isolated_origin,
                                         extension->web_extent())) {
    return url;
  }

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldCompareEffectiveURLsForSiteInstanceSelection(
        content::BrowserContext* browser_context,
        content::SiteInstance* candidate_site_instance,
        bool is_main_frame,
        const GURL& candidate_url,
        const GURL& destination_url) {
  // Don't compare effective URLs for any subframe navigations, since we don't
  // want to create OOPIFs based on that mechanism (e.g., for hosted apps). For
  // main frames, don't compare effective URLs when transitioning from app to
  // non-app URLs if there exists another app WebContents that might script
  // this one.  These navigations should stay in the app process to not break
  // scripting when a hosted app opens a same-site popup. See
  // https://crbug.com/718516 and https://crbug.com/828720 and
  // https://crbug.com/859062.
  if (!is_main_frame)
    return false;
  size_t candidate_active_contents_count =
      candidate_site_instance->GetRelatedActiveContentsCount();
  bool src_has_effective_url = HasEffectiveUrl(browser_context, candidate_url);
  bool dest_has_effective_url =
      HasEffectiveUrl(browser_context, destination_url);
  if (src_has_effective_url && !dest_has_effective_url &&
      candidate_active_contents_count > 1u)
    return false;
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
    Profile* profile, const GURL& effective_url) {
  const Extension* extension =
      GetEnabledExtensionFromEffectiveURL(profile, effective_url);
  if (!extension)
    return false;

  // If the URL is part of a hosted app that does not have the background
  // permission, or that does not allow JavaScript access to the background
  // page, we want to give each instance its own process to improve
  // responsiveness.
  if (extension->GetType() == Manifest::TYPE_HOSTED_APP) {
    if (!extension->permissions_data()->HasAPIPermission(
            APIPermission::kBackground) ||
        !BackgroundInfo::AllowJSAccess(extension)) {
      return false;
    }
  }

  // Hosted apps that have script access to their background page must use
  // process per site, since all instances can make synchronous calls to the
  // background window.  Other extensions should use process per site as well.
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldUseSpareRenderProcessHost(
    Profile* profile,
    const GURL& site_url) {
  // Extensions should not use a spare process, because they require passing a
  // command-line flag (switches::kExtensionProcess) to the renderer process
  // when it launches. A spare process is launched earlier, before it is known
  // which navigation will use it, so it lacks this flag.
  return !site_url.SchemeIs(kExtensionScheme);
}

// static
bool ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetExtensionOrAppByURL(effective_site_url);
  if (!extension)
    return false;

  // Always isolate Chrome Web Store.
  if (extension->id() == kWebStoreAppId)
    return true;

  // Extensions should be isolated, except for hosted apps. Isolating hosted
  // apps is a good idea, but ought to be a separate knob.
  if (extension->is_hosted_app())
    return false;

  // Isolate all extensions.
  return true;
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldLockToOrigin(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (!effective_site_url.SchemeIs(kExtensionScheme))
    return true;

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetExtensionOrAppByURL(effective_site_url);
  if (!extension)
    return true;

  // Hosted apps should be locked to their web origin. See
  // https://crbug.com/794315.
  if (extension->is_hosted_app())
    return true;

  // Other extensions are allowed to share processes, even in
  // --site-per-process currently. See https://crbug.com/600441#c1 for some
  // background on the intersection of extension process reuse and site
  // isolation.
  //
  // TODO(nick): Fix this, possibly by revamping the extensions process model
  // so that sharing is determined by privilege level, as described in
  // https://crbug.com/766267.
  return false;
}

// static
bool ChromeContentBrowserClientExtensionsPart::CanCommitURL(
    content::RenderProcessHost* process_host, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enforce that extension URLs commit in the correct extension process where
  // possible, accounting for many exceptions to the rule.

  // Don't bother if there is no registry.
  // TODO(rdevlin.cronin): Can this be turned into a DCHECK?  Seems like there
  // should always be a registry.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(process_host->GetBrowserContext());
  if (!registry)
    return true;

  // Only perform the checks below if the URL being committed has an extension
  // associated with it.
  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
  if (!extension)
    return true;

  // If the process is a dedicated process for this extension, then it's safe to
  // commit. This accounts for cases where an extension might have multiple
  // processes, such as incognito split mode.
  ProcessMap* process_map = ProcessMap::Get(process_host->GetBrowserContext());
  if (process_map->Contains(extension->id(), process_host->GetID())) {
    return true;
  }

  // TODO(creis): We're seeing cases where an extension URL commits in an
  // extension process but not one registered for it in ProcessMap. This is
  // surprising and we do not yet have repro steps for it. We should fix this,
  // but we're primarily concerned with preventing web processes from committing
  // an extension URL, which is more severe. (Extensions currently share
  // processes with each other anyway.) Allow it for now, as long as this is an
  // extension and not a hosted app.
  if (GetProcessPrivilege(process_host, process_map, registry) ==
      PRIV_EXTENSION) {
    return true;
  }

  // Most hosted apps (except for the Chrome Web Store) can commit anywhere.
  // The Chrome Web Store should never commit outside its process, regardless of
  // the other exceptions below.
  if (extension->is_hosted_app())
    return extension->id() != kWebStoreAppId;

  // Some special case extension URLs must be allowed to load in any guest. Note
  // that CanCommitURL may be called for validating origins as well, so do not
  // enforce a path comparison in the special cases unless there is a real path
  // (more than just "/").
  // TODO(creis): Remove this call when bugs 688565 and 778021 are resolved.
  base::StringPiece url_path = url.path_piece();
  bool is_guest =
      WebViewRendererState::GetInstance()->IsGuest(process_host->GetID());
  if (is_guest &&
      url_request_util::AllowSpecialCaseExtensionURLInGuest(
          extension, url_path.length() > 1
                         ? base::make_optional<base::StringPiece>(url_path)
                         : base::nullopt)) {
    return true;
  }

  // Platform app URLs may commit in their own guest processes, when they have
  // the webview permission.  (Some extensions are allowlisted for webviews as
  // well, but their pages load in their own extension process and are allowed
  // through above.)
  if (is_guest) {
    std::string owner_extension_id;
    int owner_process_id = -1;
    bool found_owner = WebViewRendererState::GetInstance()->GetOwnerInfo(
        process_host->GetID(), &owner_process_id, &owner_extension_id);
    DCHECK(found_owner);
    return extension->is_platform_app() &&
           extension->permissions_data()->HasAPIPermission(
               extensions::APIPermission::kWebView) &&
           extension->id() == owner_extension_id;
  }

  // Otherwise, the process is wrong for this extension URL.
  return false;
}

// static
bool ChromeContentBrowserClientExtensionsPart::IsSuitableHost(
    Profile* profile,
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  DCHECK(profile);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  ProcessMap* process_map = ProcessMap::Get(profile);

  // These may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!registry || !process_map)
    return true;

  // Otherwise, just make sure the process privilege matches the privilege
  // required by the site.
  RenderProcessHostPrivilege privilege_required =
      GetPrivilegeRequiredByUrl(site_url, registry);
  return GetProcessPrivilege(process_host, process_map, registry) ==
         privilege_required;
}

// static
bool
ChromeContentBrowserClientExtensionsPart::ShouldTryToUseExistingProcessHost(
    Profile* profile, const GURL& url) {
  // This function is trying to limit the amount of processes used by extensions
  // with background pages. It uses a globally set percentage of processes to
  // run such extensions and if the limit is exceeded, it returns true, to
  // indicate to the content module to group extensions together.
  ExtensionRegistry* registry =
      profile ? ExtensionRegistry::Get(profile) : NULL;
  if (!registry)
    return false;

  // We have to have a valid extension with background page to proceed.
  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
  if (!extension)
    return false;
  if (!BackgroundInfo::HasBackgroundPage(extension))
    return false;

  std::set<int> process_ids;
  size_t max_process_count =
      content::RenderProcessHost::GetMaxRendererProcessCount();

  // Go through all profiles to ensure we have total count of extension
  // processes containing background pages, otherwise one profile can
  // starve the other.
  std::vector<Profile*> profiles = g_browser_process->profile_manager()->
      GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    ProcessManager* epm = ProcessManager::Get(profiles[i]);
    for (ExtensionHost* host : epm->background_hosts())
      process_ids.insert(host->render_process_host()->GetID());
  }

  return (process_ids.size() >
          (max_process_count * chrome::kMaxShareOfExtensionProcesses));
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldSwapBrowsingInstancesForNavigation(SiteInstance* site_instance,
                                             const GURL& current_url,
                                             const GURL& new_url) {
  // If we don't have an ExtensionRegistry, then rely on the SiteInstance logic
  // in RenderFrameHostManager to decide when to swap.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry)
    return false;

  // We must use a new BrowsingInstance (forcing a process swap and disabling
  // scripting by existing tabs) if one of the URLs corresponds to the Chrome
  // Web Store hosted app, and the other does not.
  //
  // We don't force a BrowsingInstance swap in other cases (i.e., when opening
  // a popup from one extension to a different extension, or to a non-extension
  // URL) to preserve script connections and allow use cases like postMessage
  // via window.opener. Those cases would still force a SiteInstance swap in
  // RenderFrameHostManager.  This behavior is similar to how extension
  // subframes on a web main frame are also placed in the same BrowsingInstance
  // (by the content/ part of ShouldSwapBrowsingInstancesForNavigation); this
  // check is just doing the same for top-level frames.  See
  // https://crbug.com/590068.
  const Extension* current_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(current_url);
  bool is_current_url_for_web_store =
      current_extension && current_extension->id() == kWebStoreAppId;

  const Extension* new_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(new_url);
  bool is_new_url_for_web_store =
      new_extension && new_extension->id() == kWebStoreAppId;

  // First do a process check.  We should force a BrowsingInstance swap if we
  // are going to Chrome Web Store, but the current process doesn't know about
  // CWS, even if current_extension somehow corresponds to CWS.
  ProcessMap* process_map = ProcessMap::Get(site_instance->GetBrowserContext());
  if (is_new_url_for_web_store && site_instance->HasProcess() &&
      !process_map->Contains(new_extension->id(),
                             site_instance->GetProcess()->GetID()))
    return true;

  // Otherwise, swap BrowsingInstances when transitioning to/from Chrome Web
  // Store.
  return is_current_url_for_web_store != is_new_url_for_web_store;
}

// static
bool ChromeContentBrowserClientExtensionsPart::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party_url,
    content::ResourceContext* context) {
  // We only care about extension urls.
  if (!first_party_url.SchemeIs(kExtensionScheme))
    return true;

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
  const Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(first_party_url);
  // Don't allow a service worker for an extension url with no extension (this
  // could happen in the case of, e.g., an unloaded extension).
  return extension != nullptr;
}

// static
void ChromeContentBrowserClientExtensionsPart::OverrideNavigationParams(
    content::SiteInstance* site_instance,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer) {
  const Extension* extension =
      ExtensionRegistry::Get(site_instance->GetBrowserContext())
          ->enabled_extensions()
          .GetExtensionOrAppByURL(site_instance->GetSiteURL());
  if (!extension)
    return;

  // Hide the referrer for extension pages. We don't want sites to see a
  // referrer of chrome-extension://<...>.
  if (extension->is_extension())
    *referrer = content::Referrer();
}

// static
bool ChromeContentBrowserClientExtensionsPart::ShouldAllowOpenURL(
    content::SiteInstance* site_instance,
    const GURL& to_url,
    bool* result) {
  DCHECK(result);

  // Using url::Origin is important to properly handle blob: and filesystem:
  // URLs.
  url::Origin to_origin = url::Origin::Create(to_url);
  if (to_origin.scheme() != kExtensionScheme) {
    // We're not responsible for protecting this resource.  Note that hosted
    // apps fall into this category.
    return false;
  }

  // Do not allow pages from the web or other extensions navigate to
  // non-web-accessible extension resources.

  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  const Extension* to_extension =
      registry->enabled_extensions().GetByID(to_origin.host());
  if (!to_extension) {
    // Treat non-existent extensions the same as an extension without accessible
    // resources.
    *result = false;
    return true;
  }

  GURL site_url(site_instance->GetSiteURL());
  const Extension* from_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(site_url);
  if (from_extension && from_extension == to_extension) {
    *result = true;
    return true;
  }

  // Blob and filesystem URLs are never considered web-accessible.  See
  // https://crbug.com/656752.
  if (to_url.SchemeIsFileSystem() || to_url.SchemeIsBlob()) {
    if (to_url.SchemeIsFileSystem())
      RecordShouldAllowOpenURLFailure(FAILURE_FILE_SYSTEM_URL, site_url);
    else
      RecordShouldAllowOpenURLFailure(FAILURE_BLOB_URL, site_url);

    // TODO(alexmos): Temporary instrumentation to find any regressions for
    // this blocking.  Remove after verifying that this is not breaking any
    // legitimate use cases.
    DEBUG_ALIAS_FOR_GURL(site_url_copy, site_url);
    DEBUG_ALIAS_FOR_ORIGIN(to_origin_copy, to_origin);
    base::debug::DumpWithoutCrashing();

    *result = false;
    return true;
  }

  // Navigations from chrome://, chrome-search:// and chrome-devtools:// pages
  // need to be allowed, even if |to_url| is not web-accessible. See
  // https://crbug.com/662602.
  //
  // Note that this is intentionally done after the check for blob: and
  // filesystem: URLs above, for consistency with the renderer-side checks
  // which already disallow navigations from chrome URLs to blob/filesystem
  // URLs.
  if (site_url.SchemeIs(content::kChromeUIScheme) ||
      site_url.SchemeIs(content::kChromeDevToolsScheme) ||
      site_url.SchemeIs(chrome::kChromeSearchScheme)) {
    *result = true;
    return true;
  }

  // <webview> guests should be allowed to load only webview-accessible
  // resources, but that check is done later in
  // AllowCrossRendererResourceLoadHelper, so allow <webview> guests to proceed
  // here and rely on that check instead.  See https://crbug.com/691941.
  if (site_url.SchemeIs(content::kGuestScheme)) {
    *result = true;
    return true;
  }

  if (WebAccessibleResourcesInfo::IsResourceWebAccessible(to_extension,
                                                          to_url.path())) {
    *result = true;
    return true;
  }

  if (!site_url.SchemeIsHTTPOrHTTPS() && !site_url.SchemeIs(kExtensionScheme)) {
    // This function used to incorrectly skip the web-accessible resource
    // checks in this case. Measure how often this happens.  See also
    // https://crbug.com/696034.
    RecordShouldAllowOpenURLFailure(
        FAILURE_SCHEME_NOT_HTTP_OR_HTTPS_OR_EXTENSION, site_url);
  } else {
    RecordShouldAllowOpenURLFailure(FAILURE_RESOURCE_NOT_WEB_ACCESSIBLE,
                                    site_url);
  }

  *result = false;
  return true;
}

// static
std::unique_ptr<content::VpnServiceProxy>
ChromeContentBrowserClientExtensionsPart::GetVpnServiceProxy(
    content::BrowserContext* browser_context) {
#if defined(OS_CHROMEOS)
  chromeos::VpnService* vpn_service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context);
  if (!vpn_service)
    return nullptr;
  return vpn_service->GetVpnServiceProxy();
#else
  return nullptr;
#endif
}

// static
void ChromeContentBrowserClientExtensionsPart::
    LogInitiatorSchemeBypassingDocumentBlocking(
        const url::Origin& initiator_origin,
        int render_process_id,
        content::ResourceType resource_type) {
  // Return early if the RenderProcessHost can't be found.  This can happen if
  // the process goes away for some reason during the IO -> UI thread hop
  // required for calling LogInitiatorSchemeBypassingDocumentBlocking.
  content::RenderProcessHost* process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!process_host)
    return;
  content::BrowserContext* browser_context = process_host->GetBrowserContext();

  // Until there is Site Isolation in GuestViews, CORB cannot offer protection
  // against compromised renderers.  Therefore, let's ignore GuestViews when
  // gathering the list of extensions that issue CORB-eligible requests.
  if (process_host->IsForGuestsOnly())
    return;

  // Assert that |initiator_origin| corresponds to an extension and extract the
  // |extension_id|.
  DCHECK_EQ(kExtensionScheme, initiator_origin.scheme());
  const std::string& extension_id = initiator_origin.host();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  DCHECK(registry->enabled_extensions().GetByID(extension_id));

  // Don't log anything if the request was initiated by an extension process
  // (we're only interested in requests initiated by content scripts).
  ProcessMap* process_map = ProcessMap::Get(browser_context);
  if (process_map->Contains(extension_id, render_process_id))
    return;

  // Log that CORB would have blocked in a meaningful way a request that was
  // initiated by a content script.
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Allowed.ContentScript",
                            resource_type, content::RESOURCE_TYPE_LAST_TYPE);
  rappor::SampleString(rappor::GetDefaultService(),
                       "Extensions.CrossOriginFetchFromContentScript2",
                       rappor::UMA_RAPPOR_TYPE, extension_id);
}

// static
network::mojom::URLLoaderFactoryPtrInfo
ChromeContentBrowserClientExtensionsPart::
    CreateURLLoaderFactoryForNetworkRequests(
        content::RenderProcessHost* process,
        network::mojom::NetworkContext* network_context,
        const url::Origin& request_initiator) {
  // TODO(lukasza): https://crbug.com/894766: Re-enable after a real fix for
  // this bug.  For now, let's just avoid using separate URLLoaderFactories
  // for extensions.
  // return URLLoaderFactoryManager::CreateFactory(process, network_context,
  //                                              request_initiator);
  return network::mojom::URLLoaderFactoryPtrInfo();
}

// static
void ChromeContentBrowserClientExtensionsPart::RecordShouldAllowOpenURLFailure(
    ShouldAllowOpenURLFailureReason reason,
    const GURL& site_url) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.ShouldAllowOpenURL.Failure", reason,
                            FAILURE_LAST);

  // Must be kept in sync with the ShouldAllowOpenURLFailureScheme enum.
  static const char* const kSchemeNames[] = {
      "unknown",
      "",
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kFileScheme,
      url::kFtpScheme,
      url::kDataScheme,
      url::kJavaScriptScheme,
      url::kAboutScheme,
      content::kChromeUIScheme,
      content::kChromeDevToolsScheme,
      content::kGuestScheme,
      content::kViewSourceScheme,
      chrome::kChromeSearchScheme,
      chrome::kChromeNativeScheme,
      dom_distiller::kDomDistillerScheme,
      extensions::kExtensionScheme,
      url::kContentScheme,
      url::kBlobScheme,
      url::kFileSystemScheme,
      "last",
  };

  static_assert(arraysize(kSchemeNames) == SCHEME_LAST + 1,
                "kSchemeNames should have SCHEME_LAST + 1 elements");

  ShouldAllowOpenURLFailureScheme scheme = SCHEME_UNKNOWN;
  for (int i = 1; i < SCHEME_LAST; i++) {
    if (site_url.SchemeIs(kSchemeNames[i])) {
      scheme = static_cast<ShouldAllowOpenURLFailureScheme>(i);
      break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.ShouldAllowOpenURL.Failure.Scheme",
                            scheme, SCHEME_LAST);
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    DoesOriginMatchAllURLsInWebExtent(const url::Origin& origin,
                                      const URLPatternSet& web_extent) {
  // This function assumes |origin| is an isolated origin, which can only have
  // an HTTP or HTTPS scheme (see IsolatedOriginUtil::IsValidIsolatedOrigin()),
  // so these are the only schemes allowed to be matched below.
  DCHECK(origin.scheme() == url::kHttpsScheme ||
         origin.scheme() == url::kHttpScheme);
  URLPattern origin_pattern(URLPattern::SCHEME_HTTPS | URLPattern::SCHEME_HTTP);
  // TODO(alexmos): Temporarily disable precise scheme matching on
  // |origin_pattern| to allow apps that use *://foo.com/ in their web extent
  // to still work with isolated origins.  See https://crbug.com/799638.  We
  // should use SetScheme(origin.scheme()) here once https://crbug.com/791796
  // is fixed.
  origin_pattern.SetScheme("*");
  origin_pattern.SetHost(origin.host());
  origin_pattern.SetPath("/*");
  // We allow matching subdomains here because |origin| is the precise origin
  // retrieved from site isolation policy. Thus, we'll only allow an extent of
  // foo.example.com and bar.example.com if the isolated origin was
  // example.com; if the isolated origin is foo.example.com, this will
  // correctly fail.
  origin_pattern.SetMatchSubdomains(true);

  URLPatternSet origin_pattern_list;
  origin_pattern_list.AddPattern(origin_pattern);
  return origin_pattern_list.Contains(web_extent);
}

void ChromeContentBrowserClientExtensionsPart::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new ChromeExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionMessageFilter(id, profile));
  host->AddFilter(new IOThreadExtensionMessageFilter(id, profile));
  host->AddFilter(new ExtensionsGuestViewMessageFilter(id, profile));
  if (extensions::ExtensionsClient::Get()
          ->ExtensionAPIEnabledInExtensionServiceWorkers()) {
    host->AddFilter(new ExtensionServiceWorkerMessageFilter(
        id, profile, host->GetStoragePartition()->GetServiceWorkerContext()));
  }
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  BrowserContext* context = site_instance->GetProcess()->GetBrowserContext();

  // Only add the process to the map if the SiteInstance's site URL is already
  // a chrome-extension:// URL. This includes hosted apps, except in rare cases
  // that a URL in the hosted app's extent is not treated as a hosted app (e.g.,
  // for isolated origins or cross-site iframes). For that case, don't look up
  // the hosted app's Extension from the site URL using GetExtensionOrAppByURL,
  // since it isn't treated as a hosted app.
  const Extension* extension =
      GetEnabledExtensionFromEffectiveURL(context, site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(context)->Insert(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&InfoMap::RegisterExtensionProcess,
                     ExtensionSystem::Get(context)->info_map(), extension->id(),
                     site_instance->GetProcess()->GetID(),
                     site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  BrowserContext* context = site_instance->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return;

  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          site_instance->GetSiteURL());
  if (!extension)
    return;

  ProcessMap::Get(context)->Remove(extension->id(),
                                   site_instance->GetProcess()->GetID(),
                                   site_instance->GetId());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&InfoMap::UnregisterExtensionProcess,
                     ExtensionSystem::Get(context)->info_map(), extension->id(),
                     site_instance->GetProcess()->GetID(),
                     site_instance->GetId()));
}

void ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs(
    RenderViewHost* rvh,
    WebPreferences* web_prefs) {
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(rvh->GetProcess()->GetBrowserContext());
  if (!registry)
    return;

  // Note: it's not possible for kExtensionsScheme to change during the lifetime
  // of the process.
  //
  // Ensure that we are only granting extension preferences to URLs with
  // the correct scheme. Without this check, chrome-guest:// schemes used by
  // webview tags as well as hosts that happen to match the id of an
  // installed extension would get the wrong preferences.
  const GURL& site_url = rvh->GetSiteInstance()->GetSiteURL();
  if (!site_url.SchemeIs(kExtensionScheme))
    return;

  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  ViewType view_type = GetViewType(web_contents);
  const Extension* extension =
      registry->enabled_extensions().GetByID(site_url.host());
  extension_webkit_preferences::SetPreferences(extension, view_type, web_prefs);
}

void ChromeContentBrowserClientExtensionsPart::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  handler->AddHandlerPair(&ExtensionWebUI::HandleChromeURLOverride,
                          BrowserURLHandler::null_handler());
  handler->AddHandlerPair(BrowserURLHandler::null_handler(),
                          &ExtensionWebUI::HandleChromeURLOverrideReverse);
}

void ChromeContentBrowserClientExtensionsPart::
    GetAdditionalAllowedSchemesForFileSystem(
        std::vector<std::string>* additional_allowed_schemes) {
  additional_allowed_schemes->push_back(kExtensionScheme);
}

void ChromeContentBrowserClientExtensionsPart::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  handlers->push_back(
      base::Bind(MediaFileSystemBackend::AttemptAutoMountForURLRequest));
}

void ChromeContentBrowserClientExtensionsPart::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends) {
  additional_backends->push_back(
      std::make_unique<MediaFileSystemBackend>(storage_partition_path));

  additional_backends->push_back(
      std::make_unique<sync_file_system::SyncFileSystemBackend>(
          Profile::FromBrowserContext(browser_context)));
}

void ChromeContentBrowserClientExtensionsPart::
    AppendExtraRendererCommandLineSwitches(base::CommandLine* command_line,
                                           content::RenderProcessHost* process,
                                           Profile* profile) {
  if (!process)
    return;
  DCHECK(profile);
  if (ProcessMap::Get(profile)->Contains(process->GetID())) {
    command_line->AppendSwitch(switches::kExtensionProcess);
  }
}

void ChromeContentBrowserClientExtensionsPart::ResourceDispatcherHostCreated() {
  content::ResourceDispatcherHost::Get()->RegisterInterceptor(
      "Origin", kExtensionScheme, base::Bind(&OnHttpHeaderReceived));
}

}  // namespace extensions
