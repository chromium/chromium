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
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_accessibility_provider_proxy.h"
#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/secure_channel/presence_monitor_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ash {
namespace eche_app {

namespace {

void EnsureStreamClose(Profile* profile) {
  EcheAppManager* eche_app_manager =
      EcheAppManagerFactory::GetForProfile(profile);
  eche_app_manager->CloseStream();
}

void StreamGoBack(Profile* profile) {
  EcheAppManager* eche_app_manager =
      EcheAppManagerFactory::GetForProfile(profile);
  eche_app_manager->StreamGoBack();
}

void BubbleShown(Profile* profile, AshWebView* view) {
  EcheAppManager* eche_app_manager =
      EcheAppManagerFactory::GetForProfile(profile);
  // `eche_app_manager` is null during tests.
  if (eche_app_manager) {
    eche_app_manager->BubbleShown(view);
  }
}

void LaunchWebApp(const std::string& package_name,
                  const std::optional<int64_t>& notification_id,
                  const std::u16string& visible_name,
                  const std::optional<int64_t>& user_id,
                  const gfx::Image& icon,
                  const std::u16string& phone_name,
                  AppsLaunchInfoProvider* apps_launch_info_provider,
                  Profile* profile) {
  EcheAppManagerFactory::GetInstance()->SetLastLaunchedAppInfo(
      LaunchedAppInfo::Builder()
          .SetPackageName(package_name)
          .SetVisibleName(visible_name)
          .SetUserId(user_id)
          .SetIcon(icon)
          .SetPhoneName(phone_name)
          .SetAppsLaunchInfoProvider(apps_launch_info_provider)
          .Build());
  std::u16string url;
  // Use hash mark(#) to send params to webui so we don't need to reload the
  // whole eche window.
  if (notification_id.has_value()) {
    url = u"chrome://eche-app/#notification_id=";
    url.append(base::NumberToString16(notification_id.value()));
    url.append(u"&package_name=");
  } else {
    url = u"chrome://eche-app/#package_name=";
  }
  std::u16string u16_package_name = base::UTF8ToUTF16(package_name);
  url.append(u16_package_name);
  url.append(u"&visible_app_name=");
  url.append(visible_name);
  url.append(u"&timestamp=");

  int64_t now_ms = base::Time::Now().InMillisecondsSinceUnixEpoch();
  url.append(base::NumberToString16(now_ms));

  if (user_id.has_value()) {
    url.append(u"&user_id=");
    url.append(base::NumberToString16(user_id.value()));
  }
  const auto gurl = GURL(url);

  return LaunchBubble(
      gurl, icon, visible_name, phone_name,
      apps_launch_info_provider->GetConnectionStatusFromLastAttempt(),
      apps_launch_info_provider->entry_point(),
      base::BindOnce(&EnsureStreamClose, profile),
      base::BindRepeating(&StreamGoBack, profile),
      base::BindRepeating(&BubbleShown, profile));
}

void RelaunchLast(Profile* profile) {
  std::unique_ptr<LaunchedAppInfo> last_launched_app_info =
      EcheAppManagerFactory::GetInstance()->GetLastLaunchedAppInfo();
  EcheAppManagerFactory::LaunchEcheApp(
      profile, std::nullopt, last_launched_app_info->package_name(),
      last_launched_app_info->visible_name(), last_launched_app_info->user_id(),
      last_launched_app_info->icon(), last_launched_app_info->phone_name(),
      last_launched_app_info->apps_launch_info_provider());
}

}  // namespace

LaunchedAppInfo::~LaunchedAppInfo() = default;
LaunchedAppInfo::LaunchedAppInfo(
    const std::string& package_name,
    const std::u16string& visible_name,
    const std::optional<int64_t>& user_id,
    const gfx::Image& icon,
    const std::u16string& phone_name,
    AppsLaunchInfoProvider* apps_launch_info_provider) {
  package_name_ = package_name;
  visible_name_ = visible_name;
  user_id_ = user_id;
  icon_ = icon;
  phone_name_ = phone_name;
  apps_launch_info_provider_ = apps_launch_info_provider;
}

LaunchedAppInfo::Builder::Builder() = default;
LaunchedAppInfo::Builder::~Builder() = default;

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
void EcheAppManagerFactory::ShowNotification(
    base::WeakPtr<EcheAppManagerFactory> weak_ptr,
    Profile* profile,
    const std::optional<std::u16string>& title,
    const std::optional<std::u16string>& message,
    std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
  if (!weak_ptr->notification_controller_) {
    weak_ptr->notification_controller_ =
        std::make_unique<EcheAppNotificationController>(
            profile, base::BindRepeating(&RelaunchLast));
  }

  if (info->category() ==
      LaunchAppHelper::NotificationInfo::Category::kNative) {
    if (absl::get<LaunchAppHelper::NotificationInfo::NotificationType>(
            info->type()) ==
        LaunchAppHelper::NotificationInfo::NotificationType::kScreenLock) {
      weak_ptr->notification_controller_->ShowScreenLockNotification(
          title ? title.value()
                : u"");  // If null, show a default value to be safe.
    }
  } else if (info->category() ==
             LaunchAppHelper::NotificationInfo::Category::kWebUI) {
    weak_ptr->notification_controller_->ShowNotificationFromWebUI(
        title, message, info->type());
  }
}

// static
void EcheAppManagerFactory::CloseNotification(
    base::WeakPtr<EcheAppManagerFactory> weak_ptr,
    Profile* profile,
    const std::string& notification_id) {
  if (!weak_ptr->notification_controller_) {
    weak_ptr->notification_controller_ =
        std::make_unique<EcheAppNotificationController>(
            profile, base::BindRepeating(&RelaunchLast));
  }
  weak_ptr->notification_controller_->CloseNotification(notification_id);
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
  LaunchWebApp(package_name, notification_id, visible_name, user_id, icon,
               phone_name, apps_launch_info_provider, profile);
  EcheAppManagerFactory::GetInstance()
      ->CloseConnectionOrLaunchErrorNotifications();
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

  std::unique_ptr<EcheAppManager> eche_app_manager = std::make_unique<EcheAppManager>(
      profile->GetPrefs(), GetSystemInfo(profile), phone_hub_manager,
      device_sync_client, multidevice_setup_client, secure_channel_client,
      std::move(presence_monitor_client),
      std::make_unique<EcheAppAccessibilityProviderProxy>(),
      base::BindRepeating(&EcheAppManagerFactory::LaunchEcheApp, profile),
      base::BindRepeating(&EcheAppManagerFactory::ShowNotification,
                          weak_ptr_factory_.GetMutableWeakPtr(), profile),
      base::BindRepeating(&EcheAppManagerFactory::CloseNotification,
                          weak_ptr_factory_.GetMutableWeakPtr(), profile));

  EcheTray* eche_tray = Shell::GetPrimaryRootWindowController()
                            ->GetStatusAreaWidget()
                            ->eche_tray();

  if (features::IsEcheNetworkConnectionStateEnabled() && eche_tray) {
    eche_tray->SetEcheConnectionStatusHandler(
        eche_app_manager->GetEcheConnectionStatusHandler());
  }

  return eche_app_manager;
}

std::unique_ptr<SystemInfo> EcheAppManagerFactory::GetSystemInfo(
    Profile* profile) const {
  std::string device_name;
  const std::string board_name = base::SysInfo::GetLsbReleaseBoard();
  const std::u16string device_type = ui::GetChromeOSDeviceName();
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  std::string gaia_id;
  if (user) {
    std::u16string given_name = user->GetGivenName();
    if (!given_name.empty()) {
      device_name = l10n_util::GetStringFUTF8(
          IDS_ECHE_APP_DEFAULT_DEVICE_NAME, user->GetGivenName(), device_type);
    }
    if (user->HasGaiaAccount()) {
      const AccountId& account_id = user->GetAccountId();
      gaia_id = account_id.GetGaiaId();
    }
  }

  SystemInfo::Builder system_info;
  system_info.SetDeviceName(device_name)
      .SetBoardName(board_name)
      .SetGaiaId(gaia_id)
      .SetDeviceType(base::UTF16ToUTF8(device_type));

  if (features::IsEcheMetricsRevampEnabled()) {
    system_info.SetOsVersion(base::SysInfo::OperatingSystemVersion())
        .SetChannel(chrome::GetChannelName(chrome::WithExtendedStable(true)));
  }

  return system_info.Build();
}

void EcheAppManagerFactory::SetLastLaunchedAppInfo(
    std::unique_ptr<LaunchedAppInfo> last_launched_app_info) {
  last_launched_app_info_ = std::move(last_launched_app_info);
}

std::unique_ptr<LaunchedAppInfo>
EcheAppManagerFactory::GetLastLaunchedAppInfo() {
  return std::move(last_launched_app_info_);
}

void EcheAppManagerFactory::CloseConnectionOrLaunchErrorNotifications() {
  if (notification_controller_ != nullptr)
    notification_controller_->CloseConnectionOrLaunchErrorNotifications();
}

}  // namespace eche_app
}  // namespace ash
