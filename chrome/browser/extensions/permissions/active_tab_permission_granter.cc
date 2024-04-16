// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using RendererMessageFunction =
    base::RepeatingCallback<void(bool, content::RenderProcessHost*)>;

void UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                  const extensions::URLPatternSet& new_hosts,
                                  int tab_id,
                                  bool update_origin_allowlist,
                                  content::RenderProcessHost* process) {
  mojom::Renderer* renderer =
      RendererStartupHelperFactory::GetForBrowserContext(
          process->GetBrowserContext())
          ->GetRenderer(process);
  if (renderer) {
    renderer->UpdateTabSpecificPermissions(extension_id, new_hosts.Clone(),
                                           tab_id, update_origin_allowlist);
  }
}

void ClearTabSpecificPermissions(const std::vector<ExtensionId>& extension_ids,
                                 int tab_id,
                                 bool update_origin_allowlist,
                                 content::RenderProcessHost* process) {
  mojom::Renderer* renderer =
      RendererStartupHelperFactory::GetForBrowserContext(
          process->GetBrowserContext())
          ->GetRenderer(process);
  if (renderer) {
    renderer->ClearTabSpecificPermissions(extension_ids, tab_id,
                                          update_origin_allowlist);
  }
}

// Sends a message exactly once to each render process host owning one of the
// given |frame_hosts| and |tab_process|. If |tab_process| doesn't own any of
// the |frame_hosts|, it will not be signaled to update its origin allowlist.
void SendRendererMessageToProcesses(
    const std::set<content::RenderFrameHost*>& frame_hosts,
    content::RenderProcessHost* tab_process,
    const RendererMessageFunction& renderer_message) {
  std::set<content::RenderProcessHost*> sent_to_hosts;
  for (content::RenderFrameHost* frame_host : frame_hosts) {
    content::RenderProcessHost* process_host = frame_host->GetProcess();
    if (!base::Contains(sent_to_hosts, process_host)) {
      // Extension processes have to update the origin allowlists.
      renderer_message.Run(true, process_host);
      sent_to_hosts.insert(frame_host->GetProcess());
    }
  }
  // If the tab wasn't one of those processes already updated (it likely
  // wasn't), update it. Tabs don't need to update the origin allowlist.
  if (!base::Contains(sent_to_hosts, tab_process)) {
    renderer_message.Run(false, tab_process);
  }
}

void SetCorsOriginAccessList(content::BrowserContext* browser_context,
                             const Extension& extension,
                             base::OnceClosure closure) {
  // To limit how far the new permissions reach, we only apply them to the
  // ActiveTab's context for split-mode extensions.  OTOH, spanning-mode
  // extensions need to get the new permissions in all profiles (e.g. if the
  // ActiveTab is in an incognito window, than the [single/only/spanning]
  // background page in the regular profile also needs to get the new
  // permissions).
  NetworkPermissionsUpdater::ContextSet context_set =
      IncognitoInfo::IsSplitMode(&extension)
          ? NetworkPermissionsUpdater::ContextSet::kCurrentContextOnly
          : NetworkPermissionsUpdater::ContextSet::kAllRelatedContexts;
  NetworkPermissionsUpdater::UpdateExtension(*browser_context, extension,
                                             context_set, std::move(closure));
}

}  // namespace

ActiveTabPermissionGranter::ActiveTabPermissionGranter(
    content::WebContents* web_contents,
    int tab_id,
    Profile* profile)
    : content::WebContentsObserver(web_contents), tab_id_(tab_id) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile));
}

ActiveTabPermissionGranter::~ActiveTabPermissionGranter() {}

void ActiveTabPermissionGranter::GrantIfRequested(const Extension* extension) {
  if (granted_extensions_.Contains(extension->id())) {
    return;
  }

  APIPermissionSet new_apis;
  URLPatternSet new_hosts;

  const PermissionsData* permissions_data = extension->permissions_data();

  // Do not use `RFH::GetLastCommittedOrigin()` because it returns an empty
  // origin in case of a frame with CSP sandbox.
  const GURL& url = web_contents()->GetLastCommittedURL();

  // If the extension requested the host permission to |url| but had it
  // withheld, we grant it active tab-style permissions, even if it doesn't have
  // the activeTab permission in the manifest. This is necessary for the
  // runtime host permissions feature to work.
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  if (permissions_data->HasAPIPermission(mojom::APIPermissionID::kActiveTab) ||
      permissions_data->withheld_permissions().effective_hosts().MatchesURL(
          url)) {
    // Gate activeTab for file urls on extensions having explicit access to file
    // urls.
    int valid_schemes = UserScript::ValidUserScriptSchemes();
    if (!util::AllowFileAccess(extension->id(), browser_context)) {
      valid_schemes &= ~URLPattern::SCHEME_FILE;
    }
    new_hosts.AddOrigin(valid_schemes, url);
    new_apis.insert(mojom::APIPermissionID::kTab);

    if (permissions_data->HasAPIPermission(
            mojom::APIPermissionID::kDeclarativeNetRequest) ||
        permissions_data->HasAPIPermission(
            mojom::APIPermissionID::kDeclarativeNetRequestWithHostAccess)) {
      new_apis.insert(mojom::APIPermissionID::kDeclarativeNetRequestFeedback);
    }
  }

  if (permissions_data->HasAPIPermission(mojom::APIPermissionID::kTabCapture)) {
    new_apis.insert(mojom::APIPermissionID::kTabCaptureForTab);
  }

  if (!new_apis.empty() || !new_hosts.is_empty()) {
    granted_extensions_.Insert(extension);
    PermissionSet new_permissions(std::move(new_apis), ManifestPermissionSet(),
                                  new_hosts.Clone(), new_hosts.Clone());
    permissions_data->UpdateTabSpecificPermissions(tab_id_, new_permissions);
    SetCorsOriginAccessList(browser_context, *extension, base::DoNothing());

    if (web_contents()->GetController().GetVisibleEntry()) {
      ProcessManager* process_manager = ProcessManager::Get(browser_context);
      content::RenderProcessHost* process =
          web_contents()->GetPrimaryMainFrame()->GetProcess();

      // Notify ScriptInjectionTracker that scripts may be executed after
      // granting active tab.
      ScriptInjectionTracker::WillGrantActiveTab(
          base::PassKey<ActiveTabPermissionGranter>(), *extension, *process);

      // Update all extension render views with the new tab permissions, and
      // also the tab itself.
      RendererMessageFunction update_message =
          base::BindRepeating(&UpdateTabSpecificPermissions, extension->id(),
                              new_hosts.Clone(), tab_id_);
      SendRendererMessageToProcesses(
          process_manager->GetRenderFrameHostsForExtension(extension->id()),
          process, update_message);

      // It's important that this comes after the Mojo message is sent to the
      // renderer, so that any tasks executing in the renderer occur after it
      // has the updated permissions.
      ExtensionActionRunner::GetForWebContents(web_contents())
          ->OnActiveTabPermissionGranted(extension);

      auto* permissions_manager =
          PermissionsManager::Get(web_contents()->GetBrowserContext());
      permissions_manager->NotifyActiveTabPermisssionGranted(
          web_contents(), tab_id_, *extension);
    }
  }
}

void ActiveTabPermissionGranter::RevokeForTesting() {
  ClearGrantedExtensionsAndNotify();
}

void ActiveTabPermissionGranter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Important: sub-frames don't get granted!
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Only clear the granted permissions for cross-origin navigations.
  if (navigation_handle->IsSameOrigin()) {
    return;
  }

  ClearGrantedExtensionsAndNotify();
}

void ActiveTabPermissionGranter::WebContentsDestroyed() {
  ClearGrantedExtensionsAndNotify();
}

void ActiveTabPermissionGranter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Note: don't need to clear the permissions (nor tell the renderer about it)
  // because it's being unloaded anyway.
  granted_extensions_.Remove(extension->id());
}

void ActiveTabPermissionGranter::ClearGrantedExtensionsAndNotify() {
  ClearGrantedExtensionsAndNotify(granted_extensions_);
}

void ActiveTabPermissionGranter::ClearActiveExtensionAndNotify(
    const ExtensionId& id) {
  if (!granted_extensions_.Contains(id)) {
    return;
  }

  ExtensionSet granted_to_remove{};
  granted_to_remove.Insert(granted_extensions_.GetByID(id));
  ClearGrantedExtensionsAndNotify(granted_to_remove);
}

void ActiveTabPermissionGranter::ClearGrantedExtensionsAndNotify(
    const ExtensionSet& granted_extensions_to_remove) {
  if (granted_extensions_to_remove.empty()) {
    return;
  }

  std::set<content::RenderProcessHost*> extension_processes;
  std::vector<ExtensionId> extension_ids;
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  ProcessManager* process_manager = ProcessManager::Get(browser_context);
  for (const scoped_refptr<const Extension>& extension :
       granted_extensions_to_remove) {
    extension->permissions_data()->ClearTabSpecificPermissions(tab_id_);
    SetCorsOriginAccessList(browser_context, *extension, base::DoNothing());

    extension_ids.push_back(extension->id());
    for (content::RenderFrameHost* extension_frame_host :
         process_manager->GetRenderFrameHostsForExtension(extension->id())) {
      extension_processes.insert(extension_frame_host->GetProcess());
    }
  }

  // Notify active renders to clear tab permissions. We need to notify all
  // renderers because we notify of tab permissions on renderer creation, and a
  // previously-created (spare) renderer may be used for this tab in the future,
  // even if it isn't associated with the tab now (b/1923555).
  // TODO(b/325307774): only communicate the tab permissions to the renderers
  // that run in the given tab.
  for (content::RenderProcessHost::iterator host_iterator(
           content::RenderProcessHost::AllHostsIterator());
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    // Ignore renderers that are not ready.
    content::RenderProcessHost* process = host_iterator.GetCurrentValue();
    if (!process->IsInitializedAndNotDead()) {
      continue;
    }

    // Ignore renderers that aren't from the same profile.
    if (process->GetBrowserContext() != browser_context) {
      continue;
    }

    // Only extension processes need to update the origin allowlists.
    bool update_origin_allowlist = extension_processes.contains(process);
    ClearTabSpecificPermissions(extension_ids, tab_id_, update_origin_allowlist,
                                process);
  }

  for (const auto& id : extension_ids) {
    granted_extensions_.Remove(id);
  }
}

}  // namespace extensions
