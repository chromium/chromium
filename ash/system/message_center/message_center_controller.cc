// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_controller.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/arc_notification_manager_delegate_impl.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::NotifierId;

namespace ash {

// static
void MessageCenterController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kMessageCenterLockScreenMode,
      prefs::kMessageCenterLockScreenModeHide,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
}

namespace {

// A notification blocker that unconditionally blocks toasts. Implements
// --suppress-message-center-notifications.
class PopupNotificationBlocker : public message_center::NotificationBlocker {
 public:
  explicit PopupNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}
  ~PopupNotificationBlocker() override = default;

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupNotificationBlocker);
};

// This notification delegate passes actions back to the client that asked for
// the notification (Chrome).
class AshClientNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  AshClientNotificationDelegate(const std::string& notification_id,
                                const base::UnguessableToken& display_token,
                                mojom::AshMessageCenterClient* client)
      : notification_id_(notification_id),
        display_token_(display_token),
        client_(client) {}

  void Close(bool by_user) override {
    client_->HandleNotificationClosed(display_token_, by_user);
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (button_index) {
      client_->HandleNotificationButtonClicked(notification_id_, *button_index,
                                               reply);
    } else {
      client_->HandleNotificationClicked(notification_id_);
    }
  }

  void SettingsClick() override {
    client_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void DisableNotification() override {
    client_->DisableNotification(notification_id_);
  }

 private:
  ~AshClientNotificationDelegate() override = default;

  // The ID of the notification.
  const std::string notification_id_;

  // The token that was generated for the ShowClientNotification() call.
  const base::UnguessableToken display_token_;

  mojom::AshMessageCenterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(AshClientNotificationDelegate);
};

}  // namespace

MessageCenterController::MessageCenterController() {
  message_center::MessageCenter::Initialize(
      std::make_unique<AshMessageCenterLockScreenController>());

  fullscreen_notification_blocker_ =
      std::make_unique<FullscreenNotificationBlocker>(MessageCenter::Get());
  inactive_user_notification_blocker_ =
      std::make_unique<InactiveUserNotificationBlocker>(MessageCenter::Get());
  session_state_notification_blocker_ =
      std::make_unique<SessionStateNotificationBlocker>(MessageCenter::Get());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSuppressMessageCenterPopups)) {
    all_popup_blocker_ =
        std::make_unique<PopupNotificationBlocker>(MessageCenter::Get());
  }

  message_center::RegisterVectorIcons({&kNotificationAssistantIcon,
                                       &kNotificationCaptivePortalIcon,
                                       &kNotificationCellularAlertIcon,
                                       &kNotificationDownloadIcon,
                                       &kNotificationEndOfSupportIcon,
                                       &kNotificationFamilyLinkIcon,
                                       &kNotificationGoogleIcon,
                                       &kNotificationImageIcon,
                                       &kNotificationInstalledIcon,
                                       &kNotificationLinuxIcon,
                                       &kNotificationMultiDeviceSetupIcon,
                                       &kNotificationMobileDataIcon,
                                       &kNotificationMobileDataOffIcon,
                                       &kNotificationPlayPrismIcon,
                                       &kNotificationPrintingDoneIcon,
                                       &kNotificationPrintingIcon,
                                       &kNotificationPrintingWarningIcon,
                                       &kNotificationSettingsIcon,
                                       &kNotificationStorageFullIcon,
                                       &kNotificationSupervisedUserIcon,
                                       &kNotificationVpnIcon,
                                       &kNotificationWarningIcon,
                                       &kNotificationWifiOffIcon});

  // Set the system notification source display name ("Chrome OS" or "Chromium
  // OS").
  message_center::MessageCenter::Get()->SetSystemNotificationAppName(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME));
}

MessageCenterController::~MessageCenterController() {
  // These members all depend on the MessageCenter instance, so must be
  // destroyed first.
  all_popup_blocker_.reset();
  session_state_notification_blocker_.reset();
  inactive_user_notification_blocker_.reset();
  fullscreen_notification_blocker_.reset();
  arc_notification_manager_.reset();

  message_center::MessageCenter::Shutdown();
}

void MessageCenterController::BindRequest(
    mojom::AshMessageCenterControllerRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

void MessageCenterController::SetNotifierEnabled(const NotifierId& notifier_id,
                                                 bool enabled) {
  client_->SetNotifierEnabled(notifier_id, enabled);
}

void MessageCenterController::SetClient(
    mojom::AshMessageCenterClientAssociatedPtrInfo client) {
  DCHECK(!client_.is_bound());
  client_.Bind(std::move(client));
}

void MessageCenterController::SetArcNotificationsInstance(
    arc::mojom::NotificationsInstancePtr arc_notification_instance) {
  if (!arc_notification_manager_) {
    arc_notification_manager_ = std::make_unique<ArcNotificationManager>(
        std::make_unique<ArcNotificationManagerDelegateImpl>(),
        Shell::Get()
            ->session_controller()
            ->GetPrimaryUserSession()
            ->user_info->account_id,
        message_center::MessageCenter::Get());
  }
  arc_notification_manager_->SetInstance(std::move(arc_notification_instance));
}

void MessageCenterController::ShowClientNotification(
    const message_center::Notification& notification,
    const base::UnguessableToken& display_token) {
  DCHECK(client_.is_bound());
  auto message_center_notification =
      std::make_unique<message_center::Notification>(notification);
  message_center_notification->set_delegate(
      base::WrapRefCounted(new AshClientNotificationDelegate(
          notification.id(), display_token, client_.get())));
  MessageCenter::Get()->AddNotification(std::move(message_center_notification));
}

void MessageCenterController::CloseClientNotification(const std::string& id) {
  MessageCenter::Get()->RemoveNotification(id, false /* by_user */);
}

void MessageCenterController::UpdateNotifierIcon(const NotifierId& notifier_id,
                                                 const gfx::ImageSkia& icon) {
  for (auto& listener : notifier_settings_listeners_)
    listener.UpdateNotifierIcon(notifier_id, icon);
}

void MessageCenterController::NotifierEnabledChanged(
    const NotifierId& notifier_id,
    bool enabled) {
  if (!enabled)
    MessageCenter::Get()->RemoveNotificationsForNotifierId(notifier_id);
}

void MessageCenterController::GetActiveNotifications(
    GetActiveNotificationsCallback callback) {
  message_center::NotificationList::Notifications notification_set =
      MessageCenter::Get()->GetVisibleNotifications();
  std::vector<message_center::Notification> notification_vector;
  notification_vector.reserve(notification_set.size());
  for (auto* notification : notification_set) {
    notification_vector.emplace_back(*notification);
    // The client doesn't know how to de-serialize vector icons,
    // nor does it need to.
    notification_vector.back().set_vector_small_image(gfx::kNoneIcon);
  }
  std::move(callback).Run(notification_vector);
}

void MessageCenterController::SetQuietMode(bool enabled) {
  MessageCenter::Get()->SetQuietMode(enabled);
}

void MessageCenterController::GetArcAppIdByPackageName(
    const std::string& package_name,
    GetAppIdByPackageNameCallback callback) {
  DCHECK(client_.is_bound());
  client_->GetArcAppIdByPackageName(package_name, std::move(callback));
}

void MessageCenterController::ShowLockScreenNotificationSettings() {
  DCHECK(client_.is_bound());
  client_->ShowLockScreenNotificationSettings();
}

void MessageCenterController::AddNotifierSettingsListener(
    NotifierSettingsListener* listener) {
  DCHECK(listener);
  notifier_settings_listeners_.AddObserver(listener);
}

void MessageCenterController::RemoveNotifierSettingsListener(
    NotifierSettingsListener* listener) {
  DCHECK(listener);
  notifier_settings_listeners_.RemoveObserver(listener);
}

void MessageCenterController::RequestNotifierSettingsUpdate() {
  // |client_| may not be bound in unit tests.
  if (!client_.is_bound())
    return;

  client_->GetNotifierList(base::BindOnce(
      &MessageCenterController::OnGotNotifierList, base::Unretained(this)));
}

void MessageCenterController::OnGotNotifierList(
    std::vector<mojom::NotifierUiDataPtr> ui_data) {
  disabled_notifier_count_ = 0;
  for (const auto& notifier : ui_data) {
    if (!notifier->enabled)
      ++disabled_notifier_count_;
  }

  for (auto& listener : notifier_settings_listeners_)
    listener.OnNotifierListUpdated(ui_data);
}

}  // namespace ash
