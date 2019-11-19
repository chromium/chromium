// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_provision_notification_service.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kManagedProvisionNotificationId[] = "arc_managed_provision";
constexpr char kManagedProvisionNotifierId[] = "arc_managed_provision";

// Singleton factory for ArcProvisionNotificationService.
class ArcProvisionNotificationServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcProvisionNotificationService,
          ArcProvisionNotificationServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcProvisionNotificationServiceFactory";

  static ArcProvisionNotificationServiceFactory* GetInstance() {
    return base::Singleton<ArcProvisionNotificationServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcProvisionNotificationServiceFactory>;
  ArcProvisionNotificationServiceFactory() {
    DependsOn(NotificationDisplayServiceFactory::GetInstance());
  }
  ~ArcProvisionNotificationServiceFactory() override = default;
};

}  // namespace

// static
ArcProvisionNotificationService*
ArcProvisionNotificationService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcProvisionNotificationServiceFactory::GetForBrowserContext(context);
}

ArcProvisionNotificationService::ArcProvisionNotificationService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context) {
  ArcSessionManager::Get()->AddObserver(this);
}

ArcProvisionNotificationService::~ArcProvisionNotificationService() {
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcProvisionNotificationService::ShowNotification() {
  Profile* profile = Profile::FromBrowserContext(context_);
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kManagedProvisionNotifierId);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kManagedProvisionNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_MANAGED_PROVISION_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_ARC_MANAGED_PROVISION_NOTIFICATION_MESSAGE,
                                 ui::GetChromeOSDeviceName()),
      gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_ARC_PLAY_STORE_OPTIN_IN_PROGRESS_NOTIFICATION)),
      l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE), GURL(),
      notifier_id, optional_fields, new message_center::NotificationDelegate());

  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
}

void ArcProvisionNotificationService::HideNotification() {
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(context_))
      ->Close(NotificationHandler::Type::TRANSIENT,
              kManagedProvisionNotificationId);
}

void ArcProvisionNotificationService::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  // Make sure no notification is shown after ARC gets disabled.
  if (!enabled)
    HideNotification();
}

void ArcProvisionNotificationService::OnArcStarted() {
  // Show notification only for Public Session (except for Demo Session) when
  // ARC is going to start.
  if (profiles::IsPublicSession() &&
      !chromeos::DemoSession::IsDeviceInDemoMode()) {
    ShowNotification();
  }
}

void ArcProvisionNotificationService::OnArcOptInManagementCheckStarted() {
  // This observer is notified at an early phase of the opt-in flow, so start
  // showing the notification if the opt-in flow happens silently (due to the
  // managed prefs), or ensure that no notification is shown otherwise.
  Profile* profile = Profile::FromBrowserContext(context_);
  if (ShouldStartArcSilentlyForManagedProfile(profile)) {
    ShowNotification();
  } else {
    HideNotification();
  }
}

void ArcProvisionNotificationService::OnArcInitialStart() {
  // The opt-in flow finished successfully, so remove the notification.
  HideNotification();
}

void ArcProvisionNotificationService::OnArcSessionStopped(
    ArcStopReason stop_reason) {
  // One of the reasons of ARC being stopped is a failure of the opt-in flow.
  // Therefore remove the notification if it is shown.
  HideNotification();
}

void ArcProvisionNotificationService::OnArcErrorShowRequested(
    ArcSupportHost::Error error) {
  // If an error happens during the opt-in flow, then the provision fails, and
  // the notification should be therefore removed if it is shown. Do this here
  // unconditionally as there should be no notification displayed otherwise
  // anyway.
  HideNotification();
}

}  // namespace arc
