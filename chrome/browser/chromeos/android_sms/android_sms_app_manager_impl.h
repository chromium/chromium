// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace app_list {
class AppListSyncableService;
}  // namespace app_list

namespace chromeos {

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
  ~AndroidSmsAppManagerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class AndroidSmsAppManagerImplTest;

  // AndroidSmsAppManager:
  base::Optional<GURL> GetCurrentAppUrl() override;

  // AndroidSmsAppHelperDelegate:
  void SetUpAndroidSmsApp() override;
  void SetUpAndLaunchAndroidSmsApp() override;
  void TearDownAndroidSmsApp() override;
  bool HasAppBeenManuallyUninstalledByUser() override;
  bool IsAppInstalled() override;
  bool IsAppRegistryReady() override;
  void ExecuteOnAppRegistryReady(base::OnceClosure task) override;

  base::Optional<PwaDomain> GetInstalledPwaDomain();
  void CompleteAsyncInitialization();
  void NotifyInstalledAppUrlChangedIfNecessary();
  void OnSetUpNewAppResult(const base::Optional<PwaDomain>& migrating_from,
                           const GURL& install_url,
                           bool success);
  void OnRemoveOldAppResult(const base::Optional<PwaDomain>& migrating_from,
                            bool success);
  void HandleAppSetupFinished();

  Profile* profile_;
  AndroidSmsAppSetupController* setup_controller_;
  app_list::AppListSyncableService* app_list_syncable_service_;
  PrefService* pref_service_;

  // True if installation is in currently in progress.
  bool is_new_app_setup_in_progress_ = false;

  // True if the Messages PWA should be launched once setup completes
  // successfully.
  bool is_app_launch_pending_ = false;

  // The installed app URL, initialized when app registry is ready and updated
  // any time NotifyInstalledAppUrlChanged() is invoked.
  base::Optional<GURL> last_installed_url_;

  std::unique_ptr<PwaDelegate> pwa_delegate_;
  base::WeakPtrFactory<AndroidSmsAppManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppManagerImpl);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_IMPL_H_
