// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/common/guest_view.mojom.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using blink::web_pref::WebPreferences;
using content::BrowserContext;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

namespace {

// If non-null, a scope of a service worker to always allow to be unregistered.
const GURL* g_allow_service_worker_unregistration_scope = nullptr;

const Extension* GetEnabledExtensionFromSiteURL(BrowserContext* context,
                                                const GURL& site_url) {
  if (!site_url.SchemeIs(kExtensionScheme))
    return nullptr;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return nullptr;

  return registry->enabled_extensions().GetByID(site_url.host());
}

bool HasEffectiveUrl(content::BrowserContext* browser_context,
                     const GURL& url) {
  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
             Profile::FromBrowserContext(browser_context), url) != url;
}

bool AllowServiceWorker(const GURL& scope,
                        const GURL& script_url,
                        const Extension* extension) {
  // Don't allow a service worker for an extension url with no extension (this
  // could happen in the case of, e.g., an unloaded extension).
  if (!extension)
    return false;

  // If an extension doesn't have a service worker-based background script, it
  // can register a service worker at any scope.
  if (!BackgroundInfo::IsServiceWorkerBased(extension)) {
    return true;
  }

  // If the script_url parameter is an empty string, allow it. The
  // infrastructure will call this function at times when the script url is
  // unknown, but it is always known at registration, so this is OK.
  if (script_url.is_empty())
    return true;

  // An extension with a service worked-based background script can register a
  // service worker at any scope other than the root scope.
  if (scope != extension->url())
    return true;

  // If an extension is service-worker based, only the script specified in the
  // manifest can be registered at the root scope.
  const std::string& sw_script =
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension);
  return script_url == extension->GetResourceURL(sw_script);
}

// Returns the extension associated with the given `scope` if and only if it's
// a service worker-based extension.
const Extension* GetServiceWorkerBasedExtensionForScope(
    const GURL& scope,
    BrowserContext* browser_context) {
  // We only care about extension urls.
  if (!scope.SchemeIs(kExtensionScheme)) {
    return nullptr;
  }

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetExtensionOrAppByURL(scope);
  if (!extension) {
    return nullptr;
  }

  // We only consider service workers that are root-scoped and for service
  // worker-based extensions.
  if (scope != extension->url() ||
      !BackgroundInfo::IsServiceWorkerBased(extension)) {
    return nullptr;
  }

  return extension;
}

// Returns the number of processes containing extension background pages across
// all profiles. If this is large enough (e.g., at browser startup time), it can
// pose a risk that normal web processes will be overly constrained by the
// browser's process limit.
size_t GetExtensionBackgroundProcessCount() {
  std::set<int> process_ids;

  // Go through all profiles to ensure we have total count of extension
  // processes containing background pages, otherwise one profile can
  // starve the other. See https://crbug.com/98737.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    ProcessManager* epm = ProcessManager::Get(profile);
    if (!epm)
      continue;
    for (ExtensionHost* host : epm->background_hosts())
      process_ids.insert(host->render_process_host()->GetID());
  }
  return process_ids.size();
}

}  // namespace

ChromeContentBrowserClientExtensionsPart::
    ChromeContentBrowserClientExtensionsPart() = default;

ChromeContentBrowserClientExtensionsPart::
    ~ChromeContentBrowserClientExtensionsPart() = default;

// static
GURL ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
    Profile* profile,
    const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);

  // If the URL is part of a hosted app's web extent, convert it to the app's
  // extension URL. I.e., the effective URL becomes a chrome-extension: URL
  // with the ID of the hosted app as the host.  This has the effect of
  // grouping (possibly cross-site) URLs belonging to one hosted app together
  // in a common SiteInstance, and it ensures that hosted app capabilities are
  // properly granted to that SiteInstance's process.
  //
  // Note that we don't need to carry over the |url|'s path, because the
  // process model only uses the origin of a hosted app's effective URL.  Note
  // also that we must not return an invalid effective URL here, since that
  // might lead to incorrect security decisions - see
  // https://crbug.com/1016954.
  const Extension* hosted_app =
      registry->enabled_extensions().GetHostedAppByURL(url);
  if (hosted_app)
    return hosted_app->url();

  // If this is a chrome-extension: URL, check whether a corresponding
  // extension exists and is enabled. If this is not the case, translate |url|
  // into |kExtensionInvalidRequestURL| to avoid assigning a particular
  // extension's disabled and enabled extension URLs to the same SiteInstance.
  // This is important to prevent the SiteInstance and (unprivileged) process
  // hosting a disabled extension URL from incorrectly getting reused after
  // re-enabling the extension, which would lead to renderer kills
  // (https://crbug.com/1197360).
  if (url.SchemeIs(kExtensionScheme) &&
      !registry->enabled_extensions().GetExtensionOrAppByURL(url)) {
    return GURL(extensions::kExtensionInvalidRequestURL);
  }

  // Don't translate to effective URLs in all other cases.
  return url;
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldCompareEffectiveURLsForSiteInstanceSelection(
        content::BrowserContext* browser_context,
        content::SiteInstance* candidate_site_instance,
        bool is_outermost_main_frame,
        const GURL& candidate_url,
        const GURL& destination_url) {
  // Don't compare effective URLs for navigations involving embedded frames,
  // since we don't want to create OOPIFs based on that mechanism (e.g., for
  // hosted apps). For outermost main frames, don't compare effective URLs when
  // transitioning from app to non-app URLs if there exists another app
  // WebContents that might script this one.  These navigations should stay in
  // the app process to not break scripting when a hosted app opens a same-site
  // popup. See https://crbug.com/718516 and https://crbug.com/828720 and
  // https://crbug.com/859062.
  if (!is_outermost_main_frame)
    return false;
  size_t candidate_active_contents_count =
      candidate_site_instance->GetRelatedActiveContentsCount();

  // Intentionally only checks for hosted app effective URLs and not NTP-based
  // effective URLs (which ChromeContentBrowserClient::GetEffectiveURL would
  // include as well). This avoids keeping same-site popups in the NTP's
  // process, per https://crbug.com/859062.
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
    Profile* profile,
    const GURL& site_url) {
  const Extension* extension =
      GetEnabledExtensionFromSiteURL(profile, site_url);
  if (!extension)
    return false;

  // If the URL is part of a hosted app that does not have the background
  // permission, or that does not allow JavaScript access to the background
  // page, we want to give each instance its own process to improve
  // responsiveness.
  if (extension->GetType() == Manifest::TYPE_HOSTED_APP) {
    if (!extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kBackground) ||
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
  // Isolate all extensions.
  return extension != nullptr;
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldAllowCrossProcessSandboxedFrameForPrecursor(
        content::BrowserContext* browser_context,
        const GURL& precursor,
        const GURL& url) {
  if (precursor.is_empty()) {
    return true;
  }

  // Non-manifest sandboxed extension URLs should stay in the main extension
  // process, and have API access. Manifest-sandboxed extension URLs, sandboxed
  // about:srcdoc and data urls should be isolated in cross-process sandboxes,
  // and not have API access.
  const ExtensionId extension_id = ExtensionSet::GetExtensionIdByURL(precursor);
  if (extension_id.empty()) {
    return true;
  }

  if (url.IsAboutSrcdoc() || url.SchemeIs(url::kDataScheme)) {
    return true;
  }

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    // If the extension isn't active, allow using a cross-process sandbox.
    return true;
  }

  // Determine whether the URL is manifest-sandboxed.
  return SandboxedPageInfo::IsSandboxedPage(extension, url.path());
}

// static
bool ChromeContentBrowserClientExtensionsPart::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
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

  // If an extension URL is listed as sandboxed in the manifest, its process
  // won't be in the process map. Instead, allow it here and rely on the
  // ChildProcessSecurityPolicy::CanAccessDataForOrigin check (which occurs
  // separately) to verify that the ProcessLock matches the extension's origin.
  // TODO(https://crbug.com/346264217): Also ensure the process is sandboxed, if
  // that does not cause problems for pushState cases.
  if (SandboxedPageInfo::IsSandboxedPage(extension, url.path())) {
    return true;
  }

  // Most hosted apps (except for the Chrome Web Store) can commit anywhere.
  // The Chrome Web Store should never commit outside its process, regardless of
  // the other exceptions below.
  if (extension->is_hosted_app())
    return extension->id() != kWebStoreAppId;

  // Platform app URLs may commit in their own guest processes, when they have
  // the webview permission.  (Some extensions are allowlisted for webviews as
  // well, but their pages load in their own extension process and are allowed
  // through above.)
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  bool is_guest =
      WebViewRendererState::GetInstance()->IsGuest(process_host->GetID());
  if (is_guest) {
    ExtensionId owner_extension_id;
    int owner_process_id = -1;
    bool found_owner = WebViewRendererState::GetInstance()->GetOwnerInfo(
        process_host->GetID(), &owner_process_id, &owner_extension_id);
    DCHECK(found_owner);
    return extension->is_platform_app() &&
           extension->permissions_data()->HasAPIPermission(
               mojom::APIPermissionID::kWebView) &&
           extension->id() == owner_extension_id;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

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

  // Don't use a process that's not in the ProcessMap for a site URL that
  // corresponds to an enabled extension. For example, this prevents a
  // navigation to an enabled extension's URL from reusing a process that has
  // previously loaded non-functional URLs from that same extension while it
  // was disabled.
  //
  // Note that this is called on site URLs that have been computed after
  // effective URL translation, so site URLs with an extension scheme capture
  // SiteInstances for both extensions and hosted apps.
  const Extension* extension =
      GetEnabledExtensionFromSiteURL(profile, site_url);
  if (extension &&
      !process_map->Contains(extension->id(), process_host->GetID())) {
    return false;
  }

  // Conversely, don't use an extension process for a site URL that does not
  // map to an enabled extension. For example, this prevents a reload of an
  // extension or app that has just been disabled from staying in the
  // privileged extension process.
  if (!extension && process_map->Contains(process_host->GetID())) {
    return false;
  }

  // Otherwise, the extensions layer is ok with using `process_host` for
  // `site_url`.
  return true;
}

size_t
ChromeContentBrowserClientExtensionsPart::GetProcessCountToIgnoreForLimit() {
  // If this is a unit test with no profile manager, there is no need to ignore
  // any processes.
  if (!g_browser_process->profile_manager())
    return 0;

  size_t max_process_count =
      content::RenderProcessHost::GetMaxRendererProcessCount();

  // Ignore any extension background processes over the extension portion of the
  // process limit when deciding whether to reuse other renderer processes.
  return std::max(0, static_cast<int>(GetExtensionBackgroundProcessCount() -
                                      (max_process_count *
                                       chrome::kMaxShareOfExtensionProcesses)));
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldEmbeddedFramesTryToReuseExistingProcess(
        content::RenderFrameHost* outermost_main_frame) {
  DCHECK(!outermost_main_frame->GetParentOrOuterDocument());

  // Most out-of-process embedded frames aggressively look for a random
  // same-site process to reuse if possible, to keep the process count low. Skip
  // this for web frames inside extensions (not including hosted apps), since
  // the workload here tends to be different and we want to avoid slowing down
  // normal web pages with misbehaving extension-related content.
  //
  // Note that this does not prevent process sharing with tabs when over the
  // process limit, and OOPIFs from tabs (which will aggressively look for
  // existing processes) may still join the process of an extension's web
  // iframe.  This mainly reduces the likelihood of problems with main frames
  // and makes it more likely that the subframe process will be shown near the
  // extension in Chrome's task manager for blame purposes. See
  // https://crbug.com/899418.
  const Extension* extension =
      ExtensionRegistry::Get(
          outermost_main_frame->GetSiteInstance()->GetBrowserContext())
          ->enabled_extensions()
          .GetExtensionOrAppByURL(
              outermost_main_frame->GetSiteInstance()->GetSiteURL());
  return !extension || !extension->is_extension();
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    ShouldSwapBrowsingInstancesForNavigation(
        SiteInstance* site_instance,
        const GURL& current_effective_url,
        const GURL& destination_effective_url) {
  // If we don't have an ExtensionRegistry, then rely on the SiteInstance logic
  // in RenderFrameHostManager to decide when to swap.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry)
    return false;

  // We must use a new BrowsingInstance (forcing a process swap and disabling
  // scripting by existing tabs) if one of the URLs corresponds to the Chrome
  // Web Store and the other does not. For the old Web Store this is done by
  // checking for the Web Store hosted app and for the new Web Store we just
  // check against the expected URL.
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

  // First we check for navigations which are transitioning to/from the URL
  // associated with the new Webstore.
  bool current_url_matches_new_webstore =
      url::Origin::Create(current_effective_url)
          .IsSameOriginWith(extension_urls::GetNewWebstoreLaunchURL());
  bool dest_url_matches_new_webstore =
      url::Origin::Create(destination_effective_url)
          .IsSameOriginWith(extension_urls::GetNewWebstoreLaunchURL());
  if (current_url_matches_new_webstore != dest_url_matches_new_webstore)
    return true;

  // Next we do a process check, looking to see if the Web Store hosted app ID
  // is associated with the URLs.
  const Extension* current_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          current_effective_url);
  bool is_current_url_for_webstore_app =
      current_extension && current_extension->id() == kWebStoreAppId;

  const Extension* dest_extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          destination_effective_url);
  bool is_dest_url_for_webstore_app =
      dest_extension && dest_extension->id() == kWebStoreAppId;

  // We should force a BrowsingInstance swap if we are going to Chrome Web
  // Store, but the current process doesn't know about CWS, even if
  // current_extension somehow corresponds to CWS.
  ProcessMap* process_map = ProcessMap::Get(site_instance->GetBrowserContext());
  if (is_dest_url_for_webstore_app && site_instance->HasProcess() &&
      !process_map->Contains(dest_extension->id(),
                             site_instance->GetProcess()->GetID()))
    return true;

  // Otherwise, swap BrowsingInstances when transitioning to/from Chrome Web
  // Store.
  return is_current_url_for_webstore_app != is_dest_url_for_webstore_app;
}

// static
bool ChromeContentBrowserClientExtensionsPart::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party_url,
    const GURL& script_url,
    content::BrowserContext* context) {
  // We only care about extension urls.
  if (!first_party_url.SchemeIs(kExtensionScheme))
    return true;

  const Extension* extension = ExtensionRegistry::Get(context)
                                   ->enabled_extensions()
                                   .GetExtensionOrAppByURL(first_party_url);
  return ::extensions::AllowServiceWorker(scope, script_url, extension);
}

// static
bool ChromeContentBrowserClientExtensionsPart::
    MayDeleteServiceWorkerRegistration(
        const GURL& scope,
        content::BrowserContext* browser_context) {
  // Check if we're allowed to unregister this worker for testing purposes.
  if (g_allow_service_worker_unregistration_scope &&
      *g_allow_service_worker_unregistration_scope == scope) {
    return true;
  }

  const Extension* extension =
      GetServiceWorkerBasedExtensionForScope(scope, browser_context);

  if (!extension) {
    return true;
  }

  base::Version registered_version =
      ServiceWorkerTaskQueue::Get(browser_context)
          ->RetrieveRegisteredServiceWorkerVersion(extension->id());
  // The service worker was never fully registered; this can happen in the case
  // of e.g. throwing errors in response to installation events (where the
  // worker is registered, but then immediately unregistered).
  if (!registered_version.IsValid()) {
    return true;
  }

  // Don't allow the unregistration of a valid, enabled service worker-based
  // extension's background service worker. Doing so would put the extension in
  // a broken state. The service worker registration is instead tied to the
  // extension's enablement; it is unregistered when the extension is disabled
  // or uninstalled.
  return registered_version != extension->version();
}

bool ChromeContentBrowserClientExtensionsPart::
    ShouldTryToUpdateServiceWorkerRegistration(
        const GURL& scope,
        content::BrowserContext* browser_context) {
  const Extension* extension =
      GetServiceWorkerBasedExtensionForScope(scope, browser_context);
  // Only allow updates through the service worker layer for non-extension
  // service workers. Extension service workers are updated through the
  // extensions system, along with the rest of the extension.
  return extension == nullptr;
}

// static
std::vector<url::Origin> ChromeContentBrowserClientExtensionsPart::
    GetOriginsRequiringDedicatedProcess() {
  std::vector<url::Origin> list;

  // Require a dedicated process for the webstore origin.  See
  // https://crbug.com/939108.
  list.push_back(url::Origin::Create(extension_urls::GetWebstoreLaunchURL()));
  list.push_back(
      url::Origin::Create(extension_urls::GetNewWebstoreLaunchURL()));

  return list;
}

// static
std::unique_ptr<content::VpnServiceProxy>
ChromeContentBrowserClientExtensionsPart::GetVpnServiceProxy(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::VpnServiceInterface* vpn_service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context);
  if (!vpn_service)
    return nullptr;
  return vpn_service->GetVpnServiceProxy();
#else
  return nullptr;
#endif
}

// static
void ChromeContentBrowserClientExtensionsPart::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
}

// static
bool ChromeContentBrowserClientExtensionsPart::IsBuiltinComponent(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
#else
  if (origin.scheme() != kExtensionScheme) {
    return false;
  }

  const auto& extension_id = origin.host();

#if BUILDFLAG(IS_CHROMEOS)
  // Check if the component is the ODFS extension.
  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      extension_id == extension_misc::kODFSExtensionId) {
    // Check ODFS was loaded externally.
    const Extension* extension = ExtensionRegistry::Get(browser_context)
                                     ->GetInstalledExtension(extension_id);
    if (!extension) {
      // Occurs due to a race condition at startup where the ODFS is installed
      // but does not yet appear in the extension registry.
      LOG(ERROR) << "ODFS cannot be found in the extension registry";
      return false;
    }
    return extension->location() == mojom::ManifestLocation::kExternalComponent;
  }
#endif

  // Check if the component is a loaded component extension.
  return ExtensionSystem::Get(browser_context)
      ->extension_service()
      ->component_loader()
      ->Exists(extension_id);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

// static
bool ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
    content::BrowserContext* browser_context) {
  return AreKeyedServicesDisabledForProfileByDefault(
      Profile::FromBrowserContext(browser_context));
}

// static
base::AutoReset<const GURL*> ChromeContentBrowserClientExtensionsPart::
    AllowServiceWorkerUnregistrationForScopeForTesting(const GURL* scope) {
  return base::AutoReset<const GURL*>(
      &g_allow_service_worker_unregistration_scope, scope);
}

void ChromeContentBrowserClientExtensionsPart::SiteInstanceGotProcessAndSite(
    SiteInstance* site_instance) {
  BrowserContext* context = site_instance->GetProcess()->GetBrowserContext();

  // Only add the process to the map if the SiteInstance's site URL is a
  // chrome-extension:// URL. This includes hosted apps, except in rare cases
  // that a URL in the hosted app's extent is not treated as a hosted app (e.g.,
  // for isolated origins or cross-site iframes). For that case, don't look up
  // the hosted app's Extension from the site URL using GetExtensionOrAppByURL,
  // since it isn't treated as a hosted app.
  const Extension* extension =
      GetEnabledExtensionFromSiteURL(context, site_instance->GetSiteURL());
  if (!extension) {
    return;
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // Don't consider guests that load extension URLs as extension processes,
  // except for the PDF Viewer extension URL. This is possible when an embedder
  // app navigates <webview> to a webview-accessible app resource; the resulting
  // <webview> process shouldn't receive extension process privileges. The PDF
  // Viewer extension is an exception. The PDF extension is in a separate
  // process that needs to be classified as privileged in order to expose the
  // appropriate API methods to it.
#if BUILDFLAG(ENABLE_PDF)
  const bool is_oopif_pdf_extension =
      chrome_pdf::features::IsOopifPdfEnabled() &&
      extension->id() == extension_misc::kPdfExtensionId;
#else
  constexpr bool is_oopif_pdf_extension = false;
#endif  // BUILDFLAG(ENABLE_PDF)

  if (site_instance->IsGuest() && !is_oopif_pdf_extension) {
    return;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

  // Manifest-sandboxed documents, and data: or or about:srcdoc urls, do not get
  // access to the extension APIs. We trust that the given SiteInstance is only
  // marked as sandboxed in cases that do not have access to extension APIs.
  if (site_instance->IsSandboxed()) {
    return;
  }

  // Note that this may be called more than once for multiple instances
  // of the same extension, such as when the same hosted app is opened in
  // unrelated tabs. This call will ignore duplicate insertions, which is fine,
  // since we only need to track if the extension is in the process, rather
  // than how many instances it has in that process.
  ProcessMap::Get(context)->Insert(extension->id(),
                                   site_instance->GetProcess()->GetID());
}

bool ChromeContentBrowserClientExtensionsPart::
    OverrideWebPreferencesAfterNavigation(WebContents* web_contents,
                                          WebPreferences* web_prefs) {
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents->GetBrowserContext());
  if (!registry)
    return false;

  // Note: it's not possible for kExtensionsScheme to change during the lifetime
  // of the process.
  //
  // Ensure that we are only granting extension preferences to URLs with
  // the correct scheme. Without this check, hosts that happen to match the id
  // of an installed extension would get the wrong preferences.
  // TODO(crbug.com/40265045): Once the `web_prefs` have been set based on
  // `extension` below, they are not unset when navigating a tab from an
  // extension page to a regular web page. We should clear extension settings in
  // this case.
  const GURL& site_url =
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL();
  if (!site_url.SchemeIs(kExtensionScheme))
    return false;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If a webview navigates to a webview accessible resource, extension
  // preferences should not be applied to the webview.
  // TODO(crbug.com/40265045): Once it is possible to clear extension settings
  // after a navigation, we can remove this case so that extension settings can
  // apply to webview accessible resources without impacting web pages
  // subsequently loaded in the webview.
  if (WebViewGuest::FromWebContents(web_contents)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const Extension* extension =
      registry->enabled_extensions().GetByID(site_url.host());
  extension_webkit_preferences::SetPreferences(extension, web_prefs);
#endif
  return true;
}

void ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs(
    WebContents* web_contents,
    WebPreferences* web_prefs) {
  OverrideWebPreferencesAfterNavigation(web_contents, web_prefs);
}

void ChromeContentBrowserClientExtensionsPart::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  handler->AddHandlerPair(&ExtensionWebUI::HandleChromeURLOverride,
                          BrowserURLHandler::null_handler());
  handler->AddHandlerPair(BrowserURLHandler::null_handler(),
                          &ExtensionWebUI::HandleChromeURLOverrideReverse);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void ChromeContentBrowserClientExtensionsPart::
    GetAdditionalAllowedSchemesForFileSystem(
        std::vector<std::string>* additional_allowed_schemes) {
  additional_allowed_schemes->push_back(kExtensionScheme);
}

void ChromeContentBrowserClientExtensionsPart::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers->push_back(base::BindRepeating(
      MediaFileSystemBackend::AttemptAutoMountForURLRequest));
#endif
}

void ChromeContentBrowserClientExtensionsPart::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    download::QuarantineConnectionCallback quarantine_connection_callback,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  additional_backends->push_back(std::make_unique<MediaFileSystemBackend>(
      storage_partition_path, std::move(quarantine_connection_callback)));

  additional_backends->push_back(
      std::make_unique<sync_file_system::SyncFileSystemBackend>(
          Profile::FromBrowserContext(browser_context)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void ChromeContentBrowserClientExtensionsPart::
    AppendExtraRendererCommandLineSwitches(
        base::CommandLine* command_line,
        content::RenderProcessHost& process) {
  if (AreExtensionsDisabledForProfile(process.GetBrowserContext())) {
    return;
  }

  if (auto* extension = ProcessMap::Get(process.GetBrowserContext())
                            ->GetEnabledExtensionByProcessID(process.GetID())) {
    command_line->AppendSwitch(switches::kExtensionProcess);

    // Blink usually initializes the main-thread Isolate in background mode for
    // extension processes, assuming that they can't detect visibility. However,
    // mimehandler processes such as the PDF document viewer can indeed detect
    // visibility, and benefit from being started in foreground mode. We can
    // safely start those processes in foreground mode, knowing that
    // RenderThreadImpl::OnRendererHidden will be called when appropriate.
    if (base::Contains(MimeTypesHandler::GetMIMETypeAllowlist(),
                       extension->id())) {
      command_line->AppendSwitch(::switches::kInitIsolateAsForeground);
    }
  }
}

}  // namespace extensions
