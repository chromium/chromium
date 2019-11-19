// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace IPC {
class Message;
}

namespace extensions {
class Extension;

// The provider for ExtensionActions corresponding to scripts which are actively
// running or need permission.
class ExtensionActionRunner : public content::WebContentsObserver,
                              public ExtensionRegistryObserver {
 public:
  enum class PageAccess {
    RUN_ON_CLICK,
    RUN_ON_SITE,
    RUN_ON_ALL_SITES,
  };

  class TestObserver {
   public:
    virtual void OnBlockedActionAdded() = 0;
  };

  explicit ExtensionActionRunner(content::WebContents* web_contents);
  ~ExtensionActionRunner() override;

  // Returns the ExtensionActionRunner for the given |web_contents|, or null
  // if one does not exist.
  static ExtensionActionRunner* GetForWebContents(
      content::WebContents* web_contents);

  // Executes the action for the given |extension| and returns any further
  // action (like showing a popup) that should be taken. If
  // |grant_tab_permissions| is true, this will also grant activeTab to the
  // extension (so this should only be done if this is through a direct user
  // action).
  ExtensionAction::ShowAction RunAction(const Extension* extension,
                                        bool grant_tab_permissions);

  // Notifies the ExtensionActionRunner that the page access for |extension| has
  // changed.
  void HandlePageAccessModified(const Extension* extension,
                                PageAccess current_access,
                                PageAccess new_access);

  // Notifies the ExtensionActionRunner that an extension has been granted
  // active tab permissions. This will run any pending injections for that
  // extension.
  void OnActiveTabPermissionGranted(const Extension* extension);

  // Called when a webRequest event for the given |extension| was blocked.
  void OnWebRequestBlocked(const Extension* extension);

  // Returns a bitmask of BlockedActionType for the actions that have been
  // blocked for the given extension.
  int GetBlockedActions(const Extension* extension);

  // Returns true if the given |extension| has any blocked actions.
  bool WantsToRun(const Extension* extension);

  // Runs any blocked actions the extension has, but does not handle any page
  // refreshes for document_start/webRequest.
  void RunForTesting(const Extension* extension);

  int num_page_requests() const { return num_page_requests_; }

  void set_default_bubble_close_action_for_testing(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate::CloseAction> action) {
    default_bubble_close_action_for_testing_ = std::move(action);
  }
  void set_observer_for_testing(TestObserver* observer) {
    test_observer_ = observer;
  }

#if defined(UNIT_TEST)
  // Only used in tests.
  PermissionsData::PageAccess RequiresUserConsentForScriptInjectionForTesting(
      const Extension* extension,
      UserScript::InjectionType type) {
    return RequiresUserConsentForScriptInjection(extension, type);
  }
  void RequestScriptInjectionForTesting(const Extension* extension,
                                        UserScript::RunLocation run_location,
                                        const base::Closure& callback) {
    return RequestScriptInjection(extension, run_location, callback);
  }
  void ClearInjectionsForTesting(const Extension& extension) {
    pending_scripts_.erase(extension.id());
  }
#endif  // defined(UNIT_TEST)

 private:
  struct PendingScript {
    PendingScript(UserScript::RunLocation run_location,
                  const base::Closure& permit_script);
    PendingScript(const PendingScript& other);
    ~PendingScript();

    // The run location that the script wants to inject at.
    UserScript::RunLocation run_location;

    // The callback to run when the script is permitted by the user.
    base::Closure permit_script;
  };

  using PendingScriptList = std::vector<PendingScript>;
  using PendingScriptMap = std::map<std::string, PendingScriptList>;

  // Returns true if the extension requesting script injection requires
  // user consent. If this is true, the caller should then register a request
  // via RequestScriptInjection().
  PermissionsData::PageAccess RequiresUserConsentForScriptInjection(
      const Extension* extension,
      UserScript::InjectionType type);

  // |callback|. The only assumption that can be made about when (or if)
  // |callback| is run is that, if it is run, it will run on the current page.
  void RequestScriptInjection(const Extension* extension,
                              UserScript::RunLocation run_location,
                              const base::Closure& callback);

  // Runs any pending injections for the corresponding extension.
  void RunPendingScriptsForExtension(const Extension* extension);

  // Handle the RequestScriptInjectionPermission message.
  void OnRequestScriptInjectionPermission(const std::string& extension_id,
                                          UserScript::InjectionType script_type,
                                          UserScript::RunLocation run_location,
                                          int64_t request_id);

  // Grants permission for the given request to run.
  void PermitScriptInjection(int64_t request_id);

  // Notifies the ExtensionActionAPI of a change (either that an extension now
  // wants permission to run, or that it has been run).
  void NotifyChange(const Extension* extension);

  // Log metrics.
  void LogUMA() const;

  // Shows the bubble to prompt the user to refresh the page to run the blocked
  // actions for the given |extension|. |callback| is invoked when the bubble is
  // closed.
  void ShowBlockedActionBubble(
      const Extension* extension,
      const base::Callback<void(ToolbarActionsBarBubbleDelegate::CloseAction)>&
          callback);

  // Called when the blocked actions bubble invoked to run the extension action
  // is closed.
  void OnBlockedActionBubbleForRunActionClosed(
      const std::string& extension_id,
      ToolbarActionsBarBubbleDelegate::CloseAction action);

  // Called when the blocked actions bubble invoked for the page access grant is
  // closed.
  void OnBlockedActionBubbleForPageAccessGrantClosed(
      const std::string& extension_id,
      const GURL& page_url,
      PageAccess current_access,
      PageAccess new_access,
      ToolbarActionsBarBubbleDelegate::CloseAction action);

  // Handles permission changes necessary for page access modification of the
  // |extension|.
  void UpdatePageAccessSettings(const Extension* extension,
                                PageAccess current_access,
                                PageAccess new_access);

  // Runs any actions that were blocked for the given |extension|. As a
  // requirement, this will grant activeTab permission to the extension.
  void RunBlockedActions(const Extension* extension);

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // The total number of requests from the renderer on the current page,
  // including any that are pending or were immediately granted.
  // Right now, used only in tests.
  int num_page_requests_;

  // The associated browser context.
  content::BrowserContext* browser_context_;

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

  // If non-null, the bubble action to simulate for testing.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::CloseAction>
      default_bubble_close_action_for_testing_;

  TestObserver* test_observer_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<ExtensionActionRunner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionRunner);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
