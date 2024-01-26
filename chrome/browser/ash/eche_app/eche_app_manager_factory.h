// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_

#include <optional>

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace ash {
namespace eche_app {

class EcheAppManager;
class EcheAppNotificationController;
class SystemInfo;
class AppsLaunchInfoProvider;

class LaunchedAppInfo {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    std::unique_ptr<LaunchedAppInfo> Build() {
      return base::WrapUnique(new LaunchedAppInfo(package_name_, visible_name_,
                                                  user_id_, icon_, phone_name_,
                                                  apps_launch_info_provider_));
    }
    Builder& SetPackageName(const std::string& package_name) {
      package_name_ = package_name;
      return *this;
    }

    Builder& SetVisibleName(const std::u16string& visible_name) {
      visible_name_ = visible_name;
      return *this;
    }

    Builder& SetUserId(const std::optional<int64_t>& user_id) {
      user_id_ = user_id;
      return *this;
    }

    Builder& SetIcon(const gfx::Image& icon) {
      icon_ = icon;
      return *this;
    }

    Builder& SetPhoneName(const std::u16string& phone_name) {
      phone_name_ = phone_name;
      return *this;
    }

    Builder& SetAppsLaunchInfoProvider(
        AppsLaunchInfoProvider* apps_launch_info_provider) {
      apps_launch_info_provider_ = apps_launch_info_provider;
      return *this;
    }

   private:
    std::string package_name_;
    std::u16string visible_name_;
    std::optional<int64_t> user_id_;
    gfx::Image icon_;
    std::u16string phone_name_;
    raw_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  };

  LaunchedAppInfo() = delete;
  LaunchedAppInfo(const LaunchedAppInfo&) = delete;
  LaunchedAppInfo& operator=(const LaunchedAppInfo&) = delete;
  ~LaunchedAppInfo();

  std::string package_name() const { return package_name_; }
  std::u16string visible_name() const { return visible_name_; }
  std::optional<int64_t> user_id() const { return user_id_; }
  gfx::Image icon() const { return icon_; }
  std::u16string phone_name() const { return phone_name_; }
  AppsLaunchInfoProvider* apps_launch_info_provider() {
    return apps_launch_info_provider_;
  }

 protected:
  LaunchedAppInfo(const std::string& package_name,
                  const std::u16string& visible_name,
                  const std::optional<int64_t>& user_id,
                  const gfx::Image& icon,
                  const std::u16string& phone_name,
                  AppsLaunchInfoProvider* apps_launch_info_provider);

 private:
  std::string package_name_;
  std::u16string visible_name_;
  std::optional<int64_t> user_id_;
  gfx::Image icon_;
  std::u16string phone_name_;
  raw_ptr<AppsLaunchInfoProvider, DanglingUntriaged> apps_launch_info_provider_;
};

// Factory to create a single EcheAppManager.
class EcheAppManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static EcheAppManager* GetForProfile(Profile* profile);
  static EcheAppManagerFactory* GetInstance();
  static void ShowNotification(
      base::WeakPtr<EcheAppManagerFactory> weak_ptr,
      Profile* profile,
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info);
  static void CloseNotification(base::WeakPtr<EcheAppManagerFactory> weak_ptr,
                                Profile* profile,
                                const std::string& notification_id);
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
  void CloseConnectionOrLaunchErrorNotifications();

  EcheAppManagerFactory(const EcheAppManagerFactory&) = delete;
  EcheAppManagerFactory& operator=(const EcheAppManagerFactory&) = delete;

  std::unique_ptr<SystemInfo> GetSystemInfo(Profile* profile) const;

 private:
  friend base::NoDestructor<EcheAppManagerFactory>;
  friend class EcheAppManagerFactoryTest;

  EcheAppManagerFactory();
  ~EcheAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<LaunchedAppInfo> last_launched_app_info_;

  std::unique_ptr<EcheAppNotificationController> notification_controller_;
  base::WeakPtrFactory<EcheAppManagerFactory> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
