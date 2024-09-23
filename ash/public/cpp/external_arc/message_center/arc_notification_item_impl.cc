// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_item_impl.h"

#include <utility>
#include <vector>

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_delegate.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/external_arc/message_center/metadata_utils.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using arc::mojom::ArcNotificationExpandState;
using arc::mojom::ArcNotificationPriority;

namespace ash {

namespace {

// Converts from Android notification priority to Chrome notification priority.
// On Android, PRIORITY_DEFAULT does not pop up, so this maps PRIORITY_DEFAULT
// to Chrome's -1 to adapt that behavior. Also, this maps PRIORITY_LOW and
// _HIGH to -2 and 0 respectively to adjust the value with keeping the order
// among _LOW, _DEFAULT and _HIGH. static
// TODO(yoshiki): rewrite this conversion as typemap
int ConvertAndroidPriority(ArcNotificationPriority android_priority) {
  switch (android_priority) {
    case ArcNotificationPriority::NONE:
    case ArcNotificationPriority::MIN:
      return message_center::MIN_PRIORITY;
    case ArcNotificationPriority::LOW:
    case ArcNotificationPriority::DEFAULT:
      return message_center::LOW_PRIORITY;
    case ArcNotificationPriority::HIGH:
      return message_center::HIGH_PRIORITY;
    case ArcNotificationPriority::MAX:
      return message_center::MAX_PRIORITY;
  }

  NOTREACHED() << "Invalid Priority: " << android_priority;
}

}  // anonymous namespace

ArcNotificationItemImpl::ArcNotificationItemImpl(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : manager_(manager),
      message_center_(message_center),
      profile_id_(profile_id),
      notification_key_(notification_key),
      notification_id_(kArcNotificationIdPrefix + notification_key_) {}

ArcNotificationItemImpl::~ArcNotificationItemImpl() {
  for (auto& observer : observers_)
    observer.OnItemDestroying();
}

void ArcNotificationItemImpl::OnUpdatedFromAndroid(
    arc::mojom::ArcNotificationDataPtr data,
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(notification_key_, data->key);

  bool is_setting_shown =
      ((data->shown_contents ==
        arc::mojom::ArcNotificationShownContents::SETTINGS_SHOWN) ||
       (data->shown_contents ==
        arc::mojom::ArcNotificationShownContents::SNOOZE_SHOWN));

  message_center::RichNotificationData rich_data;
  rich_data.pinned =
      (data->no_clear || data->ongoing_event)  // Unclosable notification
      || is_setting_shown;                     // Settings are unclosable
  rich_data.priority = ConvertAndroidPriority(data->priority);
  if (data->small_icon)
    rich_data.small_image = gfx::Image::CreateFrom1xBitmap(*data->small_icon);

  if (data->big_picture) {
    rich_data.image = gfx::Image::CreateFrom1xBitmap(*data->big_picture);
  }

  if (data->accessible_name.has_value()) {
    rich_data.accessible_name =
        base::UTF8ToUTF16(data->accessible_name.value());
  }

  const bool render_on_chrome =
      features::IsRenderArcNotificationsByChromeEnabled() &&
      data->render_on_chrome;

  if (render_on_chrome) {
    rich_data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;
  } else if (manager_->IsOpeningSettingsSupported() && !is_setting_shown) {
    rich_data.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
  } else {
    rich_data.settings_button_handler =
        message_center::SettingsButtonHandler::NONE;
  }

  // TODO(b/313723218): Enable snooze on Chrome rendered ARC notifications.
  rich_data.should_show_snooze_button = false;

  message_center::NotifierId notifier_id(
      message_center::NotifierType::ARC_APPLICATION,
      app_id.empty() ? kDefaultArcNotifierId : app_id);
  notifier_id.profile_id = profile_id_.GetUserEmail();

  if (data->group_key) {
    notifier_id.group_key = data->group_key;
  }

  auto notification_type =
      render_on_chrome
          ? (data->messages
                 ? message_center::NOTIFICATION_TYPE_CONVERSATION
                 : ((data->indeterminate_progress || data->progress_max != -1)
                        ? message_center::NOTIFICATION_TYPE_PROGRESS
                        : message_center::NOTIFICATION_TYPE_SIMPLE))
          : message_center::NOTIFICATION_TYPE_CUSTOM;

  auto notification = CreateNotificationFromArcNotificationData(
      notification_type, notification_id_, data.get(), notifier_id, rich_data,
      new ArcNotificationDelegate(weak_ptr_factory_.GetWeakPtr()));

  notification->set_timestamp(
      base::Time::FromMillisecondsSinceUnixEpoch(data->time));

  if (notification_type == message_center::NOTIFICATION_TYPE_CUSTOM) {
    notification->set_custom_view_type(kArcNotificationCustomViewType);
  }

  if (notification_type == message_center::NOTIFICATION_TYPE_PROGRESS) {
    notification->set_progress_status(base::UTF8ToUTF16(data->message));
  }

  if (expand_state_ != ArcNotificationExpandState::FIXED_SIZE &&
      data->expand_state != ArcNotificationExpandState::FIXED_SIZE &&
      expand_state_ != data->expand_state) {
    // Assuming changing the expand status on Android-side is manually triggered
    // by user.
    manually_expanded_or_collapsed_ = true;
  }

  type_ = data->type;
  expand_state_ = data->expand_state;

  if (shown_contents_ != data->shown_contents) {
    for (auto& observer : observers_)
      observer.OnItemContentChanged(data->shown_contents);
  }
  shown_contents_ = data->shown_contents;

  swipe_input_rect_ =
      data->swipe_input_rect ? *data->swipe_input_rect : gfx::Rect();

  notification->set_never_timeout(
      data->remote_input_state ==
      arc::mojom::ArcNotificationRemoteInputState::OPENED);

  if (!data->snapshot_image || data->snapshot_image->isNull()) {
    snapshot_ = gfx::ImageSkia();
  } else {
    snapshot_ = gfx::ImageSkia(
        gfx::ImageSkiaRep(*data->snapshot_image, data->snapshot_image_scale));
  }

  message_center_->AddNotification(std::move(notification));
}

void ArcNotificationItemImpl::OnClosedFromAndroid() {
  being_removed_by_manager_ = true;  // Closing is initiated by the manager.
  message_center_->RemoveNotification(notification_id_, false /* by_user */);
}

void ArcNotificationItemImpl::Close(bool by_user) {
  if (being_removed_by_manager_) {
    // Closing is caused by the manager, so we don't need to nofify a close
    // event to the manager.
    return;
  }

  // Do not touch its any members afterwards, because this instance will be
  // destroyed in the following call
  manager_->SendNotificationRemovedFromChrome(notification_key_);
}

void ArcNotificationItemImpl::Click() {
  manager_->SendNotificationClickedOnChrome(notification_key_);

  // This is reached when user focuses on the notification and hits enter on
  // keyboard. Mouse clicks and taps are handled separately in
  // ArcNotificationContentView.
  // TODO(b/185943161): Record this in arc::ArcMetricsService.
  UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                            arc::UserInteractionType::NOTIFICATION_INTERACTION);
}

void ArcNotificationItemImpl::OpenSettings() {
  manager_->OpenNotificationSettings(notification_key_);
}

void ArcNotificationItemImpl::DisableNotification() {
  manager_->DisableNotification(notification_key_);
}

void ArcNotificationItemImpl::OpenSnooze() {
  manager_->OpenNotificationSnoozeSettings(notification_key_);
}

void ArcNotificationItemImpl::ClickButton(const int button_index,
                                          const std::string& input) {
  manager_->SendNotificationButtonClickedOnChrome(notification_key_,
                                                  button_index, input);
}

void ArcNotificationItemImpl::ToggleExpansion() {
  switch (expand_state_) {
    case ArcNotificationExpandState::EXPANDED:
      expand_state_ = ArcNotificationExpandState::COLLAPSED;
      break;
    case ArcNotificationExpandState::COLLAPSED:
      expand_state_ = ArcNotificationExpandState::EXPANDED;
      break;
    case ArcNotificationExpandState::FIXED_SIZE:
      // Do not change the state.
      break;
  }

  manager_->SendNotificationToggleExpansionOnChrome(notification_key_);
}

void ArcNotificationItemImpl::SetExpandState(bool expanded) {
  if (expand_state_ == ArcNotificationExpandState::FIXED_SIZE) {
    // Fixed size notification does not change state, therefore we do not
    // need to set corresponding state in ARC through SetExpandState.
    return;
  }

  if (expanded && expand_state_ == ArcNotificationExpandState::COLLAPSED) {
    manager_->SendNotificationToggleExpansionOnChrome(notification_key_);
  } else if (!expanded &&
             expand_state_ == ArcNotificationExpandState::EXPANDED) {
    manager_->SendNotificationToggleExpansionOnChrome(notification_key_);
  }
}

void ArcNotificationItemImpl::OnWindowActivated(bool activated) {
  manager_->SendNotificationActivatedInChrome(notification_key_, activated);
}

void ArcNotificationItemImpl::OnRemoteInputActivationChanged(bool activated) {
  for (auto& observer : observers_)
    observer.OnRemoteInputActivationChanged(activated);
}

void ArcNotificationItemImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcNotificationItemImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcNotificationItemImpl::IncrementWindowRefCount() {
  ++window_ref_count_;
  if (window_ref_count_ == 1)
    manager_->CreateNotificationWindow(notification_key_);
}

void ArcNotificationItemImpl::DecrementWindowRefCount() {
  DCHECK_GT(window_ref_count_, 0);
  --window_ref_count_;
  if (window_ref_count_ == 0)
    manager_->CloseNotificationWindow(notification_key_);
}

const gfx::ImageSkia& ArcNotificationItemImpl::GetSnapshot() const {
  return snapshot_;
}

arc::mojom::ArcNotificationType ArcNotificationItemImpl::GetNotificationType()
    const {
  return type_;
}

ArcNotificationExpandState ArcNotificationItemImpl::GetExpandState() const {
  return expand_state_;
}

bool ArcNotificationItemImpl::IsManuallyExpandedOrCollapsed() const {
  return manually_expanded_or_collapsed_;
}

gfx::Rect ArcNotificationItemImpl::GetSwipeInputRect() const {
  return swipe_input_rect_;
}

const std::string& ArcNotificationItemImpl::GetNotificationKey() const {
  return notification_key_;
}

const std::string& ArcNotificationItemImpl::GetNotificationId() const {
  return notification_id_;
}

void ArcNotificationItemImpl::CancelPress() {
  manager_->CancelPress(notification_key_);
}

}  // namespace ash
