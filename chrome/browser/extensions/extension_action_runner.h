// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
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

  // Runs the given extension action. This may trigger a number of different
  // behaviors, depending on the extension and state, including:
  // - Running blocked actions (if the extension had withheld permissions)
  // - Firing the action.onClicked event for the extension
  // - Determining that a UI action should be taken, indicated by the return
  //   result.
  // If `grant_tab_permissions` is true and the action is appropriate, this will
  // grant tab permissions for the extension to the active tab. This may not
  // happen in all cases (such as when showing a side panel).
  ExtensionAction::ShowAction RunAction(const Extension* extension,
                                        bool grant_tab_permissions);

  // Runs any actions that were blocked for the given `extension`. As a
  // requirement, this will grant activeTab permission to the extension.
  void RunBlockedActions(const Extension* extension);

  // Grants activeTab to `extensions` (this should only be done if this is
  // through a direct user action). The permission will be applied immediately.
  // If any extension needs a page refresh to run, this will show a dialog as
  // well.
  void GrantTabPermissions(const std::vector<const Extension*>& extensions);

  // TODO(crbug.com/40883928): Move the reload bubble outside of
  // `ExtensionActionRunner` as it is no longer tied to running an action. See
  // if it can be merged with extensions dialogs utils `ShowReloadPageDialog`.
  // Shows the bubble to prompt the user to refresh the page to run or not the
  // action for the given `extension_ids`.
  void ShowReloadPageBubble(const std::vector<ExtensionId>& extension_ids);

  // Notifies the ExtensionActionRunner that an extension has been granted
  // active tab permissions. This will run any pending injections for that
  // extension.
  void OnActiveTabPermissionGranted(const Extension* extension);

  // Called when a webRequest event for the given |extension| was blocked.
  void OnWebRequestBlocked(const Extension* extension);

  // Returns a bitmask of BlockedActionType for the actions that have been
  // blocked for the given extension.
  int GetBlockedActions(const ExtensionId& extension_id) const;

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
      const ExtensionId& extension_id,
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
  using PendingScriptMap = std::map<ExtensionId, PendingScriptList>;

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

  // Reloads the current page.
  void OnReloadPageBubbleAccepted();

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
  std::set<ExtensionId> web_request_blocked_;

  // The extensions which have been granted permission to run on the given page.
  // TODO(rdevlin.cronin): Right now, this just keeps track of extensions that
  // have been permitted to run on the page via this interface. Instead, it
  // should incorporate more fully with ActiveTab.
  std::set<ExtensionId> permitted_extensions_;

  // If true, ignore active tab being granted rather than running pending
  // actions.
  bool ignore_active_tab_granted_;

  // If true, immediately accept the blocked action dialog by running the
  // callback.
  std::optional<bool> accept_bubble_for_testing_;

  raw_ptr<TestObserver> test_observer_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<ExtensionActionRunner> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
