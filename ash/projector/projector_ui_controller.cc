// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/functional/callback_helpers.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

// A unique id to identify system notifications coming from this file.
constexpr char kProjectorNotifierId[] = "ash.projector_ui_controller";

// A unique id for system notifications reporting a generic failure.
constexpr char kProjectorErrorNotificationId[] = "projector_error_notification";

// A unique id for system notifications reporting a save failure.
constexpr char kProjectorSaveErrorNotificationId[] =
    "projector_save_error_notification";

// Shows a Projector-related notification to the user with the given parameters.
void ShowNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::NORMAL,
    const message_center::RichNotificationData& optional_fields = {},
    scoped_refptr<message_center::NotificationDelegate> delegate = nullptr,
    const gfx::VectorIcon& notification_icon = kPaletteTrayIconProjectorIcon) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_DISPLAY_SOURCE), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kProjectorNotifierId, NotificationCatalogName::kProjector),
          optional_fields, delegate, notification_icon, warning_level);

  // Remove the previous notification before showing the new one if there are
  // any.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace

// static
void ProjectorUiController::ShowFailureNotification(int message_id,
                                                    int title_id) {
  RecordCreationFlowError(message_id);
  ShowNotification(
      kProjectorErrorNotificationId, title_id, message_id,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

// static
void ProjectorUiController::ShowSaveFailureNotification() {
  RecordCreationFlowError(IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT);
  ShowNotification(
      kProjectorSaveErrorNotificationId, IDS_ASH_PROJECTOR_SAVE_FAILURE_TITLE,
      IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

ProjectorUiController::ProjectorUiController() = default;

ProjectorUiController::~ProjectorUiController() = default;

}  // namespace ash
