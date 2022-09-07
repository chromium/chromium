// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

using AppActionClosure = base::RepeatingCallback<void(void)>;

// An interface for the delegate of the privacy indicators notification,
// handling launching the app and its settings.
class ASH_EXPORT PrivacyIndicatorsNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  PrivacyIndicatorsNotificationDelegate(
      const AppActionClosure& launch_app,
      const AppActionClosure& launch_settings);
  PrivacyIndicatorsNotificationDelegate(
      const PrivacyIndicatorsNotificationDelegate&) = delete;
  PrivacyIndicatorsNotificationDelegate& operator=(
      const PrivacyIndicatorsNotificationDelegate&) = delete;

  // message_center::NotificationDelegate:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

 protected:
  ~PrivacyIndicatorsNotificationDelegate() override;

 private:
  const AppActionClosure launch_app_;
  const AppActionClosure launch_settings_;
};

// Add, update, or remove the privacy notification associated with the given
// `app_id`.
// The given scoped_refptr for `delegate` will be passed as a parameter for
// CreateSystemNotification() in case of adding/updating the notification, can
// be provided as a nullptr if irrelevant.
void ASH_EXPORT ModifyPrivacyIndicatorsNotification(
    const std::string& app_id,
    absl::optional<std::u16string> app_name,
    bool camera_is_used,
    bool microphone_is_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate);

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
