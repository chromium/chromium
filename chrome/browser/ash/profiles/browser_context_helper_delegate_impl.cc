// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/browser_context_helper_delegate_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"

namespace ash {

BrowserContextHelperDelegateImpl::BrowserContextHelperDelegateImpl() = default;
BrowserContextHelperDelegateImpl::~BrowserContextHelperDelegateImpl() = default;

content::BrowserContext*
BrowserContextHelperDelegateImpl::GetBrowserContextByPath(
    const base::FilePath& path) {
  // profile_manager can be null in unit tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  return profile_manager->GetProfileByPath(path);
}

content::BrowserContext*
BrowserContextHelperDelegateImpl::GetBrowserContextByAccountId(
    const AccountId& account_id) {
  // profile_manager can be null in unit tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return nullptr;
  }

  for (auto* profile : profile_manager->GetLoadedProfiles()) {
    auto* annotated_id = AnnotatedAccountId::Get(profile);
    if (annotated_id && *annotated_id == account_id) {
      return profile;
    }
  }
  return nullptr;
}

content::BrowserContext*
BrowserContextHelperDelegateImpl::DeprecatedGetBrowserContext(
    const base::FilePath& path) {
  // profile_manager can be null in unit tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  return profile_manager->GetProfile(path);
}

content::BrowserContext*
BrowserContextHelperDelegateImpl::GetOrCreatePrimaryOTRBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
}

content::BrowserContext*
BrowserContextHelperDelegateImpl::GetOriginalBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetOriginalProfile();
}

const base::FilePath* BrowserContextHelperDelegateImpl::GetUserDataDir() {
  // profile_manager can be null in unit tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  return &profile_manager->user_data_dir();
}

}  // namespace ash
