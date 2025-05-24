// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace {

constexpr char kOpenPopupError[] =
    "Failed to show popup either because there is an existing popup or another "
    "error occurred.";
constexpr char kFailedToOpenPopupGenericError[] = "Failed to open popup.";
constexpr char kNoActiveWindowFound[] =
    "Could not find an active browser window.";
constexpr char kNoActivePopup[] =
    "Extension does not have a popup on the active tab.";
constexpr char kOpenPopupInactiveWindow[] =
    "Cannot show popup for an inactive window. To show the popup for this "
    "window, first call `chrome.windows.update` with `focused` set to "
    "true.";

// Returns the browser that was last active in the given `profile`, optionally
// also checking the incognito profile.
Browser* FindLastActiveBrowserWindow(Profile* profile,
                                     bool check_incognito_profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile);

  if (browser && browser->window()->IsActive()) {
    return browser;  // Found an active browser.
  }

  // It's possible that the last active browser actually corresponds to the
  // associated incognito profile, and this won't be returned by
  // FindLastActiveWithProfile(). If the extension can operate incognito, then
  // check the last active incognito, too.
  if (check_incognito_profile && profile->HasPrimaryOTRProfile()) {
    Profile* incognito_profile =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/false);
    DCHECK(incognito_profile);
    Browser* incognito_browser =
        chrome::FindLastActiveWithProfile(incognito_profile);
    if (incognito_browser->window()->IsActive()) {
      return incognito_browser;
    }
  }

  return nullptr;
}

// Returns true if the given `extension` has an active popup on the active tab
// of `browser`.
bool HasPopupOnActiveTab(Browser* browser,
                         content::BrowserContext* browser_context,
                         const Extension& extension) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context)
          ->GetExtensionAction(extension);
  DCHECK(extension_action);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  return extension_action->HasPopup(tab_id) &&
         extension_action->GetIsVisibleIgnoringDeclarative(tab_id);
}

// Attempts to open `extension`'s popup in the given `browser`. Returns true on
// success; otherwise, populates `error` and returns false.
bool OpenPopupInBrowser(Browser& browser,
                        const Extension& extension,
                        std::string* error,
                        ShowPopupCallback callback) {
  if (!browser.SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
      !browser.window()->IsToolbarVisible()) {
    *error = "Browser window has no toolbar.";
    return false;
  }

  ExtensionsContainer* extensions_container =
      browser.window()->GetExtensionsContainer();
  // The ExtensionsContainer could be null if, e.g., this is a popup window with
  // no toolbar.
  // TODO(devlin): Is that still possible, given the checks above?
  if (!extensions_container ||
      !extensions_container->ShowToolbarActionPopupForAPICall(
          extension.id(), std::move(callback))) {
    *error = kFailedToOpenPopupGenericError;
    return false;
  }

  return true;
}

}  // namespace

ActionOpenPopupFunction::ActionOpenPopupFunction() = default;
ActionOpenPopupFunction::~ActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction ActionOpenPopupFunction::Run() {
  // TODO(crbug.com/360916928): Unfortunately, the action API types aren't
  // compiled. However, the bindings should still valid the form of the
  // arguments.
  EXTENSION_FUNCTION_VALIDATE(args().size() == 1u);
  EXTENSION_FUNCTION_VALIDATE(extension());
  const base::Value& options = args()[0];

  // TODO(crbug.com/40057101): Support specifying the tab ID? This is
  // kind of racy (because really what the extension probably cares about is
  // the document ID; tab ID persists across pages, whereas document ID would
  // detect things like navigations).
  int window_id = extension_misc::kCurrentWindowId;
  if (options.is_dict()) {
    const base::Value* window_value = options.GetDict().Find("windowId");
    if (window_value) {
      EXTENSION_FUNCTION_VALIDATE(window_value->is_int());
      window_id = window_value->GetInt();
    }
  }

  Browser* browser = nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  std::string error;
  if (window_id == extension_misc::kCurrentWindowId) {
    browser =
        FindLastActiveBrowserWindow(profile, include_incognito_information());
    if (!browser) {
      error = kNoActiveWindowFound;
    }
  } else {
    if (WindowController* controller =
            ExtensionTabUtil::GetControllerInProfileWithId(
                profile, window_id, include_incognito_information(), &error)) {
      browser = controller->GetBrowser();
    }
  }

  if (!browser) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  if (!browser->window()->IsActive()) {
    return RespondNow(Error(kOpenPopupInactiveWindow));
  }

  if (!HasPopupOnActiveTab(browser, browser_context(), *extension())) {
    return RespondNow(Error(kNoActivePopup));
  }

  if (!OpenPopupInBrowser(
          *browser, *extension(), &error,
          base::BindOnce(&ActionOpenPopupFunction::OnShowPopupComplete,
                         this))) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  // The function responds in OnShowPopupComplete(). Note that the function is
  // kept alive by the ref-count owned by the ShowPopupCallback.
  return RespondLater();
}

void ActionOpenPopupFunction::OnShowPopupComplete(ExtensionHost* popup_host) {
  DCHECK(!did_respond());

  if (popup_host) {
    // TODO(crbug.com/40057101): Return the tab for which the extension
    // popup was shown?
    DCHECK(popup_host->document_element_available());
    Respond(NoArguments());
  } else {
    // NOTE(devlin): We could have the callback pass more information here about
    // why the popup didn't open (e.g., another active popup vs popup closing
    // before display, as may happen if the window closes), but it's not clear
    // whether that would be significantly helpful to developers and it may
    // leak other information about the user's browser.
    Respond(Error(kFailedToOpenPopupGenericError));
  }
}

BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction() = default;
BrowserActionOpenPopupFunction::~BrowserActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction BrowserActionOpenPopupFunction::Run() {
  // We only allow the popup in the active window.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser =
      FindLastActiveBrowserWindow(profile, include_incognito_information());

  if (!browser) {
    return RespondNow(Error(kNoActiveWindowFound));
  }

  if (!HasPopupOnActiveTab(browser, browser_context(), *extension())) {
    return RespondNow(Error(kNoActivePopup));
  }

  std::string error;
  if (!OpenPopupInBrowser(*browser, *extension(), &error,
                          ShowPopupCallback())) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  // Even if this is for an incognito window, we want to use the normal profile.
  // If the extension is spanning, then extension hosts are created with the
  // original profile, and if it's split, then we know the api call came from
  // the right profile.
  host_registry_observation_.Observe(ExtensionHostRegistry::Get(profile));

  // Set a timeout for waiting for the notification that the popup is loaded.
  // Waiting is required so that the popup view can be retrieved by the custom
  // bindings for the response callback. It's also needed to keep this function
  // instance around until a notification is observed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserActionOpenPopupFunction::OpenPopupTimedOut, this),
      base::Seconds(10));
  return RespondLater();
}

void BrowserActionOpenPopupFunction::OnBrowserContextShutdown() {
  // No point in responding at this point (the context is gone). However, we
  // need to explicitly remove the ExtensionHostRegistry observation, since the
  // ExtensionHostRegistry's lifetime is tied to the BrowserContext. Otherwise,
  // this would cause a UAF when the observation is destructed as part of this
  // instance's destruction.
  host_registry_observation_.Reset();
}

void BrowserActionOpenPopupFunction::OpenPopupTimedOut() {
  if (did_respond()) {
    return;
  }

  DVLOG(1) << "chrome.browserAction.openPopup did not show a popup.";
  Respond(Error(kOpenPopupError));
}

void BrowserActionOpenPopupFunction::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  if (did_respond()) {
    return;
  }

  if (host->extension_host_type() != mojom::ViewType::kExtensionPopup ||
      host->extension()->id() != extension_->id()) {
    return;
  }

  Respond(NoArguments());
  host_registry_observation_.Reset();
}

}  // namespace extensions
