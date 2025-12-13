// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function_details.h"

#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ChromeExtensionFunctionDetails::ChromeExtensionFunctionDetails(
    ExtensionFunction* function)
    : function_(function) {}

ChromeExtensionFunctionDetails::~ChromeExtensionFunctionDetails() = default;

WindowController* ChromeExtensionFunctionDetails::GetCurrentWindowController()
    const {
  // If the delegate has an associated window controller, return it.
  if (function_->dispatcher()) {
    if (WindowController* window_controller =
            function_->dispatcher()->GetExtensionWindowController()) {
      // Only return the found controller if it's not about to be deleted,
      // otherwise fall through to finding another one.
      if (!window_controller->IsDeleteScheduled()) {
        return window_controller;
      }
    }
  }

  // Otherwise, try to default to a reasonable browser. If |include_incognito_|
  // is true, we will also search browsers in the incognito version of this
  // profile. Note that the profile may already be incognito, in which case
  // we will search the incognito version only, regardless of the value of
  // |include_incognito|.
  Profile* profile = Profile::FromBrowserContext(function_->browser_context());

  WindowController* window_controller = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile ||
            (function_->include_incognito_information() &&
             browser->GetProfile()->GetOriginalProfile() == profile)) {
          window_controller = BrowserExtensionWindowController::From(browser);
          return false;  // Stop iterating.
        }
        return true;  // Continue iterating.
      });

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here, or
  // all of this profile's browser windows may have been closed.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return window_controller;
}

gfx::NativeWindow ChromeExtensionFunctionDetails::GetNativeWindowForUI() {
  // Try to use WindowControllerList first because WebContents's
  // GetTopLevelNativeWindow() can't return the top level window when the tab
  // is not focused.
  WindowController* controller =
      WindowControllerList::GetInstance()->CurrentWindowForFunction(function_);
  if (controller) {
    return controller->window()->GetNativeWindow();
  }

  // Next, check the sender web contents for if it supports modal dialogs.
  // TODO(devlin): This seems weird. Why wouldn't we check this first?
  content::WebContents* sender_web_contents = function_->GetSenderWebContents();
  if (sender_web_contents) {
#if BUILDFLAG(IS_ANDROID)
    bool supports_modal = !!sender_web_contents->GetTopLevelNativeWindow();
#else
    bool supports_modal =
        web_modal::WebContentsModalDialogManager::FromWebContents(
            sender_web_contents);
#endif
    if (supports_modal) {
      return sender_web_contents->GetTopLevelNativeWindow();
    }
  }

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  // Then, check for any app windows that are open.
  if (function_->extension() && function_->extension()->is_app()) {
    AppWindow* window =
        AppWindowRegistry::Get(function_->browser_context())
            ->GetCurrentAppWindowForApp(function_->extension()->id());
    if (window) {
      return window->web_contents()->GetTopLevelNativeWindow();
    }
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)

  // As a last resort, find a browser.
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  BrowserWindowInterface* browser = nullptr;
  Profile* profile = Profile::FromBrowserContext(function_->browser_context());
  for (auto* candidate : all_browsers) {
    if (candidate->GetProfile() == profile) {
      browser = candidate;
      break;
    }
  }
  if (browser) {
    return browser->GetWindow()->GetNativeWindow();
  }

  // If there are no browser windows open, no window is available.
  // This could happen e.g. if extension launches a long process or simple
  // sleep() in the background script, during which browser is closed.
  return gfx::NativeWindow();
}

}  // namespace extensions
