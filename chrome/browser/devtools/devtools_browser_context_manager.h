// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/media/router/presentation/independent_otr_profile_manager.h"
#include "content/public/browser/devtools_manager_delegate.h"

class DevToolsBrowserContextManager : public BrowserListObserver {
 public:
  static DevToolsBrowserContextManager& GetInstance();

  Profile* GetProfileById(const std::string& browser_context_id);
  std::vector<content::BrowserContext*> GetBrowserContexts();
  content::BrowserContext* GetDefaultBrowserContext();
  content::BrowserContext* CreateBrowserContext();
  void DisposeBrowserContext(
      content::BrowserContext* context,
      content::DevToolsManagerDelegate::DisposeCallback callback);

 private:
  friend class base::NoDestructor<DevToolsBrowserContextManager>;
  DevToolsBrowserContextManager();
  ~DevToolsBrowserContextManager() override;
  void OnOriginalProfileDestroyed(Profile* profile);

  void OnBrowserRemoved(Browser* browser) override;

  base::flat_map<
      std::string,
      std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>>
      registrations_;
  base::flat_map<std::string, content::DevToolsManagerDelegate::DisposeCallback>
      pending_context_disposals_;

  base::WeakPtrFactory<DevToolsBrowserContextManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DevToolsBrowserContextManager);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
