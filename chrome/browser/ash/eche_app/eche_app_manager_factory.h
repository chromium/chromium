// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {
namespace eche_app {

class EcheAppManager;
class EcheAppNotificationController;
class SystemInfo;

// Factory to create a single EcheAppManager.
class EcheAppManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EcheAppManager* GetForProfile(Profile* profile);
  static EcheAppManagerFactory* GetInstance();
  static void ShowNotification(
      base::WeakPtr<EcheAppManagerFactory> weak_ptr,
      Profile* profile,
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info);

  EcheAppManagerFactory(const EcheAppManagerFactory&) = delete;
  EcheAppManagerFactory& operator=(const EcheAppManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<EcheAppManagerFactory>;

  EcheAppManagerFactory();
  ~EcheAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<SystemInfo> GetSystemInfo(Profile* profile) const;

  std::unique_ptr<EcheAppNotificationController> notification_controller_;
  base::WeakPtrFactory<EcheAppManagerFactory> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace eche_app {
using ::ash::eche_app::EcheAppManagerFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_MANAGER_FACTORY_H_
