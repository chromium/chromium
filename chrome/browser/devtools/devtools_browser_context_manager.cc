// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

DevToolsBrowserContextManager::DevToolsBrowserContextManager() {}

DevToolsBrowserContextManager::~DevToolsBrowserContextManager() = default;

// static
DevToolsBrowserContextManager& DevToolsBrowserContextManager::GetInstance() {
  static base::NoDestructor<DevToolsBrowserContextManager> instance;
  return *instance;
}

Profile* DevToolsBrowserContextManager::GetProfileById(
    const std::string& context_id) {
  auto it = otr_profiles_.find(context_id);
  if (it == otr_profiles_.end())
    return nullptr;
  return it->second;
}

content::BrowserContext* DevToolsBrowserContextManager::CreateBrowserContext() {
  Profile* original_profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();

  Profile* otr_profile = original_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForDevTools());
  const std::string& context_id = otr_profile->UniqueId();
  otr_profiles_[context_id] = otr_profile;
  otr_profile->AddObserver(this);
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
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
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
  for (auto* opened_browser : *BrowserList::GetInstance()) {
    if (opened_browser->profile() == profile) {
      has_opened_browser = true;
      break;
    }
  }

  // If no browsers are opened - dispose right away.
  if (!has_opened_browser) {
    otr_profiles_.erase(it);
    profile->RemoveObserver(this);
    ProfileDestroyer::DestroyProfileWhenAppropriate(profile);
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
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile)
      browsers_to_close.push_back(browser);
  }
  for (auto* browser : browsers_to_close)
    browser->window()->Close();
  std::string context_id = profile->UniqueId();
  otr_profiles_.erase(context_id);
}

void DevToolsBrowserContextManager::OnBrowserRemoved(Browser* browser) {
  std::string context_id = browser->profile()->UniqueId();
  auto pending_disposal = pending_context_disposals_.find(context_id);
  if (pending_disposal == pending_context_disposals_.end())
    return;
  for (auto* opened_browser : *BrowserList::GetInstance()) {
    if (opened_browser->profile() == browser->profile())
      return;
  }

  auto it = otr_profiles_.find(context_id);
  Profile* otr_profile = it->second;
  otr_profiles_.erase(it);
  otr_profile->RemoveObserver(this);
  // We cannot delete immediately here: the profile might still be referenced
  // during the browser tear-down process.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProfileDestroyer::DestroyProfileWhenAppropriate,
                     base::Unretained(otr_profile)));

  std::move(pending_disposal->second).Run(true, "");
  pending_context_disposals_.erase(pending_disposal);
  if (pending_context_disposals_.empty())
    BrowserList::RemoveObserver(this);
}
