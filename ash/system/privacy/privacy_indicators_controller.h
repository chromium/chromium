// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// An interface for the delegate of the privacy indicators notification,
// handling launching the app and its settings. Clients that use privacy
// indicators should provide this delegate when calling the privacy indicators
// controller API so that the API can add correct buttons to the notification
// based on the callbacks provided and appropriate actions are performed when
// clicking the buttons.
class ASH_EXPORT PrivacyIndicatorsNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit PrivacyIndicatorsNotificationDelegate(
      absl::optional<base::RepeatingClosure> launch_app_callback =
          absl::nullopt,
      absl::optional<base::RepeatingClosure> launch_settings_callback =
          absl::nullopt);

  PrivacyIndicatorsNotificationDelegate(
      const PrivacyIndicatorsNotificationDelegate&) = delete;
  PrivacyIndicatorsNotificationDelegate& operator=(
      const PrivacyIndicatorsNotificationDelegate&) = delete;

  const absl::optional<base::RepeatingClosure>& launch_app_callback() const {
    return launch_app_callback_;
  }
  const absl::optional<base::RepeatingClosure>& launch_settings_callback()
      const {
    return launch_settings_callback_;
  }

  // Sets the value for `launch_app_callback_`/`launch_settings_callback_`. Also
  // update the button indices.
  void SetLaunchAppCallback(const base::RepeatingClosure& launch_app_callback);
  void SetLaunchSettingsCallback(
      const base::RepeatingClosure& launch_settings_callback);

  // message_center::NotificationDelegate:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

 protected:
  ~PrivacyIndicatorsNotificationDelegate() override;

 private:
  // Updates the indices of notification buttons.
  void UpdateButtonIndices();

  // Callbacks for clicking the launch app and launch settings buttons.
  absl::optional<base::RepeatingClosure> launch_app_callback_;
  absl::optional<base::RepeatingClosure> launch_settings_callback_;

  // Button indices in the notification for launch app/launch settings.
  // Will be null if the particular button does not exist in the notification.
  absl::optional<int> launch_app_button_index_;
  absl::optional<int> launch_settings_button_index_;
};

// Updates privacy indicators, including the privacy indicators view and the
// privacy indicator notification(s).
void ASH_EXPORT UpdatePrivacyIndicators(
    const std::string& app_id,
    absl::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate);

// Get the id of the privacy indicators notification associated with `app_id`.
std::string ASH_EXPORT
GetPrivacyIndicatorsNotificationId(const std::string& app_id);

// Create a notification with the customized metadata for privacy indicators.
// The given scoped_refptr for `delegate` will be passed as a parameter for
// the function creating the notification. In case of adding/updating the
// notification it can be provided as a nullptr if irrelevant.
std::unique_ptr<message_center::Notification> ASH_EXPORT
CreatePrivacyIndicatorsNotification(
    const std::string& app_id,
    absl::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate);

// Add, update, or remove the privacy notification associated with the given
// `app_id`.
void ASH_EXPORT ModifyPrivacyIndicatorsNotification(
    const std::string& app_id,
    absl::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate);

// Update the `PrivacyIndicatorsTrayItemView` across all status area widgets.
void ASH_EXPORT UpdatePrivacyIndicatorsView(const std::string& app_id,
                                            bool is_camera_used,
                                            bool is_microphone_used);

// Update `PrivacyIndicatorsTrayItemView` screen share status across all status
// area widgets.
void ASH_EXPORT
UpdatePrivacyIndicatorsScreenShareStatus(bool is_screen_sharing);

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
