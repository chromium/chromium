// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/enterprise_policy_delegate_impl.h"

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/shell.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash::personalization_app {

namespace {

ash::UserImageManagerImpl* GetUserImageManager(const Profile* profile) {
  return ash::UserImageManagerRegistry::Get()->GetManager(
      GetAccountId(profile));
}

}  // namespace

EnterprisePolicyDelegateImpl::EnterprisePolicyDelegateImpl(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  DCHECK(Shell::HasInstance());
  scoped_shell_observation_.Observe(Shell::Get());
  scoped_user_manager_observation_.Observe(user_manager::UserManager::Get());
  scoped_wallpaper_controller_observation_.Observe(WallpaperController::Get());
}

EnterprisePolicyDelegateImpl::~EnterprisePolicyDelegateImpl() = default;

bool EnterprisePolicyDelegateImpl::IsUserImageEnterpriseManaged() const {
  return GetUserImageManager(profile_)->IsUserImageManaged();
}

bool EnterprisePolicyDelegateImpl::IsWallpaperEnterpriseManaged() const {
  return WallpaperController::Get()->IsWallpaperControlledByPolicy(
      GetAccountId(profile_));
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
  if (user.GetAccountId() != GetAccountId(profile_)) {
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnUserImageIsEnterpriseManagedChanged(is_enterprise_managed);
  }
}

void EnterprisePolicyDelegateImpl::OnWallpaperChanged() {
  for (auto& observer : observer_list_) {
    observer.OnWallpaperIsEnterpriseManagedChanged(
        IsWallpaperEnterpriseManaged());
  }
}

void EnterprisePolicyDelegateImpl::OnShellDestroying() {
  // WallpaperController is about to be destroyed.
  scoped_wallpaper_controller_observation_.Reset();
  scoped_shell_observation_.Reset();
}

}  // namespace ash::personalization_app
