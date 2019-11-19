// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/process_manager_delegate.h"

class Browser;
class Profile;

namespace extensions {

// Support for ProcessManager. Controls cases where Chrome wishes to disallow
// extension background pages or defer their creation.
class ChromeProcessManagerDelegate : public ProcessManagerDelegate,
                                     public content::NotificationObserver,
                                     public BrowserListObserver {
 public:
  ChromeProcessManagerDelegate();
  ~ChromeProcessManagerDelegate() override;

  // ProcessManagerDelegate:
  bool AreBackgroundPagesAllowedForContext(
      content::BrowserContext* context) const override;
  bool IsExtensionBackgroundPageAllowed(
      content::BrowserContext* context,
      const Extension& extension) const override;
  bool DeferCreatingStartupBackgroundHosts(
      content::BrowserContext* context) const override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  // Notification handlers.
  void OnProfileCreated(Profile* profile);
  void OnProfileDestroyed(Profile* profile);

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProcessManagerDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_
