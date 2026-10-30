// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_platform_bridge_mac.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mojo.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace {

void RecordAttributionOutcome(MacNotificationAttributionOutcome outcome) {
  base::UmaHistogramEnumeration("Notifications.macOS.AttributionOutcome",
                                outcome);
}

base::RepeatingCallback<void(std::set<std::string>, bool)>
CreateGetDisplayedBarrierCallback(
    size_t num_callbacks,
    GetDisplayedNotificationsCallback callback,
    std::set<std::string> initial_notifications = {}) {
  auto notifications =
      std::make_unique<std::set<std::string>>(std::move(initial_notifications));
  std::set<std::string>* notifications_ptr = notifications.get();
  auto supports_synchronization = std::make_unique<bool>(true);
  bool* supports_synchronization_ptr = supports_synchronization.get();
  auto barrier_closure = base::BarrierClosure(
      num_callbacks,
      base::BindOnce(
          [](std::unique_ptr<std::set<std::string>> notifications,
             std::unique_ptr<bool> supports_synchronization,
             GetDisplayedNotificationsCallback callback) {
            std::move(callback).Run(std::move(*notifications),
                                    *supports_synchronization);
          },
          std::move(notifications), std::move(supports_synchronization),
          std::move(callback)));

  return base::BindRepeating(
             [](std::set<std::string>* notifications_ptr,
                bool* supports_synchronization_ptr,
                std::set<std::string> notifications,
                bool supports_synchronization) {
               notifications_ptr->insert(notifications.begin(),
                                         notifications.end());
               *supports_synchronization_ptr &= supports_synchronization;
             },
             notifications_ptr, supports_synchronization_ptr)
      .Then(barrier_closure);
}

}  // namespace

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
  MacNotificationIdentifier identifier = {
      notification.id(), GetProfileId(profile), profile->IsOffTheRecord()};
  pending_displays_.erase(identifier);

  if (notification.notifier_id().web_app_id.has_value()) {
    const webapps::AppId& web_app_id = *notification.notifier_id().web_app_id;
    if (!web_app::UseNotificationAttributionForWebAppShims()) {
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kFallbackFeatureDisabled);
    } else if (!AppShimRegistry::Get()->IsAppInstalledInProfile(
                   web_app_id, profile->GetPath())) {
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kFallbackNoAppShim);
    } else {
      uint64_t display_request_id = ++next_display_request_id_;
      pending_displays_[identifier] = {display_request_id,
                                       notification.origin_url()};
      GetOrCreateDispatcherForWebApp(web_app_id)
          ->DisplayNotification(
              notification_type, profile, notification,
              base::BindOnce(&NotificationPlatformBridgeMac::
                                 OnAttributedNotificationDisplayed,
                             weak_factory_.GetWeakPtr(), web_app_id,
                             notification_type, profile->GetWeakPtr(),
                             notification, identifier, display_request_id));
      return;
    }
  }

  bool is_alert = IsAlertNotificationMac(notification);
  NotificationDispatcherMac* dispatcher =
      is_alert ? alert_dispatcher_.get() : banner_dispatcher_.get();
  dispatcher->DisplayNotification(notification_type, profile, notification,
                                  base::DoNothing());
  CloseImpl(profile, notification.id(), {dispatcher});
}

void NotificationPlatformBridgeMac::OnAttributedNotificationDisplayed(
    const webapps::AppId& web_app_id,
    NotificationHandler::Type notification_type,
    base::WeakPtr<Profile> profile,
    const message_center::Notification& notification,
    const MacNotificationIdentifier& identifier,
    uint64_t display_request_id,
    mac_notifications::mojom::DisplayNotificationResult result) {
  using DisplayNotificationResult =
      mac_notifications::mojom::DisplayNotificationResult;
  using RegistryPermissionStatus =
      AppShimRegistry::NotificationPermissionStatus;
  bool should_fallback = false;
  switch (result) {
    case DisplayNotificationResult::kSuccess:
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kAttributedPermissionGranted);
      should_fallback = false;
      break;
    case DisplayNotificationResult::kPermissionDenied:
      // macOS reports `Denied` both when the user explicitly denied/revoked
      // permission and when a prompt was merely ignored before the shim
      // restarted. Consult `AppShimRegistry` to distinguish the two; a cached
      // `kGranted` means the user just turned off permission in macOS System
      // Settings (`DisplayNotification` replied before `AppShimHost` synced),
      // while any other non-denied state is an implicit denial.
      switch (AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(
          web_app_id)) {
        case RegistryPermissionStatus::kExplicitlyDenied:
        case RegistryPermissionStatus::kGranted:
          RecordAttributionOutcome(
              MacNotificationAttributionOutcome::kAttributedPermissionDenied);
          should_fallback = false;
          break;
        case RegistryPermissionStatus::kImplicitlyDenied:
        case RegistryPermissionStatus::kNotDetermined:
        case RegistryPermissionStatus::kPromptPending:
          RecordAttributionOutcome(MacNotificationAttributionOutcome::
                                       kFallbackPermissionImplicitlyDenied);
          should_fallback = true;
          break;
      }
      break;
    case DisplayNotificationResult::kPermissionNotDetermined:
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kFallbackPermissionNotDetermined);
      should_fallback = true;
      break;
    case DisplayNotificationResult::kPermissionPromptPending:
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kFallbackPermissionPromptPending);
      should_fallback = true;
      break;
    case DisplayNotificationResult::kDeliveryFailed:
      RecordAttributionOutcome(
          MacNotificationAttributionOutcome::kAttributedDeliveryFailed);
      should_fallback = false;
      break;
    case DisplayNotificationResult::kConnectionFailed:
      if (AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(
              web_app_id) == RegistryPermissionStatus::kExplicitlyDenied) {
        RecordAttributionOutcome(MacNotificationAttributionOutcome::
                                     kAppShimConnectionFailedPermissionDenied);
        should_fallback = false;
      } else {
        RecordAttributionOutcome(MacNotificationAttributionOutcome::
                                     kFallbackAppShimConnectionFailed);
        should_fallback = true;
      }
      break;
  }

  // The notification was closed or replaced while the IPC was in flight.
  auto it = pending_displays_.find(identifier);
  if (it == pending_displays_.end() ||
      it->second.request_id != display_request_id) {
    return;
  }
  pending_displays_.erase(it);

  // The profile was destroyed while the IPC was in flight.
  if (!profile) {
    return;
  }

  NotificationDispatcherMac* web_app_dispatcher =
      GetOrCreateDispatcherForWebApp(web_app_id);
  // For non-attributed notifications `Display()` closes any existing
  // notification with the same ID in other dispatchers immediately, whereas for
  // attributed notifications we don't know which dispatchers to close until the
  // AppShim display attempt finishes.
  if (!should_fallback) {
    CloseImpl(profile.get(), notification.id(), {web_app_dispatcher});
  } else {
    bool is_alert = IsAlertNotificationMac(notification);
    NotificationDispatcherMac* fallback_dispatcher =
        is_alert ? alert_dispatcher_.get() : banner_dispatcher_.get();
    fallback_dispatcher->DisplayNotification(notification_type, profile.get(),
                                             notification, base::DoNothing());
    // Avoid calling `CloseNotificationWithId` on `web_app_dispatcher` when the
    // AppShim lacks OS permission or failed to connect (any prior notifications
    // are already gone or unreachable without relaunching).
    CloseImpl(profile.get(), notification.id(),
              {fallback_dispatcher, web_app_dispatcher});
  }
}

void NotificationPlatformBridgeMac::Close(Profile* profile,
                                          const std::string& notification_id) {
  CloseImpl(profile, notification_id);
}

void NotificationPlatformBridgeMac::CloseImpl(
    Profile* profile,
    const std::string& notification_id,
    base::span<const NotificationDispatcherMac* const> excluded_dispatchers) {
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();
  pending_displays_.erase({notification_id, profile_id, incognito});

  // Closing across multiple dispatchers is needed because an updated
  // notification may move between dispatchers (e.g. between banner and alert,
  // or between Chrome and a web app if a PWA is installed or uninstalled).
  if (!std::ranges::contains(excluded_dispatchers, banner_dispatcher_.get())) {
    banner_dispatcher_->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
  if (!std::ranges::contains(excluded_dispatchers, alert_dispatcher_.get())) {
    alert_dispatcher_->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
  // TODO(crbug.com/574102798): Avoid fanning out to unrelated app dispatchers,
  // as that can relaunch an unrelated app shim in the background if it had
  // notifications on screen when it last quit.
  for (auto& [app_id, dispatcher] : app_specific_dispatchers_) {
    if (std::ranges::contains(excluded_dispatchers, dispatcher.get())) {
      continue;
    }
    dispatcher->CloseNotificationWithId(
        {notification_id, profile_id, incognito});
  }
}

void NotificationPlatformBridgeMac::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  if (web_app::UseNotificationAttributionForWebAppShims()) {
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

  auto get_notifications_callback =
      CreateGetDisplayedBarrierCallback(2, std::move(callback));

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
  if (web_app::UseNotificationAttributionForWebAppShims()) {
    if (auto* provider = web_app::WebAppProvider::GetForWebApps(profile)) {
      web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
      for (const webapps::AppId& app_id : registrar.GetAppIds(
               web_app::WebAppFilter::SupportsOsNotifications())) {
        if (!url::IsSameOriginWith(registrar.GetAppScope(app_id), origin)) {
          continue;
        }
        if (!AppShimRegistry::Get()->IsAppInstalledInProfile(
                app_id, profile->GetPath())) {
          continue;
        }
        web_app_ids.push_back(app_id);
      }
    }
    // TODO(mek): filter by web_app_ids that actually could be displaying
    // notifications rather than all for the origin.
  }

  std::set<std::string> initial_notifications;
  for (const auto& [identifier, pending] : pending_displays_) {
    if (identifier.profile_id == profile_id &&
        identifier.incognito == incognito &&
        url::IsSameOriginWith(pending.origin_url, origin)) {
      initial_notifications.insert(identifier.notification_id);
    }
  }

  auto get_notifications_callback = CreateGetDisplayedBarrierCallback(
      2 + web_app_ids.size(), std::move(callback),
      std::move(initial_notifications));

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
  CHECK(profile, base::NotFatalUntil::M161);
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();
  base::EraseIf(pending_displays_, [&](const auto& entry) {
    return entry.first.profile_id == profile_id &&
           entry.first.incognito == incognito;
  });

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
