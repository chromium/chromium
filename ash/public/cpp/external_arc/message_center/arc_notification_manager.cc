// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"

#include <memory>
#include <utility>

#include "ash/components/arc/session/mojo_channel.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/arc_app_id_provider.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_delegate.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item_impl.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/external_arc/message_center/metrics_utils.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/public/cpp/message_center/arc_notification_manager_delegate.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_observer.h"

using arc::ConnectionHolder;
using arc::MojoChannel;
using arc::mojom::ArcDoNotDisturbStatus;
using arc::mojom::ArcDoNotDisturbStatusPtr;
using arc::mojom::ArcNotificationData;
using arc::mojom::ArcNotificationDataPtr;
using arc::mojom::ArcNotificationEvent;
using arc::mojom::ArcNotificationPriority;
using arc::mojom::MessageCenterVisibility;
using arc::mojom::NotificationConfiguration;
using arc::mojom::NotificationConfigurationPtr;
using arc::mojom::NotificationsHost;
using arc::mojom::NotificationsInstance;

namespace ash {
namespace {

constexpr char kPlayStorePackageName[] = "com.android.vending";
constexpr char kArcGmsPackageName[] = "org.chromium.arc.gms";
constexpr char kArcHostVpnPackageName[] = "org.chromium.arc.hostvpn";

constexpr char kManagedProvisioningPackageName[] =
    "com.android.managedprovisioning";

std::unique_ptr<message_center::MessageView> CreateCustomMessageView(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  DCHECK_EQ(notification.notifier_id().type,
            message_center::NotifierType::ARC_APPLICATION);
  DCHECK_EQ(kArcNotificationCustomViewType, notification.custom_view_type());
  auto* arc_delegate =
      static_cast<ArcNotificationDelegate*>(notification.delegate());
  return arc_delegate->CreateCustomMessageView(notification, shown_in_popup);
}

class DoNotDisturbManager : public message_center::MessageCenterObserver {
 public:
  explicit DoNotDisturbManager(ArcNotificationManager* manager)
      : manager_(manager) {}

  DoNotDisturbManager(const DoNotDisturbManager&) = delete;
  DoNotDisturbManager& operator=(const DoNotDisturbManager&) = delete;

  void OnQuietModeChanged(bool in_quiet_mode) override {
    manager_->SetDoNotDisturbStatusOnAndroid(in_quiet_mode);
  }

 private:
  const raw_ptr<ArcNotificationManager> manager_;
};

class VisibilityManager : public message_center::MessageCenterObserver {
 public:
  explicit VisibilityManager(ArcNotificationManager* manager)
      : manager_(manager) {}

  VisibilityManager(const VisibilityManager&) = delete;
  VisibilityManager& operator=(const VisibilityManager&) = delete;

  void OnCenterVisibilityChanged(
      message_center::Visibility visibility) override {
    manager_->OnMessageCenterVisibilityChanged(toMojom(visibility));
  }

 private:
  static MessageCenterVisibility toMojom(
      message_center::Visibility visibility) {
    if (visibility == message_center::Visibility::VISIBILITY_TRANSIENT)
      return MessageCenterVisibility::VISIBILITY_TRANSIENT;
    if (visibility == message_center::Visibility::VISIBILITY_MESSAGE_CENTER)
      return MessageCenterVisibility::VISIBILITY_MESSAGE_CENTER;
    VLOG(2) << "Unknown message_center::Visibility: " << visibility;
    return MessageCenterVisibility::VISIBILITY_TRANSIENT;
  }

  const raw_ptr<ArcNotificationManager> manager_;
};

}  // namespace

class ArcNotificationManager::InstanceOwner {
 public:
  InstanceOwner() = default;

  InstanceOwner(const InstanceOwner&) = delete;
  InstanceOwner& operator=(const InstanceOwner&) = delete;

  ~InstanceOwner() = default;

  void SetInstanceRemote(
      mojo::PendingRemote<arc::mojom::NotificationsInstance> instance_remote) {
    DCHECK(!channel_);

    channel_ =
        std::make_unique<MojoChannel<NotificationsInstance, NotificationsHost>>(
            &holder_, std::move(instance_remote));

    // Using base::Unretained because |this| owns |channel_|.
    channel_->set_disconnect_handler(
        base::BindOnce(&InstanceOwner::OnDisconnected, base::Unretained(this)));
    channel_->QueryVersion();
  }

  ConnectionHolder<NotificationsInstance, NotificationsHost>* holder() {
    return &holder_;
  }

 private:
  void OnDisconnected() { channel_.reset(); }

  ConnectionHolder<NotificationsInstance, NotificationsHost> holder_;
  std::unique_ptr<MojoChannel<NotificationsInstance, NotificationsHost>>
      channel_;
};

// static
void ArcNotificationManager::SetCustomNotificationViewFactory() {
  MessageViewFactory::SetCustomNotificationViewFactory(
      kArcNotificationCustomViewType,
      base::BindRepeating(&CreateCustomMessageView));
}

ArcNotificationManager::ArcNotificationManager()
    : instance_owner_(std::make_unique<InstanceOwner>()) {}

ArcNotificationManager::~ArcNotificationManager() {
  for (auto& obs : observers_)
    obs.OnArcNotificationManagerDestroyed(this);

  message_center_->RemoveObserver(do_not_disturb_manager_.get());
  message_center_->RemoveObserver(visibility_manager_.get());

  instance_owner_->holder()->RemoveObserver(this);
  instance_owner_->holder()->SetHost(nullptr);

  // Ensures that any callback tied to |instance_owner_| is not invoked.
  instance_owner_.reset();
}

void ArcNotificationManager::SetInstance(
    mojo::PendingRemote<arc::mojom::NotificationsInstance> instance_remote) {
  instance_owner_->SetInstanceRemote(std::move(instance_remote));
}

ConnectionHolder<NotificationsInstance, NotificationsHost>*
ArcNotificationManager::GetConnectionHolderForTest() {
  return instance_owner_->holder();
}

void ArcNotificationManager::OnConnectionReady() {
  DCHECK(!ready_);

  // TODO(hidehiko): Replace this by ConnectionHolder::IsConnected().
  ready_ = true;

  // Sync the initial quiet mode state with Android.
  SetDoNotDisturbStatusOnAndroid(message_center_->IsQuietMode());

  // Sync the initial visibility of message center with Android.
  auto visibility = message_center_->IsMessageCenterVisible()
                        ? MessageCenterVisibility::VISIBILITY_MESSAGE_CENTER
                        : MessageCenterVisibility::VISIBILITY_TRANSIENT;
  OnMessageCenterVisibilityChanged(visibility);

  // Set configuration variables for notifications on arc.
  SetNotificationConfiguration();
}

void ArcNotificationManager::OnConnectionClosed() {
  DCHECK(ready_);
  while (!items_.empty()) {
    auto it = items_.begin();
    std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
    items_.erase(it);
    item->OnClosedFromAndroid();
  }
  ready_ = false;
}

void ArcNotificationManager::OnNotificationPosted(ArcNotificationDataPtr data) {
  if (ShouldIgnoreNotification(data.get())) {
    VLOG(3) << "Posted notification was ignored.";
    return;
  }

  const bool render_on_chrome =
      features::IsRenderArcNotificationsByChromeEnabled() &&
      data->render_on_chrome;
  if (render_on_chrome && data->children_data) {
    const auto& children = *data->children_data;
    for (size_t i = 0; i < children.size(); ++i) {
      OnNotificationPosted(children[i]->Clone());
    }
    return;
  }

  const std::string key = data->key;
  auto it = items_.find(key);
  if (it == items_.end()) {
    // Show a notification on the primary logged-in user's desktop and badge the
    // app icon in the shelf if the icon exists.
    // TODO(yoshiki): Reconsider when ARC supports multi-user.
    auto item = std::make_unique<ArcNotificationItemImpl>(
        this, message_center_, key, main_profile_id_);
    auto result = items_.insert(std::make_pair(key, std::move(item)));
    DCHECK(result.second);
    it = result.first;

    metrics_utils::LogArcNotificationStyle(data->style);
    metrics_utils::LogArcNotificationActionEnabled(data->is_action_enabled);
    metrics_utils::LogArcNotificationInlineReplyEnabled(
        data->is_inline_reply_enabled);
    metrics_utils::LogArcNotificationIsCustomNotification(
        data->is_custom_notification);
  }

  const std::string app_id =
      data->package_name
          ? ArcAppIdProvider::Get()->GetAppIdByPackageName(*data->package_name)
          : std::string();
  it->second->OnUpdatedFromAndroid(std::move(data), app_id);

  // OnUpdatedFromAndroid may remove the new notification if the number of
  // notifications are limited.
  it = items_.find(key);
  if (it != items_.end()) {
    const std::string notification_id = it->second->GetNotificationId();
    for (auto& observer : observers_) {
      observer.OnNotificationUpdated(notification_id, app_id);
    }
  }
}

void ArcNotificationManager::OnNotificationUpdated(
    ArcNotificationDataPtr data) {
  if (ShouldIgnoreNotification(data.get())) {
    VLOG(3) << "Updated notification was ignored.";
    return;
  }

  const std::string& key = data->key;
  auto it = items_.find(key);
  if (it == items_.end())
    return;

  bool is_remote_view_focused =
      (data->remote_input_state ==
       arc::mojom::ArcNotificationRemoteInputState::OPENED);
  if (is_remote_view_focused && (previously_focused_notification_key_ != key)) {
    if (!previously_focused_notification_key_.empty()) {
      auto prev_it = items_.find(previously_focused_notification_key_);
      // The case that another remote input is focused. Notify the previously-
      // focused notification (if any).
      if (prev_it != items_.end())
        prev_it->second->OnRemoteInputActivationChanged(false);
    }

    // Notify the newly-focused notification.
    previously_focused_notification_key_ = key;
    it->second->OnRemoteInputActivationChanged(true);
  } else if (!is_remote_view_focused &&
             (previously_focused_notification_key_ == key)) {
    // The case that the previously-focused notification gets unfocused. Notify
    // the previously-focused notification if the notification still exists.
    auto previous_it = items_.find(previously_focused_notification_key_);
    if (previous_it != items_.end())
      previous_it->second->OnRemoteInputActivationChanged(false);

    previously_focused_notification_key_.clear();
  }

  std::string app_id =
      data->package_name
          ? ArcAppIdProvider::Get()->GetAppIdByPackageName(*data->package_name)
          : std::string();
  it->second->OnUpdatedFromAndroid(data->Clone(), app_id);

  for (auto& observer : observers_)
    observer.OnNotificationUpdated(it->second->GetNotificationId(), app_id);

  const bool render_on_chrome =
      features::IsRenderArcNotificationsByChromeEnabled() &&
      data->render_on_chrome;
  if (render_on_chrome && data->children_data) {
    const auto& children = *data->children_data;
    for (size_t i = 0; i < children.size(); ++i) {
      const auto& child = children[i];
      const std::string& child_key = child->key;
      auto child_it = items_.find(child_key);
      if (child_it == items_.end()) {
        OnNotificationPosted(child->Clone());
      } else {
        OnNotificationUpdated(child->Clone());
      }
    }
    return;
  }
}

void ArcNotificationManager::OpenMessageCenter() {
  delegate_->ShowMessageCenter();
}

void ArcNotificationManager::CloseMessageCenter() {
  delegate_->HideMessageCenter();
}

void ArcNotificationManager::OnLockScreenSettingUpdated(
    arc::mojom::ArcLockScreenNotificationSettingPtr setting) {
  // TODO(yoshiki): sync the setting.
}

void ArcNotificationManager::ProcessUserAction(
    arc::mojom::ArcNotificationUserActionDataPtr data) {
  if (!data->defer_until_unlock) {
    PerformUserAction(data->action_id, data->to_be_focused_after_unlock);
    return;
  }

  // TODO(yoshiki): remove the static cast after refactionring.
  static_cast<message_center::MessageCenterImpl*>(message_center_)
      ->lock_screen_controller()
      ->DismissLockScreenThenExecute(
          base::BindOnce(&ArcNotificationManager::PerformUserAction,
                         weak_ptr_factory_.GetWeakPtr(), data->action_id,
                         data->to_be_focused_after_unlock),
          base::BindOnce(&ArcNotificationManager::CancelUserAction,
                         weak_ptr_factory_.GetWeakPtr(), data->action_id));
}

void ArcNotificationManager::PerformUserAction(uint32_t id,
                                               bool open_message_center) {
  // TODO(yoshiki): Pass the event to the message center and handle the action
  // in the NotificationDelegate::Click() for consistency with non-arc
  // notifications.
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), PerformDeferredUserAction);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to perform the defered operation, "
               "but the ARC channel has already gone.";
    return;
  }

  notifications_instance->PerformDeferredUserAction(id);

  if (open_message_center) {
    OpenMessageCenter();
    // TODO(yoshiki): focus the target notification after opening the message
    // center.
  }
}

void ArcNotificationManager::CancelUserAction(uint32_t id) {
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), CancelDeferredUserAction);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to cancel the defered operation, "
               "but the ARC channel has already gone.";
    return;
  }

  notifications_instance->CancelDeferredUserAction(id);
}

void ArcNotificationManager::LogInlineReplySent(const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    return;
  }
  metrics_utils::LogInlineReplySent(it->second->GetNotificationId(),
                                    !message_center_->IsMessageCenterVisible());
}

void ArcNotificationManager::OnNotificationRemoved(const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    VLOG(3) << "Android requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
  items_.erase(it);
  item->OnClosedFromAndroid();

  for (auto& observer : observers_)
    observer.OnNotificationRemoved(item->GetNotificationId());
}

void ArcNotificationManager::SendNotificationRemovedFromChrome(
    const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    VLOG(3) << "Chrome requests to remove a notification (key: " << key
            << "), but it is already gone.";
    return;
  }

  // The removed ArcNotificationItem needs to live in this scope, since the
  // given argument |key| may be a part of the removed item.
  std::unique_ptr<ArcNotificationItem> item = std::move(it->second);
  items_.erase(it);

  for (auto& observer : observers_)
    observer.OnNotificationRemoved(item->GetNotificationId());

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is closed, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::CLOSED);
}

void ArcNotificationManager::SendNotificationClickedOnChrome(
    const std::string& key) {
  if (!base::Contains(items_, key)) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::BODY_CLICKED);
}

void ArcNotificationManager::SendNotificationActivatedInChrome(
    const std::string& key,
    bool activated) {
  if (!base::Contains(items_, key)) {
    VLOG(3)
        << "Chrome requests to fire an activation event on notification (key: "
        << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is (de)activated, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, activated ? ArcNotificationEvent::ACTIVATED
                     : ArcNotificationEvent::DEACTIVATED);
}

void ArcNotificationManager::SendNotificationButtonClickedOnChrome(
    const std::string& key,
    const int button_index,
    const std::string& input) {
  if (!base::Contains(items_, key)) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationButtonClickToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ")'s button is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationButtonClickToAndroid(
      key, button_index, input);
}

void ArcNotificationManager::CreateNotificationWindow(const std::string& key) {
  if (!base::Contains(items_, key)) {
    VLOG(3) << "Chrome requests to create window on notification (key: " << key
            << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), CreateNotificationWindow);
  if (!notifications_instance)
    return;

  notifications_instance->CreateNotificationWindow(key);
}

void ArcNotificationManager::CloseNotificationWindow(const std::string& key) {
  if (!base::Contains(items_, key)) {
    VLOG(3) << "Chrome requests to close window on notification (key: " << key
            << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), CloseNotificationWindow);
  if (!notifications_instance)
    return;

  notifications_instance->CloseNotificationWindow(key);
}

void ArcNotificationManager::OpenNotificationSettings(const std::string& key) {
  if (!base::Contains(items_, key)) {
    DVLOG(3) << "Chrome requests to fire a click event on the notification "
             << "settings button (key: " << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OpenNotificationSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance)
    return;

  notifications_instance->OpenNotificationSettings(key);
}

void ArcNotificationManager::DisableNotification(const std::string& key) {
  if (!base::Contains(items_, key)) {
    DVLOG(3) << "Chrome requests to fire a DisableNotification event on the "
             << "notification  (key: " << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), PopUpAppNotificationSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    return;
  }

  notifications_instance->PopUpAppNotificationSettings(key);
}

void ArcNotificationManager::OpenNotificationSnoozeSettings(
    const std::string& key) {
  if (!base::Contains(items_, key)) {
    DVLOG(3) << "Chrome requests to show a snooze setting gut on the"
             << "notification (key: " << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OpenNotificationSnoozeSettings);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance)
    return;

  notifications_instance->OpenNotificationSnoozeSettings(key);
}

bool ArcNotificationManager::IsOpeningSettingsSupported() const {
  const auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OpenNotificationSettings);
  return notifications_instance != nullptr;
}

void ArcNotificationManager::SendNotificationToggleExpansionOnChrome(
    const std::string& key) {
  if (!base::Contains(items_, key)) {
    VLOG(3) << "Chrome requests to fire a click event on notification (key: "
            << key << "), but it is gone.";
    return;
  }

  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SendNotificationEventToAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "ARC Notification (key: " << key
            << ") is clicked, but the ARC channel has already gone.";
    return;
  }

  notifications_instance->SendNotificationEventToAndroid(
      key, ArcNotificationEvent::TOGGLE_EXPANSION);
}

bool ArcNotificationManager::ShouldIgnoreNotification(
    ArcNotificationData* data) {
  if (data->priority == ArcNotificationPriority::NONE)
    return true;

  // Notifications from Play Store are ignored in Managed Guest Session and
  // Kiosk mode.
  // TODO (sarakato): Use centralized const for Play Store package.
  if (data->package_name.has_value() &&
      *data->package_name == kPlayStorePackageName &&
      delegate_->IsManagedGuestSessionOrKiosk()) {
    return true;
  }

  // (b/186419166) Ignore notifications from managed provisioning and ARC GMS
  // Proxy.
  // (b/147256449) Ignore notifications from facade VPN app
  if (data->package_name.has_value() &&
      (*data->package_name == kManagedProvisioningPackageName ||
       *data->package_name == kArcGmsPackageName ||
       *data->package_name == kArcHostVpnPackageName)) {
    return true;
  }

  // Media Notifications are ignored because we show native views-based media
  // session notifications instead.
  if (data->is_media_notification) {
    return true;
  }

  return false;
}

void ArcNotificationManager::OnDoNotDisturbStatusUpdated(
    ArcDoNotDisturbStatusPtr status) {
  // Remove the observer to prevent from sending the command to Android since
  // this request came from Android.
  message_center_->RemoveObserver(do_not_disturb_manager_.get());

  if (message_center_->IsQuietMode() != status->enabled)
    message_center_->SetQuietMode(status->enabled);

  // Add back the observer.
  message_center_->AddObserver(do_not_disturb_manager_.get());
}

void ArcNotificationManager::SetDoNotDisturbStatusOnAndroid(bool enabled) {
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SetDoNotDisturbStatusOnAndroid);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to send the Do Not Disturb status (" << enabled
            << "), but the ARC channel has already gone.";
    return;
  }

  ArcDoNotDisturbStatusPtr status = ArcDoNotDisturbStatus::New();
  status->enabled = enabled;

  notifications_instance->SetDoNotDisturbStatusOnAndroid(std::move(status));
}

void ArcNotificationManager::CancelPress(const std::string& key) {
  auto* notifications_instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_owner_->holder(), CancelPress);

  // On shutdown, the ARC channel may quit earlier than notifications.
  if (!notifications_instance) {
    VLOG(2) << "Trying to cancel the long press operation, "
               "but the ARC channel has already gone.";
    return;
  }

  notifications_instance->CancelPress(key);
}

void ArcNotificationManager::SetNotificationConfiguration() {
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), SetNotificationConfiguration);

  if (!notifications_instance) {
    VLOG(2) << "Trying to set notification expansion animations"
            << ", but the ARC channel has already gone.";
    return;
  }

  NotificationConfigurationPtr configuration = NotificationConfiguration::New();
  configuration->expansion_animation =
      features::IsNotificationExpansionAnimationEnabled();

  notifications_instance->SetNotificationConfiguration(
      std::move(configuration));
}

void ArcNotificationManager::OnMessageCenterVisibilityChanged(
    MessageCenterVisibility visibility) {
  auto* notifications_instance = ARC_GET_INSTANCE_FOR_METHOD(
      instance_owner_->holder(), OnMessageCenterVisibilityChanged);

  if (!notifications_instance) {
    VLOG(2) << "Trying to report message center visibility (" << visibility
            << "), but the ARC channel has already gone.";
    return;
  }

  notifications_instance->OnMessageCenterVisibilityChanged(visibility);
}

void ArcNotificationManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcNotificationManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcNotificationManager::Init(
    std::unique_ptr<ArcNotificationManagerDelegate> delegate,
    const AccountId& main_profile_id,
    message_center::MessageCenter* message_center) {
  DCHECK(message_center);
  delegate_ = std::move(delegate);
  main_profile_id_ = main_profile_id;
  message_center_ = message_center;
  do_not_disturb_manager_ = std::make_unique<DoNotDisturbManager>(this);
  visibility_manager_ = std::make_unique<VisibilityManager>(this);

  instance_owner_->holder()->SetHost(this);
  instance_owner_->holder()->AddObserver(this);
  if (!MessageViewFactory::HasCustomNotificationViewFactory(
          kArcNotificationCustomViewType)) {
    SetCustomNotificationViewFactory();
  }
  message_center_->AddObserver(do_not_disturb_manager_.get());
  message_center_->AddObserver(visibility_manager_.get());
}

}  // namespace ash
