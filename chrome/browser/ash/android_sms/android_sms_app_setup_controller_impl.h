// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/android_sms/android_sms_app_setup_controller.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class Profile;

namespace network {
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace ash {
namespace android_sms {

// Concrete AndroidSmsAppSetupController implementation.
class AndroidSmsAppSetupControllerImpl : public AndroidSmsAppSetupController {
 public:
  AndroidSmsAppSetupControllerImpl(
      Profile* profile,
      web_app::ExternallyManagedAppManager* externally_managed_app_manager,
      HostContentSettingsMap* host_content_settings_map);

  AndroidSmsAppSetupControllerImpl(const AndroidSmsAppSetupControllerImpl&) =
      delete;
  AndroidSmsAppSetupControllerImpl& operator=(
      const AndroidSmsAppSetupControllerImpl&) = delete;

  ~AndroidSmsAppSetupControllerImpl() override;

 private:
  friend class AndroidSmsAppSetupControllerImplTest;
  FRIEND_TEST_ALL_PREFIXES(AndroidSmsAppSetupControllerImplTest,
                           SetUpApp_Retry);

  static const base::TimeDelta kInstallRetryDelay;
  static const size_t kMaxInstallRetryCount;

  // Thin wrapper around static PWA functions which is stubbed out for tests.
  class PwaDelegate {
   public:
    PwaDelegate();
    virtual ~PwaDelegate();

    virtual std::optional<webapps::AppId> GetPwaForUrl(const GURL& install_url,
                                                       Profile* profile);
    virtual network::mojom::CookieManager* GetCookieManager(Profile* profile);
    virtual void RemovePwa(const webapps::AppId& app_id,
                           Profile* profile,
                           SuccessCallback callback);
  };

  // AndroidSmsAppSetupController:
  void SetUpApp(const GURL& app_url,
                const GURL& install_url,
                SuccessCallback callback) override;
  std::optional<webapps::AppId> GetPwa(const GURL& install_url) override;
  void DeleteRememberDeviceByDefaultCookie(const GURL& app_url,
                                           SuccessCallback callback) override;
  void RemoveApp(const GURL& app_url,
                 const GURL& install_url,
                 const GURL& migrated_to_app_url,
                 SuccessCallback callback) override;

  void OnAppRemoved(SuccessCallback callback,
                    const GURL& app_url,
                    const GURL& install_url,
                    const GURL& migrated_to_app_url,
                    bool uninstalled);
  void OnSetRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                const GURL& install_url,
                                                SuccessCallback callback,
                                                net::CookieAccessResult result);
  void OnSetMigrationCookieResult(const GURL& app_url,
                                  SuccessCallback callback,
                                  net::CookieAccessResult result);

  void TryInstallApp(const GURL& install_url,
                     const GURL& app_url,
                     size_t num_attempts_so_far,
                     SuccessCallback callback);

  void OnAppInstallResult(
      SuccessCallback callback,
      size_t num_attempts_so_far,
      const GURL& app_url,
      const GURL& install_url,
      web_app::ExternallyManagedAppManager::InstallResult result);
  void SetMigrationCookie(const GURL& app_url,
                          const GURL& migrated_to_app_url,
                          SuccessCallback callback);
  void OnDeleteRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                   SuccessCallback callback,
                                                   uint32_t num_deleted);
  void OnDeleteMigrationCookieResult(const GURL& app_url,
                                     const GURL& install_url,
                                     SuccessCallback callback,
                                     uint32_t num_deleted);

  void SetPwaDelegateForTesting(std::unique_ptr<PwaDelegate> test_pwa_delegate);

  raw_ptr<Profile> profile_;
  raw_ptr<web_app::ExternallyManagedAppManager> externally_managed_app_manager_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  std::unique_ptr<PwaDelegate> pwa_delegate_;
  base::WeakPtrFactory<AndroidSmsAppSetupControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_
