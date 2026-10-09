// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "ash/webui/eche_app_ui/eche_tray_stream_status_observer.h"
#include "ash/webui/eche_app_ui/eche_uid_provider.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_accessibility_provider_proxy.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#include "chromeos/ash/services/secure_channel/presence_monitor_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ash {
namespace eche_app {

namespace {

void LaunchWebApp(const std::string& package_name,
                  const std::optional<int64_t>& notification_id,
                  const std::u16string& visible_name,
                  const std::optional<int64_t>& user_id,
                  const gfx::Image& icon,
                  const std::u16string& phone_name,
                  AppsLaunchInfoProvider* apps_launch_info_provider,
                  EcheAppManager* eche_app_manager) {
  EcheAppManagerFactory::GetInstance()->SetLastLaunchedAppInfo(
      std::make_unique<LaunchedAppInfo>(LaunchedAppInfo{
          .package_name = package_name,
          .visible_name = visible_name,
          .user_id = user_id,
          .icon = icon,
          .phone_name = phone_name,
          .apps_launch_info_provider = apps_launch_info_provider,
      }));
  // Use hash mark(#) to send params to webui so we don't need to reload the
  // whole eche window.
  std::u16string url = base::StrCat(
      {u"chrome://eche-app/#package_name=", base::UTF8ToUTF16(package_name),
       u"&visible_app_name=", visible_name, u"&timestamp=",
       base::NumberToString16(
           base::Time::Now().InMillisecondsSinceUnixEpoch())});
  if (notification_id.has_value()) {
    base::StrAppend(&url, {u"&notification_id=",
                           base::NumberToString16(notification_id.value())});
  }
  if (user_id.has_value()) {
    base::StrAppend(&url,
                    {u"&user_id=", base::NumberToString16(user_id.value())});
  }
  const auto gurl = GURL(url);

  // `eche_app_manager` may be null in tests.
  return LaunchBubble(
      gurl, icon, visible_name, phone_name,
      apps_launch_info_provider->GetConnectionStatusFromLastAttempt(),
      apps_launch_info_provider->entry_point(),
      eche_app_manager ? base::BindOnce(&EcheAppManager::CloseStream,
                                        base::Unretained(eche_app_manager))
                       : base::DoNothing(),
      eche_app_manager ? base::BindRepeating(&EcheAppManager::StreamGoBack,
                                             base::Unretained(eche_app_manager))
                       : base::DoNothing(),
      eche_app_manager ? base::BindRepeating(&EcheAppManager::BubbleShown,
                                             base::Unretained(eche_app_manager))
                       : base::DoNothing());
}

void RelaunchLast(Profile* profile) {
  EcheAppManager* eche_app_manager =
      EcheAppManagerFactory::GetForProfile(profile);
  std::unique_ptr<LaunchedAppInfo> last_launched_app_info =
      EcheAppManagerFactory::GetInstance()->GetLastLaunchedAppInfo();
  LaunchWebApp(
      last_launched_app_info->package_name,
      /*notification_id=*/std::nullopt, last_launched_app_info->visible_name,
      last_launched_app_info->user_id, last_launched_app_info->icon,
      last_launched_app_info->phone_name,
      last_launched_app_info->apps_launch_info_provider, eche_app_manager);
  if (eche_app_manager) {
    eche_app_manager->CloseConnectionOrLaunchErrorNotifications();
  }
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
  static base::NoDestructor<EcheAppManagerFactory> instance;
  return instance.get();
}

// static
void EcheAppManagerFactory::LaunchEcheApp(
    Profile* profile,
    const std::optional<int64_t>& notification_id,
    const std::string& package_name,
    const std::u16string& visible_name,
    const std::optional<int64_t>& user_id,
    const gfx::Image& icon,
    const std::u16string& phone_name,
    AppsLaunchInfoProvider* apps_launch_info_provider) {
  EcheAppManager* eche_app_manager =
      EcheAppManagerFactory::GetForProfile(profile);
  LaunchWebApp(package_name, notification_id, visible_name, user_id, icon,
               phone_name, apps_launch_info_provider, eche_app_manager);
  if (eche_app_manager) {
    eche_app_manager->CloseConnectionOrLaunchErrorNotifications();
  }
}

EcheAppManagerFactory::EcheAppManagerFactory()
    : ProfileKeyedServiceFactory(
          "EcheAppManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(phonehub::PhoneHubManagerFactory::GetInstance());
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(secure_channel::NearbyConnectorFactory::GetInstance());
}

EcheAppManagerFactory::~EcheAppManagerFactory() = default;

void EcheAppManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kEcheAppSeedPref, "");
  AppsAccessManagerImpl::RegisterPrefs(registry);
}

std::unique_ptr<KeyedService>
EcheAppManagerFactory::BuildServiceInstanceForBrowserContext(
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

  auto presence_monitor =
      std::make_unique<secure_channel::PresenceMonitorImpl>();
  std::unique_ptr<secure_channel::PresenceMonitorClient>
      presence_monitor_client =
          secure_channel::PresenceMonitorClientImpl::Factory::Create(
              std::move(presence_monitor));

  std::unique_ptr<EcheAppManager> eche_app_manager =
      std::make_unique<EcheAppManager>(
          profile->GetPrefs(),
          BrowserContextHelper::Get()->GetUserByBrowserContext(profile),
          phone_hub_manager, device_sync_client, multidevice_setup_client,
          secure_channel_client, std::move(presence_monitor_client),
          std::make_unique<EcheAppAccessibilityProviderProxy>(),
          base::BindRepeating(&EcheAppManagerFactory::LaunchEcheApp, profile),
          base::BindRepeating(&RelaunchLast, profile));

  EcheTray* eche_tray = Shell::GetPrimaryRootWindowController()
                            ->GetStatusAreaWidget()
                            ->eche_tray();

  if (eche_tray) {
    eche_tray->SetEcheConnectionStatusHandler(
        eche_app_manager->GetEcheConnectionStatusHandler());
  }

  return eche_app_manager;
}

void EcheAppManagerFactory::SetLastLaunchedAppInfo(
    std::unique_ptr<LaunchedAppInfo> last_launched_app_info) {
  last_launched_app_info_ = std::move(last_launched_app_info);
}

std::unique_ptr<LaunchedAppInfo>
EcheAppManagerFactory::GetLastLaunchedAppInfo() {
  return std::move(last_launched_app_info_);
}

}  // namespace eche_app
}  // namespace ash
