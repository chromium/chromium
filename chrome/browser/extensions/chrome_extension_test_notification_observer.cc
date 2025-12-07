// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

bool HasPageActionActivityReachedTarget(
    content::WebContents* web_contents,
    size_t target_visible_page_action_count) {
  return extension_action_test_util::GetActivePageActionCount(web_contents) ==
         target_visible_page_action_count;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ChromeExtensionTestNotificationObserver::
    ChromeExtensionTestNotificationObserver(content::BrowserContext* context)
    : ExtensionTestNotificationObserver(context) {}

ChromeExtensionTestNotificationObserver::
    ~ChromeExtensionTestNotificationObserver() = default;

content::BrowserContext*
ChromeExtensionTestNotificationObserver::GetBrowserContext() {
  if (!context_) {
    context_ = ProfileManager::GetLastUsedProfileIfLoaded();
  }
  return context_;
}

bool ChromeExtensionTestNotificationObserver::
    WaitForPageActionVisibilityChangeTo(content::WebContents* web_contents,
                                        int count) {
  base::ScopedObservation<ExtensionActionDispatcher,
                          ExtensionActionDispatcher::Observer>
      observer(this);
  observer.Observe(ExtensionActionDispatcher::Get(GetBrowserContext()));
  WaitForCondition(base::BindRepeating(&HasPageActionActivityReachedTarget,
                                       web_contents, count),
                   /*notification_set=*/nullptr);
  return true;
}

void ChromeExtensionTestNotificationObserver::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  MaybeQuit();
}

}  // namespace extensions
