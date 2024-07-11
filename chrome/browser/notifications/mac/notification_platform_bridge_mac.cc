// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_platform_bridge_mac.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mojo.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

NotificationPlatformBridgeMac::NotificationPlatformBridgeMac(
    std::unique_ptr<NotificationDispatcherMac> banner_dispatcher,
    std::unique_ptr<NotificationDispatcherMac> alert_dispatcher,
    WebAppDispatcherFactory web_app_dispatcher_factory)
    : banner_dispatcher_(std::move(banner_dispatcher)),
      alert_dispatcher_(std::move(alert_dispatcher)),
      web_app_dispatcher_factory_(std::move(web_app_dispatcher_factory)) {}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  // TODO(miguelg) do not remove banners if possible.
  banner_dispatcher_->CloseAllNotifications();
  alert_dispatcher_->CloseAllNotifications();
}

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  auto banner_dispatcher = std::make_unique<NotificationDispatcherMojo>(
      std::make_unique<MacNotificationProviderFactory>(
          mac_notifications::NotificationStyle::kBanner));
  auto alert_dispatcher = std::make_unique<NotificationDispatcherMojo>(
      std::make_unique<MacNotificationProviderFactory>(
          mac_notifications::NotificationStyle::kAlert));
  auto create_dispatcher_for_web_app =
      base::BindRepeating([](const webapps::AppId& web_app_id)
                              -> std::unique_ptr<NotificationDispatcherMac> {
        return std::make_unique<NotificationDispatcherMojo>(
            std::make_unique<MacNotificationProviderFactory>(
                mac_notifications::NotificationStyle::kAppShim, web_app_id));
      });

  return std::make_unique<NotificationPlatformBridgeMac>(
      std::move(banner_dispatcher), std::move(alert_dispatcher),
      std::move(create_dispatcher_for_web_app));
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

void NotificationPlatformBridgeMac::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  NotificationDispatcherMac* dispatcher = nullptr;

  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution) &&
      notification.notifier_id().web_app_id.has_value() &&
      AppShimRegistry::Get()->IsAppInstalledInProfile(
          *notification.notifier_id().web_app_id, profile->GetPath())) {
    dispatcher =
        GetOrCreateDispatcherForWebApp(*notification.notifier_id().web_app_id);
  }

  if (!dispatcher) {
    bool is_alert = IsAlertNotificationMac(notification);
    dispatcher = is_alert ? alert_dispatcher_.get() : banner_dispatcher_.get();
  }
  dispatcher->DisplayNotification(notification_type, profile, notification);
  CloseImpl(profile, notification.id(), dispatcher);
}

void NotificationPlatformBridgeMac::Close(Profile* profile,
                                          const std::string& notification_id) {
  CloseImpl(profile, notification_id);
}

void NotificationPlatformBridgeMac::CloseImpl(
    Profile* profile,
    const std::string& notification_id,
    NotificationDispatcherMac* excluded_dispatcher) {
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  if (banner_dispatcher_.get() != excluded_dispatcher) {
    banner_dispatcher_->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
  if (alert_dispatcher_.get() != excluded_dispatcher) {
    alert_dispatcher_->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
  for (auto& [app_id, dispatcher] : app_specific_dispatchers_) {
    if (dispatcher.get() == excluded_dispatcher) {
      continue;
    }
    dispatcher->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
}

void NotificationPlatformBridgeMac::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution)) {
    // We can't get all displayed notifications for all origins, since that
    // would involve starting up any app shim that might currently show
    // notifications. Fortunately we don't really need to implement this method
    // as this is only used for an initial state sync on Chrome start up. Before
    // for example a list of displayed notifications is returned via the web
    // API, additional calls to get displayed notifications for specific origins
    // happen.
    // TODO(crbug.com/40283098): Figure out how we can refactor the
    // various APIs to make this not be an issue.
    std::move(callback).Run({}, /*supports_synchronization=*/false);
    return;
  }

  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  auto notifications = std::make_unique<std::set<std::string>>();
  std::set<std::string>* notifications_ptr = notifications.get();
  auto barrier_closure = base::BarrierClosure(
      2, base::BindOnce(
             [](std::unique_ptr<std::set<std::string>> notifications,
                GetDisplayedNotificationsCallback callback) {
               std::move(callback).Run(std::move(*notifications),
                                       /*supports_synchronization=*/true);
             },
             std::move(notifications), std::move(callback)));

  auto get_notifications_callback = base::BindRepeating(
      [](std::set<std::string>* notifications_ptr, base::OnceClosure callback,
         std::set<std::string> notifications, bool supports_synchronization) {
        notifications_ptr->insert(notifications.begin(), notifications.end());
        std::move(callback).Run();
      },
      notifications_ptr, barrier_closure);

  banner_dispatcher_->GetDisplayedNotificationsForProfileId(
      profile_id, incognito, get_notifications_callback);
  alert_dispatcher_->GetDisplayedNotificationsForProfileId(
      profile_id, incognito, get_notifications_callback);
}

void NotificationPlatformBridgeMac::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  std::vector<webapps::AppId> web_app_ids;
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution)) {
    if (auto* web_app_provider =
            web_app::WebAppProvider::GetForWebApps(profile)) {
      web_app::WebAppRegistrar& registrar =
          web_app_provider->registrar_unsafe();
      for (const webapps::AppId& app_id : registrar.GetAppIds()) {
        if (!registrar.IsInstallState(
                app_id, {web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
          continue;
        }
        if (!url::IsSameOriginWith(registrar.GetAppScope(app_id), origin)) {
          continue;
        }
        web_app_ids.push_back(app_id);
      }
    }
    // TODO(mek): filter by web_app_ids that actually could be displaying
    // notifications rather than all for the origin.
  }

  auto notifications = std::make_unique<std::set<std::string>>();
  std::set<std::string>* notifications_ptr = notifications.get();
  auto barrier_closure = base::BarrierClosure(
      2 + web_app_ids.size(),
      base::BindOnce(
          [](std::unique_ptr<std::set<std::string>> notifications,
             GetDisplayedNotificationsCallback callback) {
            std::move(callback).Run(std::move(*notifications),
                                    /*supports_synchronization=*/true);
          },
          std::move(notifications), std::move(callback)));

  auto get_notifications_callback =
      base::BindRepeating(
          [](std::set<std::string>* notifications_ptr,
             std::set<std::string> notifications,
             bool supports_synchronization) {
            notifications_ptr->insert(notifications.begin(),
                                      notifications.end());
          },
          notifications_ptr)
          .Then(barrier_closure);

  banner_dispatcher_->GetDisplayedNotificationsForProfileIdAndOrigin(
      profile_id, incognito, origin, get_notifications_callback);
  alert_dispatcher_->GetDisplayedNotificationsForProfileIdAndOrigin(
      profile_id, incognito, origin, get_notifications_callback);

  for (const webapps::AppId& web_app_id : web_app_ids) {
    auto* dispatcher = GetOrCreateDispatcherForWebApp(web_app_id);
    dispatcher->GetDisplayedNotificationsForProfileIdAndOrigin(
        profile_id, incognito, origin, get_notifications_callback);
  }
}

void NotificationPlatformBridgeMac::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeMac::DisplayServiceShutDown(Profile* profile) {
  // Close all alerts and banners for |profile| on shutdown. We have to clean up
  // here instead of the destructor as mojo messages won't be delivered from
  // there as it's too late in the shutdown process. If the profile is null it
  // was the SystemNotificationHelper instance but we never show notifications
  // without a profile (Type::TRANSIENT) on macOS, so nothing to do here.
  if (profile)
    CloseAllNotificationsForProfile(profile);
}

void NotificationPlatformBridgeMac::AppShimWillTerminate(
    const webapps::AppId& web_app_id) {
  auto it = app_specific_dispatchers_.find(web_app_id);
  if (it == app_specific_dispatchers_.end()) {
    return;
  }
  it->second->UserInitiatedShutdown();
}

void NotificationPlatformBridgeMac::CloseAllNotificationsForProfile(
    Profile* profile) {
  DCHECK(profile);
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  banner_dispatcher_->CloseNotificationsWithProfileId(profile_id, incognito);
  alert_dispatcher_->CloseNotificationsWithProfileId(profile_id, incognito);
}

NotificationDispatcherMac*
NotificationPlatformBridgeMac::GetOrCreateDispatcherForWebApp(
    const webapps::AppId& web_app_id) const {
  auto& owned_dispatcher = app_specific_dispatchers_[web_app_id];
  if (!owned_dispatcher) {
    owned_dispatcher = web_app_dispatcher_factory_.Run(web_app_id);
  }
  return owned_dispatcher.get();
}
