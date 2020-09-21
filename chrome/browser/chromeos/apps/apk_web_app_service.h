// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/keyed_service/core/keyed_service.h"

class ArcAppListPrefs;
class Profile;
class GURL;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace web_app {
enum class InstallResultCode;
class WebAppProvider;
}  // namespace web_app

namespace chromeos {

class ApkWebAppService : public KeyedService,
                         public ApkWebAppInstaller::Owner,
                         public ArcAppListPrefs::Observer,
                         public web_app::AppRegistrarObserver {
 public:
  static ApkWebAppService* Get(Profile* profile);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit ApkWebAppService(Profile* profile);
  ~ApkWebAppService() override;

  void SetArcAppListPrefsForTesting(ArcAppListPrefs* prefs);

  bool IsWebOnlyTwa(const web_app::AppId& app_id);

  bool IsWebAppInstalledFromArc(const web_app::AppId& web_app_id);

  base::Optional<std::string> GetPackageNameForWebApp(
      const web_app::AppId& app_id);

  base::Optional<std::string> GetPackageNameForWebApp(const GURL& url);

  base::Optional<std::string> GetCertificateSha256Fingerprint(
      const web_app::AppId& app_id);

  using WebAppCallbackForTesting =
      base::OnceCallback<void(const std::string& package_name,
                              const web_app::AppId& web_app_id)>;
  void SetWebAppInstalledCallbackForTesting(
      WebAppCallbackForTesting web_app_installed_callback);
  void SetWebAppUninstalledCallbackForTesting(
      WebAppCallbackForTesting web_app_uninstalled_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ApkWebAppInstallerDelayedArcStartBrowserTest,
                           DelayedUninstall);

  // Uninstalls a web app with id |web_app_id| iff it was installed via calling
  // ApkWebAppInstaller::Install().
  void UninstallWebApp(const web_app::AppId& web_app_id);

  // If the app has updated from a web app to Android app or vice-versa,
  // this function pins the new app in the old app's place on the shelf if it
  // was pinned prior to the update.
  void UpdateShelfPin(const arc::mojom::ArcPackageInfo* package_info);

  // KeyedService:
  void Shutdown() override;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  // web_app::AppRegistrarObserver overrides.
  void OnWebAppUninstalled(const web_app::AppId& web_app_id) override;

  void OnDidGetWebAppIcon(const std::string& package_name,
                          arc::mojom::WebAppInfoPtr web_app_info,
                          arc::mojom::RawIconPngDataPtr icon);
  void OnDidFinishInstall(const std::string& package_name,
                          const web_app::AppId& web_app_id,
                          bool is_web_only_twa,
                          const base::Optional<std::string> sha256_fingerprint,
                          web_app::InstallResultCode code);
  void UpdatePackageInfo(const std::string& app_id,
                         const arc::mojom::WebAppInfoPtr& web_app_info);

  WebAppCallbackForTesting web_app_installed_callback_;
  WebAppCallbackForTesting web_app_uninstalled_callback_;

  Profile* profile_;
  ArcAppListPrefs* arc_app_list_prefs_;
  web_app::WebAppProvider* provider_;

  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observer_{this};

  // Must go last.
  base::WeakPtrFactory<ApkWebAppService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ApkWebAppService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_
