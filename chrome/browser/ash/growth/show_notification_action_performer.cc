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
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace {

constexpr char kTitlePath[] = "title";
constexpr char kMessagePath[] = "message";
constexpr char kIconPath[] = "icon";
constexpr char kButtonsPath[] = "buttons";
constexpr char kLabelPath[] = "label";
constexpr char kActionPath[] = "action";
constexpr char kNotificationIdTemplate[] = "growth_campaign_%d";

struct ShowNotificationParams {
  std::string title;
  std::string message;
  raw_ptr<const gfx::VectorIcon> icon = nullptr;

  std::vector<message_center::ButtonInfo> buttons_info;
};

std::unique_ptr<ShowNotificationParams>
ParseShowNotificationActionPerformerParams(const base::Value::Dict* params) {
  if (!params) {
    LOG(ERROR) << "Empty parameter to ShowNotificationActionPerformer.";
    return nullptr;
  }

  auto show_notification_params = std::make_unique<ShowNotificationParams>();

  const auto* title = params->FindString(kTitlePath);
  const auto* message = params->FindString(kMessagePath);
  show_notification_params->title = title ? *title : std::string();
  show_notification_params->message = message ? *message : std::string();

  // Set icons if available.
  const auto* icon_value = params->FindDict(kIconPath);
  if (!icon_value) {
    // TODO: b/331633771 - Consider adding default icon for notification.
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadMissingIcon);
    LOG(ERROR) << "icon is required for notification.";
    return nullptr;
  }

  const auto* icon = growth::Image(icon_value).GetVectorIcon();
  if (!icon) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadInvalidIcon);
    return nullptr;
  }
  show_notification_params->icon = icon;

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

}  // namespace

ShowNotificationActionPerformer::ShowNotificationActionPerformer() = default;
ShowNotificationActionPerformer::~ShowNotificationActionPerformer() = default;

void ShowNotificationActionPerformer::Run(
    int campaign_id,
    const base::Value::Dict* params,
    growth::ActionPerformer::Callback callback) {
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

  auto id = base::StringPrintf(kNotificationIdTemplate, campaign_id);
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
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &ShowNotificationActionPerformer::HandleNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr(), params, id, campaign_id)),
          *show_notification_params->icon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);

  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));

  NotifyReadyToLogImpression(campaign_id);
  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

growth::ActionType ShowNotificationActionPerformer::ActionType() const {
  return growth::ActionType::kShowNotification;
}

void ShowNotificationActionPerformer::HandleNotificationClicked(
    const base::Value::Dict* params,
    const std::string& notification_id,
    int campaign_id,
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

  NotifyButtonPressed(campaign_id, button_id, /*should_mark_dismissed=*/true);

  const auto* buttons_value = params->FindList(kButtonsPath);
  CHECK(buttons_value);

  const auto& button_value = (*buttons_value)[button_index_value];
  if (!button_value.is_dict()) {
    LOG(ERROR) << "Invalid button payload.";
  }
  const auto* action_value = button_value.GetDict().FindDict(kActionPath);
  if (!action_value) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNotificationPayloadMissingButtonAction);
    LOG(ERROR) << "Missing action.";
    return;
  }
  auto action = growth::Action(action_value);
  if (action.GetActionType() == growth::ActionType::kDismiss) {
    message_center::MessageCenter::Get()->RemoveNotification(
        notification_id, false /* by_user */);
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->PerformAction(campaign_id, &action);
}
