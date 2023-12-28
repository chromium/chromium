// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_blocker.h"

class PrefService;
class PrefChangeRegistrar;

namespace message_center {
class MessageCenter;
class Notification;
}  // namespace message_center

namespace ash {

// A notification blocker that suppresses popup notifications when the HPS
// service detects a person looking over the user's shoulder.
//
// Also manages a popup notification informing the user of which popups have
// been blocked.
//
// TODO(crbug.com/1241706): make this naming less opaque. Currently using "HPS
// notify" because it was the feature name early in development, but paths /
// identifiers will be renamed in one fell swoop.
class ASH_EXPORT SnoopingProtectionNotificationBlocker
    : public SessionObserver,
      public message_center::NotificationBlocker,
      public message_center::NotificationObserver,
      public SnoopingProtectionController::Observer,
      public message_center::MessageCenterObserver {
 public:
  // The ID of the informational popup.
  static constexpr char kInfoNotificationId[] = "hps-notify-info";

  SnoopingProtectionNotificationBlocker(
      message_center::MessageCenter* message_center,
      SnoopingProtectionController* controller);

  SnoopingProtectionNotificationBlocker(
      const SnoopingProtectionNotificationBlocker&) = delete;
  SnoopingProtectionNotificationBlocker& operator=(
      const SnoopingProtectionNotificationBlocker&) = delete;

  ~SnoopingProtectionNotificationBlocker() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // SnoopingProtectionController::Observer:
  void OnSnoopingStatusChanged(bool snooper) override;
  void OnSnoopingProtectionControllerDestroyed() override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override;

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  // Starts or stops blocking and showing the info notification based on the
  // the snooping state and the user's preferences.
  void OnBlockingActiveChanged();

  // Called when prefs::kSnoopingProtectionNotificationSuppressionEnabled
  // changes its value.
  void OnBlockingPrefChanged();

  // Returns true if we are currently blocking notifications that aren't
  // explicit exceptions.
  bool BlockingActive() const;

  // Updates the visibility and contents of the info notification if necessary.
  void UpdateInfoNotificationIfNecessary();

  // Creates a new info notification with a message dependent on the set of
  // currently-blocked notifications.
  std::unique_ptr<message_center::Notification> CreateInfoNotification() const;

  const raw_ptr<message_center::MessageCenter> message_center_;
  const raw_ptr<SnoopingProtectionController> controller_;

  bool info_popup_exists_ = false;

  // The set of popups we are currently blocking.
  std::set<std::string> blocked_popups_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<SnoopingProtectionController,
                          SnoopingProtectionController::Observer>
      controller_observation_{this};
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      message_center_observation_{this};

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Must be last.
  base::WeakPtrFactory<SnoopingProtectionNotificationBlocker> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_H_
