// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace ash {
namespace eche_app {

class EcheAppManager;
class AppsLaunchInfoProvider;

struct LaunchedAppInfo {
  std::string package_name;
  std::u16string visible_name;
  std::optional<int64_t> user_id;
  gfx::Image icon;
  std::u16string phone_name;
  raw_ptr<AppsLaunchInfoProvider, DanglingUntriaged> apps_launch_info_provider =
      nullptr;
};

// Factory to create a single EcheAppManager.
class EcheAppManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static EcheAppManager* GetForProfile(Profile* profile);
  static EcheAppManagerFactory* GetInstance();
  static void LaunchEcheApp(Profile* profile,
                            const std::optional<int64_t>& notification_id,
                            const std::string& package_name,
                            const std::u16string& visible_name,
                            const std::optional<int64_t>& user_id,
                            const gfx::Image& icon,
                            const std::u16string& phone_name,
                            AppsLaunchInfoProvider* apps_launch_info_provider);

  void SetLastLaunchedAppInfo(
      std::unique_ptr<LaunchedAppInfo> last_launched_app_info);
  std::unique_ptr<LaunchedAppInfo> GetLastLaunchedAppInfo();

  EcheAppManagerFactory(const EcheAppManagerFactory&) = delete;
  EcheAppManagerFactory& operator=(const EcheAppManagerFactory&) = delete;

 private:
  friend base::NoDestructor<EcheAppManagerFactory>;

  EcheAppManagerFactory();
  ~EcheAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<LaunchedAppInfo> last_launched_app_info_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
