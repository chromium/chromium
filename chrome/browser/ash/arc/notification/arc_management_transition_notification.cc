// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_management_transition_notification.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace arc {

namespace {

// Id of the notifier.
constexpr char kNotifierId[] = "arc_management_transition";

// Observes following ARC events that dismisses notification.
//   * ARC opted out.
//   * management transition completed.
// If one of these events happens notification is automatically dismissed.
class NotificationDelegate : public message_center::NotificationDelegate,
                             public ArcSessionManagerObserver {
 public:
  explicit NotificationDelegate(Profile* profile) : profile_(profile) {
    ArcSessionManager::Get()->AddObserver(this);
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kArcManagementTransition,
        base::BindRepeating(&NotificationDelegate::OnTransitionChanged,
                            base::Unretained(this)));
  }

  NotificationDelegate(const NotificationDelegate&) = delete;
  NotificationDelegate& operator=(const NotificationDelegate&) = delete;

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override {
    // ARC Play Store can be only opted out in case notifcation is shown.
    DCHECK(!enabled);
    Dismiss();
  }

 private:
  ~NotificationDelegate() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  // Dismisses currently active notification.
  void Dismiss() {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT,
        kManagementTransitionNotificationId);
  }

  // Called in case transition state is changed.
  void OnTransitionChanged() {
    DCHECK_EQ(ArcManagementTransition::NO_TRANSITION,
              GetManagementTransition(profile_));
    Dismiss();
  }

  // Not owned.
  const raw_ptr<Profile> profile_;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;
};

const gfx::VectorIcon& GetNotificationIcon(ArcManagementTransition transition) {
  if (transition == ArcManagementTransition::UNMANAGED_TO_MANAGED) {
    return chromeos::kEnterpriseIcon;
  } else {
    return kNotificationFamilyLinkIcon;
  }
}

}  // namespace

const char kManagementTransitionNotificationId[] =
    "arc_management_transition/notification";

void ShowManagementTransitionNotification(Profile* profile) {
  const ArcManagementTransition transition = GetManagementTransition(profile);
  DCHECK(transition == ArcManagementTransition::CHILD_TO_REGULAR ||
         transition == ArcManagementTransition::REGULAR_TO_CHILD ||
         transition == ArcManagementTransition::UNMANAGED_TO_MANAGED);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      ash::NotificationCatalogName::kManagementTransition);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kManagementTransitionNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_CHILD_TRANSITION_TITLE),
      l10n_util::GetStringUTF16(IDS_ARC_CHILD_TRANSITION_MESSAGE),
      l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE), GURL(),
      notifier_id, message_center::RichNotificationData(),
      new NotificationDelegate(profile), GetNotificationIcon(transition),
      message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

}  // namespace arc
