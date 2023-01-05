// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/browser_context_helper_delegate_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

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

const base::FilePath* BrowserContextHelperDelegateImpl::GetUserDataDir() {
  // profile_manager can be null in unit tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  return &profile_manager->user_data_dir();
}

}  // namespace ash
