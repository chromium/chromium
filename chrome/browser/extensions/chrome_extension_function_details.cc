// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function_details.h"

#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"

ChromeExtensionFunctionDetails::ChromeExtensionFunctionDetails(
    ExtensionFunction* function)
    : function_(function) {}

ChromeExtensionFunctionDetails::~ChromeExtensionFunctionDetails() {
}

extensions::WindowController*
ChromeExtensionFunctionDetails::GetCurrentWindowController() const {
  // If the delegate has an associated window controller, return it.
  if (function_->dispatcher()) {
    if (extensions::WindowController* window_controller =
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
  // |include_incognito|. Look only for browsers on the active desktop as it is
  // preferable to pretend no browser is open then to return a browser on
  // another desktop.
  content::WebContents* web_contents = function_->GetSenderWebContents();
  Profile* profile = Profile::FromBrowserContext(
      web_contents ? web_contents->GetBrowserContext()
                   : function_->browser_context());
  Browser* browser = chrome::FindAnyBrowser(
      profile, function_->include_incognito_information());
  if (browser)
    return browser->extension_window_controller();

  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a background_page onload chrome.tabs api call can make it into here
  // before the browser is sufficiently initialized to return here, or
  // all of this profile's browser windows may have been closed.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return nullptr;
}

gfx::NativeWindow ChromeExtensionFunctionDetails::GetNativeWindowForUI() {
  // Try to use WindowControllerList first because WebContents's
  // GetTopLevelNativeWindow() can't return the top level window when the tab
  // is not focused.
  extensions::WindowController* controller =
      extensions::WindowControllerList::GetInstance()->CurrentWindowForFunction(
          function_);
  if (controller)
    return controller->window()->GetNativeWindow();

  // Next, check the sender web contents for if it supports modal dialogs.
  // TODO(devlin): This seems weird. Why wouldn't we check this first?
  content::WebContents* sender_web_contents = function_->GetSenderWebContents();
  if (sender_web_contents &&
      web_modal::WebContentsModalDialogManager::FromWebContents(
           sender_web_contents)) {
    return sender_web_contents->GetTopLevelNativeWindow();
  }

  // Then, check for any app windows that are open.
  if (function_->extension() &&
      function_->extension()->is_app()) {
    extensions::AppWindow* window =
        extensions::AppWindowRegistry::Get(function_->browser_context())
            ->GetCurrentAppWindowForApp(function_->extension()->id());
    if (window)
      return window->web_contents()->GetTopLevelNativeWindow();
  }

  // As a last resort, find a browser.
  Browser* browser = chrome::FindBrowserWithProfile(
      Profile::FromBrowserContext(function_->browser_context()));
  // If there are no browser windows open, no window is available.
  // This could happen e.g. if extension launches a long process or simple
  // sleep() in the background script, during which browser is closed.
  if (!browser)
    return nullptr;
  return browser->window()->GetNativeWindow();
}
