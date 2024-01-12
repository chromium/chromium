// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace app_list {
class AppListSyncableService;
}  // namespace app_list

namespace ash {
namespace android_sms {

class AndroidSmsAppSetupController;
enum class PwaDomain;

class AndroidSmsAppManagerImpl : public AndroidSmsAppManager {
 public:
  // Thin wrapper around static PWA functions which is stubbed out for tests.
  class PwaDelegate {
   public:
    PwaDelegate();
    virtual ~PwaDelegate();
    virtual void OpenApp(Profile* profile, const std::string& app_id);
    virtual bool TransferItemAttributes(
        const std::string& from_app_id,
        const std::string& to_app_id,
        app_list::AppListSyncableService* app_list_syncable_service);
    virtual bool IsAppRegistryReady(Profile* profile);
    virtual void ExecuteOnAppRegistryReady(Profile* profile,
                                           base::OnceClosure task);
  };

  AndroidSmsAppManagerImpl(
      Profile* profile,
      AndroidSmsAppSetupController* setup_controller,
      PrefService* pref_service,
      app_list::AppListSyncableService* app_list_syncable_service,
      std::unique_ptr<PwaDelegate> test_pwa_delegate = nullptr);

  AndroidSmsAppManagerImpl(const AndroidSmsAppManagerImpl&) = delete;
  AndroidSmsAppManagerImpl& operator=(const AndroidSmsAppManagerImpl&) = delete;

  ~AndroidSmsAppManagerImpl() override;

 private:
  friend class AndroidSmsAppManagerImplTest;

  // AndroidSmsAppManager:
  std::optional<GURL> GetCurrentAppUrl() override;

  // AndroidSmsAppHelperDelegate:
  void SetUpAndroidSmsApp() override;
  void SetUpAndLaunchAndroidSmsApp() override;
  void TearDownAndroidSmsApp() override;
  bool IsAppInstalled() override;
  bool IsAppRegistryReady() override;
  void ExecuteOnAppRegistryReady(base::OnceClosure task) override;

  std::optional<PwaDomain> GetInstalledPwaDomain();
  std::optional<PwaDomain> GetInstalledPwaDomainForMigration();
  void CompleteAsyncInitialization();
  void NotifyInstalledAppUrlChangedIfNecessary();
  void OnSetUpNewAppResult(const std::optional<PwaDomain>& migrating_from,
                           const GURL& install_url,
                           bool success);
  void OnRemoveOldAppResult(const std::optional<PwaDomain>& migrating_from,
                            bool success);
  void HandleAppSetupFinished();

  raw_ptr<Profile> profile_;
  raw_ptr<AndroidSmsAppSetupController> setup_controller_;
  raw_ptr<app_list::AppListSyncableService> app_list_syncable_service_;
  raw_ptr<PrefService> pref_service_;

  // True if installation is in currently in progress.
  bool is_new_app_setup_in_progress_ = false;

  // True if the Messages PWA should be launched once setup completes
  // successfully.
  bool is_app_launch_pending_ = false;

  // The installed app URL, initialized when app registry is ready and updated
  // any time NotifyInstalledAppUrlChanged() is invoked.
  std::optional<GURL> last_installed_url_;

  std::unique_ptr<PwaDelegate> pwa_delegate_;
  base::WeakPtrFactory<AndroidSmsAppManagerImpl> weak_ptr_factory_{this};
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_
