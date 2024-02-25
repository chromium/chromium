// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_notification_observer.h"

class Browser;

namespace content {
class BrowserContext;
}

namespace extensions {

// Test helper class for observing extension-related events.
class ChromeExtensionTestNotificationObserver
    : public ExtensionTestNotificationObserver,
      public ExtensionActionAPI::Observer {
 public:
  explicit ChromeExtensionTestNotificationObserver(Browser* browser);
  explicit ChromeExtensionTestNotificationObserver(
      content::BrowserContext* browser_context);

  ChromeExtensionTestNotificationObserver(
      const ChromeExtensionTestNotificationObserver&) = delete;
  ChromeExtensionTestNotificationObserver& operator=(
      const ChromeExtensionTestNotificationObserver&) = delete;

  ~ChromeExtensionTestNotificationObserver() override;

  // Waits for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

  // Waits for all extension views to load.
  bool WaitForExtensionViewsToLoad();

  // Waits for extension to be idle.
  bool WaitForExtensionIdle(const ExtensionId& extension_id);

  // Waits for extension to be not idle.
  bool WaitForExtensionNotIdle(const ExtensionId& extension_id);

 private:
  content::BrowserContext* GetBrowserContext();

  // ExtensionActionAPI::Observer:
  void OnExtensionActionUpdated(
      ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;

  const raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
