// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/scalable_iph/wallpaper_ash_notification_view.h"
#include "ash/system/message_center/message_view_factory.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::kNoLimit;
using ::chromeos::network_config::mojom::NetworkFilter;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using Observer = ::scalable_iph::ScalableIphDelegate::Observer;
using NotificationParams =
    ::scalable_iph::ScalableIphDelegate::NotificationParams;
using NotificationImageType =
    ::scalable_iph::ScalableIphDelegate::NotificationImageType;

constexpr char kNotificationSourceName[] = "ChromeOS";
constexpr char kWallpaperNotificationType[] = "wallpaper_notification_type";
constexpr char kNotifierId[] = "scalable_iph";
constexpr char kButtonIndex = 0;

bool HasOnlineNetwork(const std::vector<NetworkStatePropertiesPtr>& networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network->connection_state == ConnectionStateType::kOnline) {
      return true;
    }
  }
  return false;
}

// Adds the given `notification` to the message center after it removes any
// existing notification that has the same ID.
void AddOrReplaceNotification(
    std::unique_ptr<message_center::Notification> notification) {
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      NotificationCatalogName::kScalableIphNotification);
}

bool IsWallpaperNotification(const NotificationParams& params) {
  return params.image_type == NotificationImageType::kWallpaper;
}

message_center::NotificationType GetNotificationType(
    const NotificationParams& params) {
  switch (params.image_type) {
    case NotificationImageType::kWallpaper:
      return message_center::NOTIFICATION_TYPE_CUSTOM;
    case NotificationImageType::kNoImage:
      return message_center::NOTIFICATION_TYPE_SIMPLE;
  }
  NOTREACHED_NORETURN();
}

class ScalableIphNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  // TODO(b/284158779): Add `ActionType`.
  ScalableIphNotificationDelegate(
      std::unique_ptr<scalable_iph::IphSession> iph_session,
      std::string notification_id)
      : iph_session_(std::move(iph_session)),
        notification_id_(notification_id) {}

  // message_center::NotificationDelegate:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    if (!button_index.has_value() || button_index.value() != kButtonIndex) {
      return;
    }

    // TODO(b/284158779): Handle action.
    // iph_session->PerformAction(action_type_)

    message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                             /*by_user=*/false);
  }

  void Close(bool by_user) override {
    // TODO(b/284158779): Handle dismiss.
    // iph_session->Dismiss(action_type_, by_user);
  }

 private:
  ~ScalableIphNotificationDelegate() override = default;

  std::unique_ptr<scalable_iph::IphSession> iph_session_;
  std::string notification_id_;
};

}  // namespace

ScalableIphDelegateImpl::ScalableIphDelegateImpl(Profile* profile)
    : profile_(profile) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      receiver_cros_network_config_observer_.BindNewPipeAndPassRemote());

  QueryOnlineNetworkState();

  MessageViewFactory::SetCustomNotificationViewFactory(
      kWallpaperNotificationType,
      base::BindRepeating(&WallpaperAshNotificationView::CreateWithPreview));
}

// Remember NOT to interact with `iph_session` from the destructor. See the
// comment of `ScalableIphDelegate::ShowBubble` for details.
ScalableIphDelegateImpl::~ScalableIphDelegateImpl() {
  // Remove the custom notification view factories.
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kWallpaperNotificationType);
}

void ScalableIphDelegateImpl::ShowBubble(
    const scalable_iph::ScalableIphDelegate::BubbleParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  ash::AnchoredNudgeData nudge_data(
      params.bubble_id, NudgeCatalogName::kScalableIphBubble,
      base::UTF8ToUTF16(params.text), /*anchor_view=*/nullptr);

  nudge_data.first_button_text = base::UTF8ToUTF16(params.button.text);
  nudge_data.first_button_callback = base::BindRepeating(
      &ScalableIphDelegateImpl::OnNudgeButtonClicked,
      weak_ptr_factory_.GetWeakPtr(), params.button.action.action_type);
  nudge_data.dismiss_callback =
      base::BindRepeating(&ScalableIphDelegateImpl::OnNudgeDismissed,
                          weak_ptr_factory_.GetWeakPtr());
  ash::AnchoredNudgeManager::Get()->Show(nudge_data);
  // TODO(b/289565494): Handle life cycle of `iph_session`.
}

void ScalableIphDelegateImpl::ShowNotification(
    const NotificationParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158831): Add implementation.
  std::string notification_source_name = kNotificationSourceName;
  std::string notification_title = params.title;
  std::string notification_text = params.text;

  message_center::RichNotificationData rich_notification_data;
  CHECK(!params.button.text.empty())
      << "Scalable IPH notification should have a button";
  std::string button_text = params.button.text;
  message_center::ButtonInfo button_info;
  button_info.title = base::UTF8ToUTF16(button_text);
  rich_notification_data.buttons.push_back(button_info);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          GetNotificationType(params), params.notification_id,
          base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16(notification_text),
          base::UTF8ToUTF16(notification_source_name), GURL(), GetNotifierId(),
          rich_notification_data,
          base::MakeRefCounted<ScalableIphNotificationDelegate>(
              std::move(iph_session), params.notification_id),
          gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  if (IsWallpaperNotification(params)) {
    notification->set_custom_view_type(kWallpaperNotificationType);
  }
  AddOrReplaceNotification(std::move(notification));
}

void ScalableIphDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScalableIphDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ScalableIphDelegateImpl::IsOnline() {
  return has_online_network_;
}

int ScalableIphDelegateImpl::ClientAgeInDays() {
  const base::Time& creation_time = profile_->GetCreationTime();
  const base::TimeDelta& delta = base::Time::Now() - creation_time;
  return delta.InDaysFloored();
}

void ScalableIphDelegateImpl::PerformActionForScalableIph(
    scalable_iph::ActionType action_type) {
  // TODO(b/284158779): Implement this.
}

void ScalableIphDelegateImpl::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

void ScalableIphDelegateImpl::SetHasOnlineNetwork(bool has_online_network) {
  if (has_online_network_ == has_online_network) {
    return;
  }

  has_online_network_ = has_online_network;

  for (Observer& observer : observers_) {
    observer.OnConnectionChanged(has_online_network_);
  }
}

void ScalableIphDelegateImpl::QueryOnlineNetworkState() {
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll, kNoLimit),
      base::BindOnce(&ScalableIphDelegateImpl::OnNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIphDelegateImpl::OnNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

void ScalableIphDelegateImpl::OnNudgeButtonClicked(
    scalable_iph::ActionType action_type) {
  // TODO(b/289565494): Handle life cycle of `iph_session`.
}

void ScalableIphDelegateImpl::OnNudgeDismissed() {
  // TODO(b/289565494): Handle life cycle of `iph_session`.
}

}  // namespace ash
