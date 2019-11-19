// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/extensions/chrome_extension_test_notification_observer.h>
#include "base/bind.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

// A callback that returns true if the condition has been met and takes no
// arguments.
using ConditionCallback = base::Callback<bool(void)>;

bool HasPageActionVisibilityReachedTarget(
    Browser* browser,
    size_t target_visible_page_action_count) {
  return extension_action_test_util::GetVisiblePageActionCount(
             browser->tab_strip_model()->GetActiveWebContents()) ==
         target_visible_page_action_count;
}

bool HaveAllExtensionRenderFrameHostsFinishedLoading(ProcessManager* manager) {
  ProcessManager::FrameSet all_views = manager->GetAllFrames();
  for (content::RenderFrameHost* host : manager->GetAllFrames()) {
    if (content::WebContents::FromRenderFrameHost(host)->IsLoading())
      return false;
  }
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ChromeExtensionTestNotificationObserver::
    ChromeExtensionTestNotificationObserver(Browser* browser)
    : ExtensionTestNotificationObserver(browser ? browser->profile() : nullptr),
      browser_(browser) {}

ChromeExtensionTestNotificationObserver::
    ChromeExtensionTestNotificationObserver(content::BrowserContext* context)
    : ExtensionTestNotificationObserver(context), browser_(nullptr) {}

ChromeExtensionTestNotificationObserver::
    ~ChromeExtensionTestNotificationObserver() {}

content::BrowserContext*
ChromeExtensionTestNotificationObserver::GetBrowserContext() {
  if (!context_) {
    if (browser_)
      context_ = browser_->profile();
    else
      context_ = ProfileManager::GetActiveUserProfile();
  }
  return context_;
}

bool ChromeExtensionTestNotificationObserver::
    WaitForPageActionVisibilityChangeTo(int count) {
  DCHECK(browser_);
  ScopedObserver<ExtensionActionAPI, ExtensionActionAPI::Observer> observer(
      this);
  observer.Add(ExtensionActionAPI::Get(GetBrowserContext()));
  WaitForCondition(
      base::Bind(&HasPageActionVisibilityReachedTarget, browser_, count), NULL);
  return true;
}

bool ChromeExtensionTestNotificationObserver::WaitForExtensionViewsToLoad() {
  // Some views might not be created yet. This call may become insufficient if
  // e.g. implementation of ExtensionHostQueue changes.
  base::RunLoop().RunUntilIdle();

  ProcessManager* manager = ProcessManager::Get(GetBrowserContext());
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  notification_set.Add(content::NOTIFICATION_LOAD_STOP);
  notification_set.AddExtensionFrameUnregistration(manager);
  WaitForCondition(
      base::Bind(&HaveAllExtensionRenderFrameHostsFinishedLoading, manager),
      &notification_set);
  return true;
}

bool ChromeExtensionTestNotificationObserver::WaitForExtensionIdle(
    const std::string& extension_id) {
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  WaitForCondition(
      base::Bind(&util::IsExtensionIdle, extension_id, GetBrowserContext()),
      &notification_set);
  return true;
}

bool ChromeExtensionTestNotificationObserver::WaitForExtensionNotIdle(
    const std::string& extension_id) {
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_LOAD_STOP);
  WaitForCondition(base::Bind(
                       [](const std::string& extension_id,
                          content::BrowserContext* context) -> bool {
                         return !util::IsExtensionIdle(extension_id, context);
                       },
                       extension_id, GetBrowserContext()),
                   &notification_set);
  return true;
}

void ChromeExtensionTestNotificationObserver::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  MaybeQuit();
}

}  // namespace extensions
