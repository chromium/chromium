// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"

class DevToolsBrowserContextManager : public BrowserListObserver,
                                      public ProfileObserver {
 public:
  static DevToolsBrowserContextManager& GetInstance();

  DevToolsBrowserContextManager(const DevToolsBrowserContextManager&) = delete;
  DevToolsBrowserContextManager& operator=(
      const DevToolsBrowserContextManager&) = delete;

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

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Idempotent function ensuring the `profile` is not more observed, and
  // `otr_profiles_` is updated accordingly.
  void StopObservingProfileIfAny(Profile* profile);

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observation_{this};
  base::flat_map<std::string, raw_ptr<Profile, CtnExperimental>> otr_profiles_;
  base::flat_map<std::string, content::DevToolsManagerDelegate::DisposeCallback>
      pending_context_disposals_;

  base::WeakPtrFactory<DevToolsBrowserContextManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
