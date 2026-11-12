// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_management_transition_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/session/arc_management_transition.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace arc {

namespace {

// Id of the notifier.
constexpr char kNotifierId[] = "arc_management_transition";

ArcManagementTransition GetManagementTransitionFromPrefs(
    const PrefService& prefs) {
  return static_cast<ArcManagementTransition>(
      prefs.GetInteger(prefs::kArcManagementTransition));
}

// Observes following ARC events that dismisses notification.
//   * ARC opted out.
//   * management transition completed.
// If one of these events happens notification is automatically dismissed.
class NotificationDelegate : public message_center::NotificationDelegate,
                             public ArcSessionManagerObserver {
 public:
  // `prefs` must be non-null and must outlive this instance.
  explicit NotificationDelegate(PrefService* prefs)
      : prefs_(CHECK_DEREF(prefs)) {
    ArcSessionManager::Get()->AddObserver(this);
    pref_change_registrar_.Init(&prefs_.get());
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
    CHECK(!enabled, base::NotFatalUntil::M160);
    Dismiss();
  }

 private:
  ~NotificationDelegate() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  // Dismisses currently active notification.
  void Dismiss() {
    message_center::MessageCenter::Get()->RemoveNotification(
        kManagementTransitionNotificationId, /*by_user=*/false);
  }

  // Called in case transition state is changed.
  void OnTransitionChanged() {
    CHECK_EQ(ArcManagementTransition::NO_TRANSITION,
             GetManagementTransitionFromPrefs(*prefs_),
             base::NotFatalUntil::M160);
    Dismiss();
  }

  // Not owned. The user's profile prefs.
  const raw_ref<PrefService> prefs_;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;
};

const gfx::VectorIcon& GetNotificationIcon(ArcManagementTransition transition) {
  if (transition == ArcManagementTransition::UNMANAGED_TO_MANAGED) {
    return chromeos::kEnterpriseIcon;
  } else {
    return ash::kNotificationFamilyLinkIcon;
  }
}

}  // namespace

const char kManagementTransitionNotificationId[] =
    "arc_management_transition/notification";

void ShowManagementTransitionNotification(user_manager::User* user) {
  CHECK(user);
  PrefService* prefs = user->GetProfilePrefs();
  CHECK(prefs);
  const ArcManagementTransition transition =
      GetManagementTransitionFromPrefs(*prefs);
  CHECK(transition == ArcManagementTransition::CHILD_TO_REGULAR ||
            transition == ArcManagementTransition::REGULAR_TO_CHILD ||
            transition == ArcManagementTransition::UNMANAGED_TO_MANAGED,
        base::NotFatalUntil::M160);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      ash::NotificationCatalogName::kManagementTransition);
  notifier_id.profile_id = user->GetAccountId().GetUserEmail();

  auto notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kManagementTransitionNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_CHILD_TRANSITION_TITLE),
      l10n_util::GetStringUTF16(IDS_ARC_CHILD_TRANSITION_MESSAGE),
      l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE),
      notifier_id, message_center::RichNotificationData(),
      base::MakeRefCounted<NotificationDelegate>(prefs),
      GetNotificationIcon(transition),
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace arc
