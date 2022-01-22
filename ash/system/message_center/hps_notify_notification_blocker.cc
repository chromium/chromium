// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/hps_notify_notification_blocker.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

HpsNotifyNotificationBlocker::HpsNotifyNotificationBlocker(
    message_center::MessageCenter* message_center,
    HpsNotifyController* controller)
    : NotificationBlocker(message_center), controller_(controller) {
  controller_observer_.Observe(controller_);

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
}

HpsNotifyNotificationBlocker::~HpsNotifyNotificationBlocker() = default;

void HpsNotifyNotificationBlocker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  NotifyBlockingStateChanged();

  // Listen to hide/unhide notifications when the user updates their
  // preferences.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      base::BindRepeating(
          &HpsNotifyNotificationBlocker::NotifyBlockingStateChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

bool HpsNotifyNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  // Always show important system notifications.
  // An exception: SMS content is personal information.
  if (notification.notifier_id().type ==
          message_center::NotifierType::SYSTEM_COMPONENT &&
      !base::StartsWith(notification.id(), SmsObserver::kNotificationPrefix))
    return true;

  // Always show if snooping protection is disabled.
  const PrefService* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service ||
      !pref_service->GetBoolean(
          prefs::kSnoopingProtectionNotificationSuppressionEnabled))
    return true;

  // Only show notifications when there isn't a snooper detected.
  return !controller_->SnooperPresent();
}

void HpsNotifyNotificationBlocker::OnSnoopingStatusChanged(bool /*snooper*/) {
  // Need to reevaluate blocking for (i.e. un/hide) all notifications when a
  // snooper appears. This also catches disabling the snooping feature all
  // together, since that is translated to a "no snooper" event by the
  // controller.
  NotifyBlockingStateChanged();
}

}  // namespace ash
