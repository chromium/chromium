// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/show_notification_action_performer.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace {

constexpr char kTitlePath[] = "title";
constexpr char kMessagePath[] = "message";
constexpr char kIconPath[] = "sourceIcon";
constexpr char kButtonsPath[] = "buttons";
constexpr char kLabelPath[] = "label";
constexpr char kMarkDismissedPath[] = "shouldMarkDismissed";
constexpr char kActionPath[] = "action";
constexpr char kMarkDismissedOnClosePath[] = "shouldMarkDismissOnClose";
constexpr char kLogCrOSEventsPath[] = "shouldLogCrOSEvents";
constexpr char kImagePath[] = "image";
constexpr char kNotificationIdTemplate[] = "growth_campaign_%d";

struct ShowNotificationParams {
  std::string title;
  std::string message;
  bool should_mark_dismissed_on_close = false;
  bool should_log_cros_events = false;
  raw_ptr<const gfx::VectorIcon> icon = nullptr;
  raw_ptr<const gfx::Image> image = nullptr;

  std::vector<message_center::ButtonInfo> buttons_info;
};

std::unique_ptr<ShowNotificationParams>
ParseShowNotificationActionPerformerParams(const base::Value::Dict* params) {
  if (!params) {
    CAMPAIGNS_LOG(ERROR)
        << "Empty parameter to ShowNotificationActionPerformer.";
    return nullptr;
  }

  auto show_notification_params = std::make_unique<ShowNotificationParams>();

  const auto* title = params->FindString(kTitlePath);
  const auto* message = params->FindString(kMessagePath);
  show_notification_params->title = title ? *title : std::string();
  show_notification_params->message = message ? *message : std::string();

  show_notification_params->should_mark_dismissed_on_close =
      params->FindBool(kMarkDismissedOnClosePath).value_or(false);

  show_notification_params->should_log_cros_events =
      params->FindBool(kLogCrOSEventsPath).value_or(false);

  // Set icons if available.
  const auto* icon_value = params->FindDict(kIconPath);
  if (!icon_value) {
    // TODO: b/331633771 - Consider adding default icon for notification.
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadMissingIcon);
    CAMPAIGNS_LOG(ERROR) << "icon is required for notification.";
    return nullptr;
  }

  const auto* icon = growth::VectorIcon(icon_value).GetVectorIcon();
  if (!icon) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadInvalidIcon);
    return nullptr;
  }
  show_notification_params->icon = icon;

  const auto* image_dict = params->FindDict(kImagePath);
  if (image_dict) {
    // TODO: b/341368196 - consider skip showing the notification if the image
    // type is not recognized. The payload is invalid in this case.
    show_notification_params->image = growth::Image(image_dict).GetImage();
  }

  // Set buttons info.
  const auto* buttons = params->FindList(kButtonsPath);
  if (buttons) {
    for (auto button_it = buttons->begin(); button_it != buttons->end();
         button_it++) {
      if (!button_it->is_dict()) {
        growth::RecordCampaignsManagerError(
            growth::CampaignsManagerError::kNotificationPayloadInvalidButton);
        continue;
      }

      auto* const label = button_it->GetDict().FindString(kLabelPath);
      if (!label) {
        growth::RecordCampaignsManagerError(
            growth::CampaignsManagerError::
                kNotificationPayloadMissingButtonLabel);
        continue;
      }

      show_notification_params->buttons_info.emplace_back(
          base::UTF8ToUTF16(*label));
    }
  }

  return show_notification_params;
}

std::string GetNotificationId(int campaign_id) {
  return base::StringPrintf(kNotificationIdTemplate, campaign_id);
}

}  // namespace

HandleNotificationClickAndCloseDelegate::
    HandleNotificationClickAndCloseDelegate(
        const ButtonClickCallback& click_callback,
        const CloseCallback& close_callback)
    : click_callback_(click_callback), close_callback_(close_callback) {}
HandleNotificationClickAndCloseDelegate::
    ~HandleNotificationClickAndCloseDelegate() {}

void HandleNotificationClickAndCloseDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (click_callback_.is_null()) {
    return;
  }
  click_callback_.Run(button_index);
}

void HandleNotificationClickAndCloseDelegate::Close(bool by_user) {
  if (close_callback_.is_null()) {
    return;
  }
  close_callback_.Run(by_user);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ShowNotificationActionPerformer,
                                      kBubbleIdForTesting);

ShowNotificationActionPerformer::ShowNotificationActionPerformer() = default;
ShowNotificationActionPerformer::~ShowNotificationActionPerformer() = default;

void ShowNotificationActionPerformer::Run(
    int campaign_id,
    std::optional<int> group_id,
    const base::Value::Dict* params,
    growth::ActionPerformer::Callback callback) {
  // Cache the campaign ID
  current_campaign_id_ = campaign_id;
  auto show_notification_params =
      ParseShowNotificationActionPerformerParams(params);
  if (!show_notification_params) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kInvalidNotificationPayload);
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    return;
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons = std::move(show_notification_params->buttons_info);
  if (show_notification_params->image) {
    optional_fields.image = *show_notification_params->image;
  }

  auto id = GetNotificationId(campaign_id);
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, id,
          base::UTF8ToUTF16(show_notification_params->title),
          base::UTF8ToUTF16(show_notification_params->message),
          /*display_source=*/std::u16string(),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, id,
              ash::NotificationCatalogName::kGrowthFramework),
          optional_fields,
          base::MakeRefCounted<HandleNotificationClickAndCloseDelegate>(
              base::BindRepeating(
                  &ShowNotificationActionPerformer::HandleNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr(), params, id, campaign_id,
                  group_id, show_notification_params->should_log_cros_events),
              base::BindRepeating(
                  &ShowNotificationActionPerformer::HandleNotificationClose,
                  weak_ptr_factory_.GetWeakPtr(), campaign_id, group_id,
                  show_notification_params->should_mark_dismissed_on_close,
                  show_notification_params->should_log_cros_events)),
          *show_notification_params->icon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_host_view_element_id(kBubbleIdForTesting);

  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);

  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));

  NotifyReadyToLogImpression(campaign_id, group_id,
                             show_notification_params->should_log_cros_events);
  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

growth::ActionType ShowNotificationActionPerformer::ActionType() const {
  return growth::ActionType::kShowNotification;
}

void ShowNotificationActionPerformer::HandleNotificationClose(
    int campaign_id,
    std::optional<int> group_id,
    bool should_mark_dismissed,
    bool should_log_cros_events,
    bool by_user) {
  if (!by_user) {
    return;
  }

  // Dismiss and marked the notification dismissed as it is by user action.
  NotifyButtonPressed(campaign_id, group_id, CampaignButtonId::kClose,
                      should_mark_dismissed, should_log_cros_events);
}

void ShowNotificationActionPerformer::HandleNotificationClicked(
    const base::Value::Dict* params,
    const std::string& notification_id,
    int campaign_id,
    std::optional<int> group_id,
    bool should_log_cros_events,
    std::optional<int> button_index) {
  if (!button_index) {
    // Notification message body clicked.
    return;
  }

  auto button_index_value = button_index.value();
  auto button_id = CampaignButtonId::kOthers;
  if (button_index_value == 0) {
    button_id = CampaignButtonId::kPrimary;
  } else if (button_index_value == 1) {
    button_id = CampaignButtonId::kSecondary;
  }

  const auto* buttons_value = params->FindList(kButtonsPath);
  CHECK(buttons_value);

  const auto& button_value = (*buttons_value)[button_index_value];
  if (!button_value.is_dict()) {
    CAMPAIGNS_LOG(ERROR) << "Invalid button payload.";
  }

  const auto should_mark_dismissed =
      button_value.GetDict().FindBool(kMarkDismissedPath).value_or(false);
  NotifyButtonPressed(campaign_id, group_id, button_id, should_mark_dismissed,
                      should_log_cros_events);

  const auto* action_value = button_value.GetDict().FindDict(kActionPath);
  if (!action_value) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadMissingButtonAction);
    CAMPAIGNS_LOG(ERROR) << "Missing action.";
    return;
  }
  auto action = growth::Action(action_value);
  if (action.GetActionType() == growth::ActionType::kDismiss) {
    message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                             /* by_user=*/true);
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->PerformAction(campaign_id, group_id, &action);

  // Explicitly remove the notification as the notification framework doesn't
  // automatically close at buttons click.
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /* by_user=*/true);
}
