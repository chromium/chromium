// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eche_app/eche_app_manager_factory.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/chromeos/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chromeos/components/eche_app_ui/eche_app_manager.h"
#include "chromeos/components/eche_app_ui/eche_uid_provider.h"
#include "chromeos/components/eche_app_ui/system_info.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"

namespace chromeos {
namespace eche_app {

namespace {

void CloseEcheApp(Profile* profile) {
  for (auto* browser : *(BrowserList::GetInstance())) {
    if (browser->profile() != profile)
      continue;
    if (!browser->app_controller())
      continue;
    if (browser->app_controller()->system_app_type() !=
        web_app::SystemAppType::ECHE)
      continue;
    browser->window()->Close();
    return;
  }
}
// Enumeration of possible interactions with a PhoneHub notification. Keep in
// sync with corresponding enum in tools/metrics/histograms/enums.xml. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class NotificationInteraction {
  kUnknown = 0,
  kOpenAppStreaming = 1,
  kMaxValue = kOpenAppStreaming,
};

void LaunchSystemWebApp(Profile* profile,
                        std::string package_name,
                        absl::optional<int64_t> notification_id) {
  std::string url;
  // Use hash mark(#) to send params to webui so we don't need to reload the
  // whole eche window.
  if (notification_id.has_value()) {
    url = "chrome://eche-app/#notification_id=";
    url.append(base::NumberToString(notification_id.value()));
    url.append("&package_name=");
  } else {
    url = "chrome://eche-app/#package_name=";
  }
  url.append(package_name);
  url.append("&timestamp=");

  double now_seconds = base::Time::Now().ToDoubleT();
  int64_t now_ms = static_cast<int64_t>(now_seconds * 1000);
  url.append(base::NumberToString(now_ms));
  web_app::SystemAppLaunchParams params;
  params.url = GURL(url);
  web_app::LaunchSystemWebAppAsync(profile, web_app::SystemAppType::ECHE,
                                   params);
}

void LaunchEcheApp(Profile* profile,
                   int64_t notification_id,
                   const std::string& package_name) {
  LaunchSystemWebApp(profile, package_name, notification_id);
  base::UmaHistogramEnumeration("Eche.NotificationClicked",
                                NotificationInteraction::kOpenAppStreaming);
}

void LaunchEcheAppWithPackageName(Profile* profile,
                                  const std::string& package_name) {
  LaunchSystemWebApp(profile, package_name, /*notification_id=*/absl::nullopt);
}

}  // namespace

// static
EcheAppManager* EcheAppManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<EcheAppManager*>(
      EcheAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
EcheAppManagerFactory* EcheAppManagerFactory::GetInstance() {
  return base::Singleton<EcheAppManagerFactory>::get();
}

EcheAppManagerFactory::EcheAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "EcheAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(phonehub::PhoneHubManagerFactory::GetInstance());
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(secure_channel::NearbyConnectorFactory::GetInstance());
}

EcheAppManagerFactory::~EcheAppManagerFactory() = default;

void EcheAppManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kEcheAppSeedPref, "");
}

KeyedService* EcheAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsPhoneHubEnabled() || !features::IsEcheSWAEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  phonehub::PhoneHubManager* phone_hub_manager =
      phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  if (!phone_hub_manager)
    return nullptr;

  device_sync::DeviceSyncClient* device_sync_client =
      device_sync::DeviceSyncClientFactory::GetForProfile(profile);
  if (!device_sync_client)
    return nullptr;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client =
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile);
  if (!multidevice_setup_client)
    return nullptr;

  secure_channel::SecureChannelClient* secure_channel_client =
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient();
  if (!secure_channel_client)
    return nullptr;

  return new EcheAppManager(
      profile->GetPrefs(), GetSystemInfo(profile), phone_hub_manager,
      device_sync_client, multidevice_setup_client, secure_channel_client,
      base::BindRepeating(&LaunchEcheApp, profile),
      base::BindRepeating(&CloseEcheApp, profile),
      base::BindRepeating(&LaunchEcheAppWithPackageName, profile));
}

std::unique_ptr<SystemInfo> EcheAppManagerFactory::GetSystemInfo(
    Profile* profile) const {
  std::string device_name = "";
  const std::string board_name = base::SysInfo::GetLsbReleaseBoard();
  const std::u16string device_type = ui::GetChromeOSDeviceName();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    std::u16string given_name = user->GetGivenName();
    if (!given_name.empty()) {
      device_name = l10n_util::GetStringFUTF8(
          IDS_ECHE_APP_DEFAULT_DEVICE_NAME, user->GetGivenName(), device_type);
    }
  }
  return SystemInfo::Builder()
      .SetDeviceName(device_name)
      .SetBoardName(board_name)
      .Build();
}

}  // namespace eche_app
}  // namespace chromeos
