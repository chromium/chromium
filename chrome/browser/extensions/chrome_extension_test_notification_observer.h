// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_

#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/extension_test_notification_observer.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

// Test helper class for observing extension-related events.
class ChromeExtensionTestNotificationObserver
    : public ExtensionTestNotificationObserver,
      public ExtensionActionDispatcher::Observer {
 public:
  explicit ChromeExtensionTestNotificationObserver(
      content::BrowserContext* browser_context);

  ChromeExtensionTestNotificationObserver(
      const ChromeExtensionTestNotificationObserver&) = delete;
  ChromeExtensionTestNotificationObserver& operator=(
      const ChromeExtensionTestNotificationObserver&) = delete;

  ~ChromeExtensionTestNotificationObserver() override;

  // Waits for the number of visible page actions for the tab for `web_contents`
  // to change to `count`.
  bool WaitForPageActionVisibilityChangeTo(content::WebContents* web_contents,
                                           int count);

 private:
  content::BrowserContext* GetBrowserContext();

  // ExtensionActionDispatcher::Observer:
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
