// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/crx_file/id_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/origin.h"

namespace {

std::vector<extensions::ExtensionId> GetExtensionIds(
    std::vector<const extensions::Extension*> extensions) {
  std::vector<extensions::ExtensionId> extension_ids;
  extension_ids.reserve(extensions.size());
  for (auto* extension : extensions) {
    extension_ids.push_back(extension->id());
  }
  return extension_ids;
}

}  // namespace

namespace extensions {

ExtensionActionRunner::PendingScript::PendingScript(
    mojom::RunLocation run_location,
    ScriptInjectionCallback permit_script)
    : run_location(run_location), permit_script(std::move(permit_script)) {}

ExtensionActionRunner::PendingScript::~PendingScript() = default;

ExtensionActionRunner::ExtensionActionRunner(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      num_page_requests_(0),
      browser_context_(web_contents->GetBrowserContext()),
      was_used_on_page_(false),
      ignore_active_tab_granted_(false),
      test_observer_(nullptr) {
  CHECK(web_contents);
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
}

ExtensionActionRunner::~ExtensionActionRunner() {
  LogUMA();
}

// static
ExtensionActionRunner* ExtensionActionRunner::GetForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  return tab_helper ? tab_helper->extension_action_runner() : nullptr;
}

ExtensionAction::ShowAction ExtensionActionRunner::RunAction(
    const Extension* extension,
    bool grant_tab_permissions) {
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();

  if (grant_tab_permissions && GetBlockedActions(extension->id())) {
    // If the extension had blocked actions before granting tab permissions,
    // granting active tab will have run the extension. Don't execute further
    // since clicking should run blocked actions *or* the normal extension
    // action, not both.
    GrantTabPermissions({extension});
    return ExtensionAction::ShowAction::kNone;
  }

  // Anything that gets here should have a page or browser action, or toggle the
  // extension's side panel, and not blocked actions.
  // This method is only called to execute an action by the user, so we can
  // grant tab permissions unless `action` will toggle the side panel. Tab
  // permissions are not granted in this case because:
  //  - the extension's side panel entry can be opened through the side panel
  //    itself which does not grant tab permissions
  //  - extension side panels can persist through tab changes and so
  //  permissions
  //    granted for one tab shouldn't persist on that side panel across tab
  //    changes.
  // TODO(crbug.com/40904917): Evaluate if this is the best course of action.
  SidePanelService* side_panel_service =
      SidePanelService::Get(browser_context_);
  if (side_panel_service &&
      side_panel_service->HasSidePanelActionForTab(*extension, tab_id)) {
    return ExtensionAction::ShowAction::kToggleSidePanel;
  }

  if (grant_tab_permissions) {
    GrantTabPermissions({extension});
  }

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)
          ->GetExtensionAction(*extension);
  DCHECK(extension_action);

  if (!extension_action->GetIsVisible(tab_id)) {
    return ExtensionAction::ShowAction::kNone;
  }

  if (extension_action->HasPopup(tab_id)) {
    return ExtensionAction::ShowAction::kShowPopup;
  }

  ExtensionActionAPI::Get(browser_context_)
      ->DispatchExtensionActionClicked(*extension_action, web_contents(),
                                       extension);
  return ExtensionAction::ShowAction::kNone;
}

// TODO(crbug.com/40883928): Consider moving this to SitePermissionsHelper since
// it's more about permissions than running an action.
void ExtensionActionRunner::GrantTabPermissions(
    const std::vector<const Extension*>& extensions) {
  SitePermissionsHelper permissions_helper(
      Profile::FromBrowserContext(browser_context_));
  bool refresh_required = base::ranges::any_of(
      extensions, [this, &permissions_helper](const Extension* extension) {
        return permissions_helper.PageNeedsRefreshToRun(
            GetBlockedActions(extension->id()));
      });

  // If a refresh is required this prevents blocked actions (that wouldn't run
  // at the right time) from running until the user refreshes the page.
  base::AutoReset<bool> ignore_active_tab(&ignore_active_tab_granted_,
                                          refresh_required);
  // Immediately grant permissions to every extension.
  for (auto* extension : extensions) {
    TabHelper::FromWebContents(web_contents())
        ->active_tab_permission_granter()
        ->GrantIfRequested(extension);
  }

  if (!refresh_required) {
    return;
  }

  // Every extension that was granted tab permission should currently have
  // "on click" site access, but extension actions are still blocked as page
  // hasn't been refreshed yet.
  const GURL& url = web_contents()->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(browser_context_);
  DCHECK(base::ranges::all_of(
      extensions, [url, &permissions_manager](const Extension* extension) {
        return permissions_manager->GetUserSiteAccess(*extension, url) ==
               PermissionsManager::UserSiteAccess::kOnClick;
      }));

  std::vector<ExtensionId> extension_ids = GetExtensionIds(extensions);
  ShowReloadPageBubble(extension_ids);
}

void ExtensionActionRunner::OnActiveTabPermissionGranted(
    const Extension* extension) {
  if (ignore_active_tab_granted_) {
    return;
  }

  if (WantsToRun(extension)) {
    RunBlockedActions(extension);
  }
}

void ExtensionActionRunner::OnWebRequestBlocked(const Extension* extension) {
  bool inserted = false;
  std::tie(std::ignore, inserted) =
      web_request_blocked_.insert(extension->id());
  if (inserted) {
    NotifyChange(extension);
  }

  if (test_observer_) {
    test_observer_->OnBlockedActionAdded();
  }
}

int ExtensionActionRunner::GetBlockedActions(
    const ExtensionId& extension_id) const {
  int blocked_actions = BLOCKED_ACTION_NONE;
  if (web_request_blocked_.count(extension_id) != 0) {
    blocked_actions |= BLOCKED_ACTION_WEB_REQUEST;
  }
  auto iter = pending_scripts_.find(extension_id);
  if (iter != pending_scripts_.end()) {
    for (const auto& script : iter->second) {
      switch (script->run_location) {
        case mojom::RunLocation::kDocumentStart:
          blocked_actions |= BLOCKED_ACTION_SCRIPT_AT_START;
          break;
        case mojom::RunLocation::kDocumentEnd:
        case mojom::RunLocation::kDocumentIdle:
        case mojom::RunLocation::kBrowserDriven:
          blocked_actions |= BLOCKED_ACTION_SCRIPT_OTHER;
          break;
        case mojom::RunLocation::kUndefined:
        case mojom::RunLocation::kRunDeferred:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

  return blocked_actions;
}

bool ExtensionActionRunner::WantsToRun(const Extension* extension) {
  return GetBlockedActions(extension->id()) != BLOCKED_ACTION_NONE;
}

void ExtensionActionRunner::RunForTesting(const Extension* extension) {
  if (WantsToRun(extension)) {
    TabHelper::FromWebContents(web_contents())
        ->active_tab_permission_granter()
        ->GrantIfRequested(extension);
  }
}

PermissionsData::PageAccess
ExtensionActionRunner::RequiresUserConsentForScriptInjection(
    const Extension* extension,
    mojom::InjectionType type) {
  CHECK(extension);

  // Allow the extension if it's been explicitly granted permission.
  if (permitted_extensions_.count(extension->id()) > 0) {
    return PermissionsData::PageAccess::kAllowed;
  }

  GURL url = web_contents()->GetVisibleURL();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  switch (type) {
    case mojom::InjectionType::kContentScript:
      return extension->permissions_data()->GetContentScriptAccess(url, tab_id,
                                                                   nullptr);
    case mojom::InjectionType::kProgrammaticScript:
      return extension->permissions_data()->GetPageAccess(url, tab_id, nullptr);
  }

  NOTREACHED_IN_MIGRATION();
  return PermissionsData::PageAccess::kDenied;
}

void ExtensionActionRunner::RequestScriptInjection(
    const Extension* extension,
    mojom::RunLocation run_location,
    ScriptInjectionCallback callback) {
  CHECK(extension);
  PendingScriptList& list = pending_scripts_[extension->id()];
  list.push_back(
      std::make_unique<PendingScript>(run_location, std::move(callback)));

  // If this was the first entry, we need to notify that a new extension wants
  // to run.
  if (list.size() == 1u) {
    NotifyChange(extension);
  }

  was_used_on_page_ = true;

  if (test_observer_) {
    test_observer_->OnBlockedActionAdded();
  }
}

void ExtensionActionRunner::RunPendingScriptsForExtension(
    const Extension* extension) {
  DCHECK(extension);

  content::NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();
  // Refuse to run if the visible entry is the initial NavigationEntry, because
  // we have no way of determining if it's the proper page. This should rarely,
  // if ever, happen.
  if (visible_entry->IsInitialEntry()) {
    return;
  }

  // We add this to the list of permitted extensions and erase pending entries
  // *before* running them to guard against the crazy case where running the
  // callbacks adds more entries.
  permitted_extensions_.insert(extension->id());

  auto iter = pending_scripts_.find(extension->id());
  if (iter == pending_scripts_.end()) {
    return;
  }

  PendingScriptList scripts;
  iter->second.swap(scripts);
  pending_scripts_.erase(extension->id());

  // Run all pending injections for the given extension.
  RunCallbackOnPendingScript(scripts, true);
}

void ExtensionActionRunner::OnRequestScriptInjectionPermission(
    const ExtensionId& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    mojom::LocalFrameHost::RequestScriptInjectionPermissionCallback callback) {
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    NOTREACHED_IN_MIGRATION() << "'" << extension_id << "' is not a valid id.";
    std::move(callback).Run(false);
    return;
  }

  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  // We shouldn't allow extensions which are no longer enabled to run any
  // scripts. Ignore the request.
  if (!extension) {
    std::move(callback).Run(false);
    return;
  }

  ++num_page_requests_;

  switch (RequiresUserConsentForScriptInjection(extension, script_type)) {
    case PermissionsData::PageAccess::kAllowed:
      std::move(callback).Run(true);
      break;
    case PermissionsData::PageAccess::kWithheld:
      RequestScriptInjection(extension, run_location, std::move(callback));
      break;
    case PermissionsData::PageAccess::kDenied:
      std::move(callback).Run(false);
      // We should usually only get a "deny access" if the page changed (as the
      // renderer wouldn't have requested permission if the answer was always
      // "no"). Just let the request fizzle and die.
      break;
  }
}

void ExtensionActionRunner::NotifyChange(const Extension* extension) {
  ExtensionActionAPI* extension_action_api =
      ExtensionActionAPI::Get(browser_context_);
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)
          ->GetExtensionAction(*extension);
  // If the extension has an action, we need to notify that it's updated.
  if (extension_action) {
    extension_action_api->NotifyChange(extension_action, web_contents(),
                                       browser_context_);
  }
}

void ExtensionActionRunner::LogUMA() const {
  // We only log the permitted extensions metric if the feature was used at all
  // on the page, because otherwise the data will be boring.
  if (was_used_on_page_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.PermittedExtensions",
        permitted_extensions_.size());
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.DeniedExtensions",
        pending_scripts_.size());
  }
}

void ExtensionActionRunner::ShowReloadPageBubble(
    const std::vector<ExtensionId>& extension_ids) {
  // For testing, simulate the bubble being accepted by directly invoking the
  // callback, or rejected by skipping the callback.
  if (accept_bubble_for_testing_.has_value()) {
    if (*accept_bubble_for_testing_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ExtensionActionRunner::OnReloadPageBubbleAccepted,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }

  // TODO(emiliapaz): Consider showing the dialog as a modal if container
  // doesn't exist. Currently we get the extension's icon via the action
  // controller from the container, so the container must exist.
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  ExtensionsContainer* const extensions_container =
      browser ? browser->window()->GetExtensionsContainer() : nullptr;
  if (!extensions_container) {
    return;
  }

  ShowReloadPageDialog(
      browser, extension_ids,
      base::BindOnce(&ExtensionActionRunner::OnReloadPageBubbleAccepted,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionActionRunner::OnReloadPageBubbleAccepted() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
}

void ExtensionActionRunner::RunBlockedActions(const Extension* extension) {
  DCHECK(base::Contains(pending_scripts_, extension->id()) ||
         web_request_blocked_.count(extension->id()) != 0);

  // Clicking to run the extension counts as granting it permission to run on
  // the given tab.
  // The extension may already have active tab at this point, but granting
  // it twice is essentially a no-op.
  TabHelper::FromWebContents(web_contents())
      ->active_tab_permission_granter()
      ->GrantIfRequested(extension);

  RunPendingScriptsForExtension(extension);
  web_request_blocked_.erase(extension->id());
}

void ExtensionActionRunner::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  declarative_net_request::RulesMonitorService* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context_);

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    if (rules_monitor_service && !navigation_handle->IsSameDocument()) {
      // Clean up any pending actions recorded in the action tracker for this
      // navigation.
      rules_monitor_service->action_tracker().ClearPendingNavigation(
          navigation_handle->GetNavigationId());
    }
    return;
  }

  LogUMA();
  num_page_requests_ = 0;
  permitted_extensions_.clear();
  // Runs all pending callbacks before clearing them.
  for (auto& scripts : pending_scripts_) {
    RunCallbackOnPendingScript(scripts.second, false);
  }
  pending_scripts_.clear();
  web_request_blocked_.clear();
  was_used_on_page_ = false;
  weak_factory_.InvalidateWeakPtrs();

  // Note: This needs to be called *after* the maps have been updated, so that
  // when the UI updates, this object returns the proper result for "wants to
  // run".
  ExtensionActionAPI::Get(browser_context_)
      ->ClearAllValuesForTab(web_contents());
  // |rules_monitor_service| can be null for some unit tests.
  if (rules_monitor_service) {
    int tab_id = ExtensionTabUtil::GetTabId(web_contents());
    declarative_net_request::ActionTracker& action_tracker =
        rules_monitor_service->action_tracker();
    action_tracker.ResetTrackedInfoForTab(tab_id,
                                          navigation_handle->GetNavigationId());
  }
}

void ExtensionActionRunner::WebContentsDestroyed() {
  ExtensionActionAPI::Get(browser_context_)
      ->ClearAllValuesForTab(web_contents());

  declarative_net_request::RulesMonitorService* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context_);

  // |rules_monitor_service| can be null for some unit tests.
  if (rules_monitor_service) {
    declarative_net_request::ActionTracker& action_tracker =
        rules_monitor_service->action_tracker();

    int tab_id = ExtensionTabUtil::GetTabId(web_contents());
    action_tracker.ClearTabData(tab_id);
  }
}

void ExtensionActionRunner::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  auto iter = pending_scripts_.find(extension->id());
  if (iter != pending_scripts_.end()) {
    PendingScriptList scripts;
    iter->second.swap(scripts);
    pending_scripts_.erase(iter);
    NotifyChange(extension);

    RunCallbackOnPendingScript(scripts, false);
  }
}

void ExtensionActionRunner::RunCallbackOnPendingScript(
    const PendingScriptList& list,
    bool granted) {
  // Calls RequestScriptInjectionPermissionCallback stored in
  // |pending_scripts_|.
  for (const auto& pending_script : list) {
    std::move(pending_script->permit_script).Run(granted);
  }
}

}  // namespace extensions
