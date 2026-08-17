// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "ui/base/base_window.h"

namespace {

const int64_t kDestroyProfileTimeoutSeconds = 60;

void DestroyOTRProfileWhenAppropriate(base::WeakPtr<Profile> weak_profile) {
  if (Profile* profile = weak_profile.get()) {
    ProfileDestroyer::DestroyOTRProfileWhenAppropriateWithTimeout(
        profile, base::Seconds(kDestroyProfileTimeoutSeconds));
  }
}

#if BUILDFLAG(IS_ANDROID)
void CloseAndroidTabsForProfile(Profile* profile) {
  // Closing the last incognito tab can synchronously destroy its TabModel and
  // remove it from TabModelList. Iterate by reverse index so removing the
  // current model does not invalidate the remaining traversal.
  for (size_t model_index = TabModelList::models().size(); model_index > 0;) {
    TabModel* model = TabModelList::models()[--model_index];
    for (int tab_index = model->GetTabCount(); tab_index > 0;) {
      TabAndroid* tab = model->GetTabAt(--tab_index);
      if (tab && tab->profile() == profile) {
        model->CloseTabAt(tab_index);
      }
    }
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

DevToolsBrowserContextManager::DevToolsBrowserContextManager() = default;

DevToolsBrowserContextManager::~DevToolsBrowserContextManager() = default;

// static
DevToolsBrowserContextManager& DevToolsBrowserContextManager::GetInstance() {
  static base::NoDestructor<DevToolsBrowserContextManager> instance;
  return *instance;
}

Profile* DevToolsBrowserContextManager::GetProfileById(
    const std::string& context_id) {
  Profile* default_profile =
      ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  if (context_id == default_profile->UniqueId()) {
    return default_profile;
  }
  auto it = otr_profiles_.find(context_id);
  if (it == otr_profiles_.end())
    return nullptr;
  return it->second;
}

content::BrowserContext* DevToolsBrowserContextManager::CreateBrowserContext() {
  Profile* original_profile =
      ProfileManager::GetLastUsedProfile()->GetOriginalProfile();

  Profile* otr_profile = original_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForDevTools(),
      /*create_if_needed=*/true);
  const std::string& context_id = otr_profile->UniqueId();

  // The two lines are matched in `StopObservingProfileIfAny()`.
  profile_observation_.AddObservation(otr_profile);
  otr_profiles_[context_id] = otr_profile;
  return otr_profile;
}

std::vector<base::WeakPtr<content::BrowserContext>>
DevToolsBrowserContextManager::GetBrowserContexts() {
  std::vector<base::WeakPtr<content::BrowserContext>> result;
  for (const auto& profile_pair : otr_profiles_)
    result.push_back(profile_pair.second->GetWeakPtr());
  return result;
}

content::BrowserContext*
DevToolsBrowserContextManager::GetDefaultBrowserContext() {
  // Do not force profile loading (or it will blow up if called on shutdown).
  auto* last_profile = ProfileManager::GetLastUsedProfileIfLoaded();
  return last_profile ? last_profile->GetOriginalProfile() : nullptr;
}

void DevToolsBrowserContextManager::DisposeBrowserContext(
    content::BrowserContext* context,
    content::DevToolsManagerDelegate::DisposeCallback callback) {
  std::string context_id = context->UniqueId();
  if (pending_context_disposals_.find(context_id) !=
      pending_context_disposals_.end()) {
    std::move(callback).Run(false, "Disposal of browser context " + context_id +
                                       " is already pending");
    return;
  }
  auto it = otr_profiles_.find(context_id);
  if (it == otr_profiles_.end()) {
    std::move(callback).Run(
        false, "Failed to find browser context with id " + context_id);
    return;
  }

  Profile* profile = it->second;
#if BUILDFLAG(IS_ANDROID)
  CloseAndroidTabsForProfile(profile);
  StopObservingProfileIfAny(profile);
  DestroyOTRProfileWhenAppropriate(profile->GetWeakPtr());
  std::move(callback).Run(true, "");
  return;
#else
  bool has_opened_browser = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile,
       &has_opened_browser](BrowserWindowInterface* browser_window_interface) {
        if (browser_window_interface->GetProfile() == profile) {
          has_opened_browser = true;
          return false;
        }
        return true;
      });

  // If no browsers are opened - dispose right away.
  if (!has_opened_browser) {
    StopObservingProfileIfAny(profile);
    DestroyOTRProfileWhenAppropriate(profile->GetWeakPtr());
    std::move(callback).Run(true, "");
    return;
  }

  if (pending_context_disposals_.empty()) {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  pending_context_disposals_[context_id] = std::move(callback);
  chrome::CloseAllBrowsersWithIncognitoProfile(profile);
#endif  // BUILDFLAG(IS_ANDROID)
}

void DevToolsBrowserContextManager::OnProfileWillBeDestroyed(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  CloseAndroidTabsForProfile(profile);
#else
  // This is likely happening during shutdown. We'll immediately
  // close all browser windows for our profile without unload handling.
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile](BrowserWindowInterface* browser_window_interface) {
        if (browser_window_interface->GetProfile() == profile) {
          browser_window_interface->GetWindow()->Close();
        }
        return true;
      });
#endif  // BUILDFLAG(IS_ANDROID)

  StopObservingProfileIfAny(profile);
}

void DevToolsBrowserContextManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  std::string context_id = browser->GetProfile()->UniqueId();
  auto pending_disposal = pending_context_disposals_.find(context_id);
  if (pending_disposal == pending_context_disposals_.end())
    return;
  bool found = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [browser, &found](BrowserWindowInterface* browser_window_interface) {
        if (browser_window_interface->GetProfile() == browser->GetProfile()) {
          found = true;
          return false;
        }
        return true;
      });
  if (found) {
    return;
  }

  StopObservingProfileIfAny(browser->GetProfile());

  // We cannot delete immediately here: the profile might still be referenced
  // during the browser tear-down process.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DestroyOTRProfileWhenAppropriate,
                                browser->GetProfile()->GetWeakPtr()));

  std::move(pending_disposal->second).Run(true, "");
  pending_context_disposals_.erase(pending_disposal);
  if (pending_context_disposals_.empty()) {
    browser_collection_observation_.Reset();
  }
}

void DevToolsBrowserContextManager::StopObservingProfileIfAny(
    Profile* profile) {
  if (!profile_observation_.IsObservingSource(profile))
    return;

  profile_observation_.RemoveObservation(profile);
  otr_profiles_.erase(profile->UniqueId());
}
