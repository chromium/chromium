// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/enterprise_policy_delegate_impl.h"

#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash::personalization_app {

namespace {

ash::UserImageManager* GetUserImageManager(const Profile* profile) {
  return ash::ChromeUserManager::Get()->GetUserImageManager(
      GetAccountId(profile));
}

}  // namespace

EnterprisePolicyDelegateImpl::EnterprisePolicyDelegateImpl(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  scoped_user_manager_observation_.Observe(user_manager::UserManager::Get());
}

EnterprisePolicyDelegateImpl::~EnterprisePolicyDelegateImpl() = default;

bool EnterprisePolicyDelegateImpl::IsUserImageEnterpriseManaged() const {
  return GetUserImageManager(profile_)->IsUserImageManaged();
}

void EnterprisePolicyDelegateImpl::AddObserver(
    EnterprisePolicyDelegate::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EnterprisePolicyDelegateImpl::RemoveObserver(
    EnterprisePolicyDelegate::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EnterprisePolicyDelegateImpl::OnUserImageIsEnterpriseManagedChanged(
    const user_manager::User& user,
    bool is_enterprise_managed) {
  // Filter out updates from other users.
  if (user.GetAccountId() != GetUser(profile_)->GetAccountId()) {
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnUserImageIsEnterpriseManagedChanged(is_enterprise_managed);
  }
}

}  // namespace ash::personalization_app
