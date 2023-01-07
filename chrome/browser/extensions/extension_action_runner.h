// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class Extension;

// The provider for ExtensionActions corresponding to scripts which are actively
// running or need permission.
class ExtensionActionRunner : public content::WebContentsObserver,
                              public ExtensionRegistryObserver {
 public:
  class TestObserver {
   public:
    virtual void OnBlockedActionAdded() = 0;
  };

  explicit ExtensionActionRunner(content::WebContents* web_contents);

  ExtensionActionRunner(const ExtensionActionRunner&) = delete;
  ExtensionActionRunner& operator=(const ExtensionActionRunner&) = delete;

  ~ExtensionActionRunner() override;

  // Returns the ExtensionActionRunner for the given |web_contents|, or null
  // if one does not exist.
  static ExtensionActionRunner* GetForWebContents(
      content::WebContents* web_contents);

  // Executes the action for the given |extension| and |grant_tab_permissions|
  // if true. Returns any further action (like showing a popup) that should be
  // taken.
  ExtensionAction::ShowAction RunAction(const Extension* extension,
                                        bool grant_tab_permissions);

  // Grants activeTab to |extensions| (this should only be done if this is
  // through a direct user action). If any extension needs a page refresh to
  // run, this will show a dialog instead of immediately granting permissions.
  void GrantTabPermissions(const std::vector<const Extension*>& extensions);

  // Notifies the ExtensionActionRunner that the page access for |extension| has
  // changed.
  void HandlePageAccessModified(
      const Extension* extension,
      SitePermissionsHelper::SiteAccess current_access,
      SitePermissionsHelper::SiteAccess new_access);

  // Notifies the ExtensionActionRunner that the user site setting for `origin`
  // with `action_ids` has changed.
  void HandleUserSiteSettingModified(
      const base::flat_set<ToolbarActionsModel::ActionId>& action_ids,
      const url::Origin& origin,
      PermissionsManager::UserSiteSetting new_site_settings);

  // Notifies the ExtensionActionRunner that an extension has been granted
  // active tab permissions. This will run any pending injections for that
  // extension.
  void OnActiveTabPermissionGranted(const Extension* extension);

  // Called when a webRequest event for the given |extension| was blocked.
  void OnWebRequestBlocked(const Extension* extension);

  // Returns a bitmask of BlockedActionType for the actions that have been
  // blocked for the given extension.
  int GetBlockedActions(const ExtensionId& extension_id);

  // Returns true if the given |extension| has any blocked actions.
  bool WantsToRun(const Extension* extension);

  // Runs any blocked actions the extension has, but does not handle any page
  // refreshes for document_start/webRequest.
  void RunForTesting(const Extension* extension);

  int num_page_requests() const { return num_page_requests_; }

  void accept_bubble_for_testing(bool accept_bubble) {
    accept_bubble_for_testing_ = accept_bubble;
  }

  void set_observer_for_testing(TestObserver* observer) {
    test_observer_ = observer;
  }

  // Handles mojom::LocalFrameHost::RequestScriptInjectionPermission(). It
  // replies back with |callback|.
  void OnRequestScriptInjectionPermission(
      const std::string& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      mojom::LocalFrameHost::RequestScriptInjectionPermissionCallback callback);

  using ScriptInjectionCallback = base::OnceCallback<void(bool)>;

#if defined(UNIT_TEST)
  // Only used in tests.
  PermissionsData::PageAccess RequiresUserConsentForScriptInjectionForTesting(
      const Extension* extension,
      mojom::InjectionType type) {
    return RequiresUserConsentForScriptInjection(extension, type);
  }
  void RequestScriptInjectionForTesting(const Extension* extension,
                                        mojom::RunLocation run_location,
                                        ScriptInjectionCallback callback) {
    return RequestScriptInjection(extension, run_location, std::move(callback));
  }
  void ClearInjectionsForTesting(const Extension& extension) {
    pending_scripts_.erase(extension.id());
  }
#endif  // defined(UNIT_TEST)

  // The blocked actions that require a page refresh to run.
  static const int kRefreshRequiredActionsMask;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionActionRunnerFencedFrameBrowserTest,
                           DoNotResetExtensionActionRunner);

  struct PendingScript {
    PendingScript(mojom::RunLocation run_location,
                  ScriptInjectionCallback permit_script);
    PendingScript(const PendingScript&) = delete;
    PendingScript& operator=(const PendingScript&) = delete;
    ~PendingScript();

    // The run location that the script wants to inject at.
    mojom::RunLocation run_location;

    // The callback to run when the script is permitted by the user.
    ScriptInjectionCallback permit_script;
  };

  using PendingScriptList = std::vector<std::unique_ptr<PendingScript>>;
  using PendingScriptMap = std::map<std::string, PendingScriptList>;

  // Returns true if the extension requesting script injection requires
  // user consent. If this is true, the caller should then register a request
  // via RequestScriptInjection().
  PermissionsData::PageAccess RequiresUserConsentForScriptInjection(
      const Extension* extension,
      mojom::InjectionType type);

  // |callback|. The only assumption that can be made about when (or if)
  // |callback| is run is that, if it is run, it will run on the current page.
  void RequestScriptInjection(const Extension* extension,
                              mojom::RunLocation run_location,
                              ScriptInjectionCallback callback);

  // Runs any pending injections for the corresponding extension.
  void RunPendingScriptsForExtension(const Extension* extension);

  // Notifies the ExtensionActionAPI of a change (either that an extension now
  // wants permission to run, or that it has been run).
  void NotifyChange(const Extension* extension);

  // Log metrics.
  void LogUMA() const;

  // Shows the bubble to prompt the user to refresh the page to run or not the
  // action for the given |extension_ids|. |callback| is invoked when the
  // bubble is closed.
  void ShowReloadPageBubble(const std::vector<ExtensionId>& extension_ids,
                            base::OnceClosure callback);

  // Called when the reload page bubble is accepted. Grants one time site access
  // to `page_url` for each extension in `extension_ids`.
  void OnReloadPageBubbleAcceptedForGrantTabPermissions(
      const std::vector<ExtensionId>& extension_ids,
      const GURL& page_url);

  // Called when the reload page bubble is accepted. Updates site access of
  // `extension_id` from `current_access` to `new_access` for `page_url`.
  void OnReloadPageBubbleAcceptedForExtensionSiteAccessChange(
      const ExtensionId& extension_id,
      const GURL& page_url,
      SitePermissionsHelper::SiteAccess current_access,
      SitePermissionsHelper::SiteAccess new_access);

  // Called when the reload page bubble is accepted. Updates user site setting
  // of `origin` to `site_settings`.
  void OnReloadPageBubbleAcceptedForUserSiteSettingsChange(
      const url::Origin& origin,
      extensions::PermissionsManager::UserSiteSetting site_settings);

  // Handles permission changes necessary for page access modification of the
  // |extension|.
  void UpdatePageAccessSettings(
      const Extension* extension,
      SitePermissionsHelper::SiteAccess current_access,
      SitePermissionsHelper::SiteAccess new_access);

  // Runs any actions that were blocked for the given |extension|. As a
  // requirement, this will grant activeTab permission to the extension.
  void RunBlockedActions(const Extension* extension);

  // Returns true if the given `extension` needs a page refresh to run a blocked
  // action.
  bool PageNeedsRefreshToRun(const Extension* extension);

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Runs the callback from the pending script. Since the callback holds
  // RequestScriptInjectionPermissionCallback, it should be called before the
  // pending script is cleared. |granted| represents whether the script is
  // granted or not.
  void RunCallbackOnPendingScript(const PendingScriptList& list, bool granted);

  // The total number of requests from the renderer on the current page,
  // including any that are pending or were immediately granted.
  // Right now, used only in tests.
  int num_page_requests_;

  // The associated browser context.
  raw_ptr<content::BrowserContext> browser_context_;

  // Whether or not the feature was used for any extensions. This may not be the
  // case if the user never enabled the scripts-require-action flag.
  bool was_used_on_page_;

  // The map of extension_id:pending_request of all pending script requests.
  PendingScriptMap pending_scripts_;

  // A set of ids for which the webRequest API was blocked on the page.
  std::set<std::string> web_request_blocked_;

  // The extensions which have been granted permission to run on the given page.
  // TODO(rdevlin.cronin): Right now, this just keeps track of extensions that
  // have been permitted to run on the page via this interface. Instead, it
  // should incorporate more fully with ActiveTab.
  std::set<std::string> permitted_extensions_;

  // If true, ignore active tab being granted rather than running pending
  // actions.
  bool ignore_active_tab_granted_;

  // If true, immediately accept the blocked action dialog by running the
  // callback.
  absl::optional<bool> accept_bubble_for_testing_;

  raw_ptr<TestObserver> test_observer_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<ExtensionActionRunner> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
