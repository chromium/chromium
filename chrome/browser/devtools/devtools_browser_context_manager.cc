// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

DevToolsBrowserContextManager::DevToolsBrowserContextManager() {}

DevToolsBrowserContextManager::~DevToolsBrowserContextManager() = default;

// static
DevToolsBrowserContextManager& DevToolsBrowserContextManager::GetInstance() {
  static base::NoDestructor<DevToolsBrowserContextManager> instance;
  return *instance;
}

Profile* DevToolsBrowserContextManager::GetProfileById(
    const std::string& context_id) {
  auto it = registrations_.find(context_id);
  if (it == registrations_.end())
    return nullptr;
  return it->second->profile();
}

content::BrowserContext* DevToolsBrowserContextManager::CreateBrowserContext() {
  Profile* original_profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();

  auto registration =
      IndependentOTRProfileManager::GetInstance()->CreateFromOriginalProfile(
          original_profile,
          base::BindOnce(
              &DevToolsBrowserContextManager::OnOriginalProfileDestroyed,
              weak_factory_.GetWeakPtr()));
  content::BrowserContext* context = registration->profile();
  const std::string& context_id = context->UniqueId();
  registrations_[context_id] = std::move(registration);
  return context;
}

std::vector<content::BrowserContext*>
DevToolsBrowserContextManager::GetBrowserContexts() {
  std::vector<content::BrowserContext*> result;
  for (const auto& registration_pair : registrations_)
    result.push_back(registration_pair.second->profile());
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
  auto it = registrations_.find(context_id);
  if (it == registrations_.end()) {
    std::move(callback).Run(
        false, "Failed to find browser context with id " + context_id);
    return;
  }

  Profile* profile = it->second->profile();
  bool has_opened_browser = false;
  for (auto* opened_browser : *BrowserList::GetInstance()) {
    if (opened_browser->profile() == profile) {
      has_opened_browser = true;
      break;
    }
  }

  // If no browsers are opened - dispose right away.
  if (!has_opened_browser) {
    registrations_.erase(it);
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

void DevToolsBrowserContextManager::OnOriginalProfileDestroyed(
    Profile* profile) {
  base::EraseIf(registrations_, [&profile](const auto& it) {
    return it.second->profile()->GetOriginalProfile() == profile;
  });
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
  auto it = registrations_.find(context_id);
  // We cannot delete immediately here: the profile might still be referenced
  // during the browser tier-down process.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  it->second.release());
  registrations_.erase(it);
  std::move(pending_disposal->second).Run(true, "");
  pending_context_disposals_.erase(pending_disposal);
  if (pending_context_disposals_.empty())
    BrowserList::RemoveObserver(this);
}
