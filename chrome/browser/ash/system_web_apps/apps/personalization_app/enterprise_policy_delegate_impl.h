// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_IMPL_H_

#include "ash/public/cpp/personalization_app/enterprise_policy_delegate.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

class Shell;
class WallpaperController;

namespace personalization_app {

class EnterprisePolicyDelegateImpl : public EnterprisePolicyDelegate,
                                     public user_manager::UserManager::Observer,
                                     public WallpaperControllerObserver,
                                     public ShellObserver {
 public:
  explicit EnterprisePolicyDelegateImpl(
      content::BrowserContext* browser_context);

  EnterprisePolicyDelegateImpl(const EnterprisePolicyDelegateImpl&) = delete;
  EnterprisePolicyDelegateImpl& operator=(const EnterprisePolicyDelegateImpl&) =
      delete;

  ~EnterprisePolicyDelegateImpl() override;

  // EnterprisePolicyDelegate:
  bool IsUserImageEnterpriseManaged() const override;
  bool IsWallpaperEnterpriseManaged() const override;
  void AddObserver(EnterprisePolicyDelegate::Observer* observer) override;
  void RemoveObserver(EnterprisePolicyDelegate::Observer* observer) override;

 private:
  // user_manager::UserManager::Observer:
  void OnUserImageIsEnterpriseManagedChanged(
      const user_manager::User& user,
      bool is_enterprise_managed) override;

  // WallpaperControllerObserver:
  void OnWallpaperChanged() override;

  // ShellObserver:
  void OnShellDestroying() override;

  raw_ptr<Profile> profile_;

  base::ScopedObservation<Shell, ShellObserver> scoped_shell_observation_{this};

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      scoped_user_manager_observation_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      scoped_wallpaper_controller_observation_{this};

  base::ObserverList<EnterprisePolicyDelegate::Observer> observer_list_;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_IMPL_H_
