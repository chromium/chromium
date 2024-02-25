// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

namespace {

const int64_t kDestroyProfileTimeoutSeconds = 60;

void DestroyOTRProfileWhenAppropriate(base::WeakPtr<Profile> weak_profile) {
  if (Profile* profile = weak_profile.get()) {
    ProfileDestroyer::DestroyOTRProfileWhenAppropriateWithTimeout(
        profile, base::Seconds(kDestroyProfileTimeoutSeconds));
  }
}

}  // namespace

DevToolsBrowserContextManager::DevToolsBrowserContextManager() {}

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

std::vector<content::BrowserContext*>
DevToolsBrowserContextManager::GetBrowserContexts() {
  std::vector<content::BrowserContext*> result;
  for (const auto& profile_pair : otr_profiles_)
    result.push_back(profile_pair.second);
  return result;
}

content::BrowserContext*
DevToolsBrowserContextManager::GetDefaultBrowserContext() {
  return ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
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
  bool has_opened_browser = false;
  for (Browser* opened_browser : *BrowserList::GetInstance()) {
    if (opened_browser->profile() == profile) {
      has_opened_browser = true;
      break;
    }
  }

  // If no browsers are opened - dispose right away.
  if (!has_opened_browser) {
    StopObservingProfileIfAny(profile);
    DestroyOTRProfileWhenAppropriate(profile->GetWeakPtr());
    std::move(callback).Run(true, "");
    return;
  }

  if (pending_context_disposals_.empty())
    BrowserList::AddObserver(this);

  pending_context_disposals_[context_id] = std::move(callback);
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      profile, base::DoNothing(), base::DoNothing(),
      true /* skip_beforeunload */);
}

void DevToolsBrowserContextManager::OnProfileWillBeDestroyed(Profile* profile) {
  // This is likely happening during shutdown. We'll immediately
  // close all browser windows for our profile without unload handling.
  BrowserList::BrowserVector browsers_to_close;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile)
      browsers_to_close.push_back(browser);
  }
  for (Browser* browser : browsers_to_close) {
    browser->window()->Close();
  }

  StopObservingProfileIfAny(profile);
}

void DevToolsBrowserContextManager::OnBrowserRemoved(Browser* browser) {
  std::string context_id = browser->profile()->UniqueId();
  auto pending_disposal = pending_context_disposals_.find(context_id);
  if (pending_disposal == pending_context_disposals_.end())
    return;
  for (Browser* opened_browser : *BrowserList::GetInstance()) {
    if (opened_browser->profile() == browser->profile())
      return;
  }

  StopObservingProfileIfAny(browser->profile());

  // We cannot delete immediately here: the profile might still be referenced
  // during the browser tear-down process.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DestroyOTRProfileWhenAppropriate,
                                browser->profile()->GetWeakPtr()));

  std::move(pending_disposal->second).Run(true, "");
  pending_context_disposals_.erase(pending_disposal);
  if (pending_context_disposals_.empty())
    BrowserList::RemoveObserver(this);
}

void DevToolsBrowserContextManager::StopObservingProfileIfAny(
    Profile* profile) {
  if (!profile_observation_.IsObservingSource(profile))
    return;

  profile_observation_.RemoveObservation(profile);
  otr_profiles_.erase(profile->UniqueId());
}
