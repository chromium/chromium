// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/system/message_center/arc/arc_notification_manager.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/inactive_user_notification_blocker.h"
#include "ash/system/message_center/session_state_notification_blocker.h"
#include "base/macros.h"
#include "components/arc/common/notifications.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace message_center {
struct NotifierId;
}

namespace ash {

// This class manages the ash message center and allows clients (like Chrome) to
// add and remove notifications.
class ASH_EXPORT MessageCenterController
    : public mojom::AshMessageCenterController {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  MessageCenterController();
  ~MessageCenterController() override;

  void BindRequest(mojom::AshMessageCenterControllerRequest request);

  // Called when the user has toggled a notifier in the inline settings UI.
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled);

  // mojom::AshMessageCenterController:
  void SetClient(
      mojom::AshMessageCenterClientAssociatedPtrInfo client) override;
  void SetArcNotificationsInstance(
      arc::mojom::NotificationsInstancePtr arc_notification_instance) override;
  void ShowClientNotification(
      const message_center::Notification& notification,
      const base::UnguessableToken& display_token) override;
  void CloseClientNotification(const std::string& id) override;
  void UpdateNotifierIcon(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void NotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                              bool enabled) override;
  void GetActiveNotifications(GetActiveNotificationsCallback callback) override;
  void SetQuietMode(bool enabled) override;

  // Handles get app id calls from ArcNotificationManager.
  using GetAppIdByPackageNameCallback =
      base::OnceCallback<void(const std::string& app_id)>;
  void GetArcAppIdByPackageName(const std::string& package_name,
                                GetAppIdByPackageNameCallback callback);

  // Shows the lock screen notification settings in Chrome OS setting.
  void ShowLockScreenNotificationSettings();

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return inactive_user_notification_blocker_.get();
  }

  // An interface used to listen for changes to notifier settings information,
  // implemented by the view that displays notifier settings.
  class NotifierSettingsListener {
   public:
    // Sets the user-visible and toggle-able list of notifiers.
    virtual void OnNotifierListUpdated(
        const std::vector<mojom::NotifierUiDataPtr>& ui_data) = 0;

    // Updates an icon for a notifier previously sent via OnNotifierListUpdated.
    virtual void UpdateNotifierIcon(
        const message_center::NotifierId& notifier_id,
        const gfx::ImageSkia& icon) = 0;
  };

  void AddNotifierSettingsListener(NotifierSettingsListener* listener);
  void RemoveNotifierSettingsListener(NotifierSettingsListener* listener);

  // Asks the client for the list of notifiers to display.
  void RequestNotifierSettingsUpdate();

  int disabled_notifier_count() const { return disabled_notifier_count_; }

 private:
  // Number of disabled notifier sources. Updated in OnGotNotifierList.
  int disabled_notifier_count_ = 0;

  // Callback for GetNotifierList.
  void OnGotNotifierList(std::vector<mojom::NotifierUiDataPtr> ui_data);

  std::unique_ptr<FullscreenNotificationBlocker>
      fullscreen_notification_blocker_;
  std::unique_ptr<InactiveUserNotificationBlocker>
      inactive_user_notification_blocker_;
  std::unique_ptr<SessionStateNotificationBlocker>
      session_state_notification_blocker_;
  std::unique_ptr<message_center::NotificationBlocker> all_popup_blocker_;

  base::ObserverList<NotifierSettingsListener>::Unchecked
      notifier_settings_listeners_;

  mojo::BindingSet<mojom::AshMessageCenterController> binding_set_;

  mojom::AshMessageCenterClientAssociatedPtr client_;

  std::unique_ptr<ArcNotificationManager> arc_notification_manager_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
