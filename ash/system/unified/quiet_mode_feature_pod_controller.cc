// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quiet_mode_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {
namespace {

void LogUserQuietModeEvent(const bool enabled) {
  auto* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogQuietModeUkmEvent(enabled);
  }
}

}  // namespace

QuietModeFeaturePodController::QuietModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  MessageCenter::Get()->AddObserver(this);
}

QuietModeFeaturePodController::~QuietModeFeaturePodController() {
  NotifierSettingsController::Get()->RemoveNotifierSettingsObserver(this);
  MessageCenter::Get()->RemoveObserver(this);
}

FeaturePodButton* QuietModeFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuDoNotDisturbIcon);
  const bool visible =
      Shell::Get()->session_controller()->ShouldShowNotificationTray() &&
      !Shell::Get()->session_controller()->IsScreenLocked();
  button_->SetVisible(visible);
  if (visible)
    TrackVisibilityUMA();

  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NOTIFICATIONS_LABEL));
  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
      GetQuietModeStateTooltip()));
  button_->ShowDetailedViewArrow();
  NotifierSettingsController::Get()->AddNotifierSettingsObserver(this);
  OnQuietModeChanged(MessageCenter::Get()->IsQuietMode());
  return button_;
}

QsFeatureCatalogName QuietModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kQuietMode;
}

void QuietModeFeaturePodController::OnIconPressed() {
  MessageCenter* message_center = MessageCenter::Get();
  bool is_quiet_mode = message_center->IsQuietMode();
  TrackToggleUMA(/*target_toggle_state=*/!is_quiet_mode);
  LogUserQuietModeEvent(!is_quiet_mode);
  message_center->SetQuietMode(!is_quiet_mode);

  if (message_center->IsQuietMode()) {
    base::RecordAction(base::UserMetricsAction("StatusArea_QuietMode_Enabled"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_QuietMode_Disabled"));
  }
}

void QuietModeFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowNotifierSettingsView();
}

void QuietModeFeaturePodController::OnQuietModeChanged(bool in_quiet_mode) {
  button_->SetToggled(in_quiet_mode);
  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
      GetQuietModeStateTooltip()));

  if (in_quiet_mode) {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_SUBLABEL));
    button_->SetLabelTooltip(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_DO_NOT_DISTURB_TOOLTIP));
  } else if (button_->GetVisible()) {
    NotifierSettingsController::Get()->GetNotifiers();
  }
}

void QuietModeFeaturePodController::OnNotifiersUpdated(
    const std::vector<NotifierMetadata>& notifiers) {
  if (MessageCenter::Get()->IsQuietMode())
    return;

  int disabled_count = 0;
  for (const NotifierMetadata& notifier : notifiers) {
    if (!notifier.enabled)
      ++disabled_count;
  }
  RecordDisabledNotifierCount(disabled_count);

  if (disabled_count > 0) {
    button_->SetSubLabel(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_OFF_FOR_APPS_SUBLABEL,
        disabled_count));
    button_->SetLabelTooltip(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_OFF_FOR_APPS_TOOLTIP,
        disabled_count));
  } else {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ON_SUBLABEL));
    button_->SetLabelTooltip(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_ON_TOOLTIP));
  }
}

std::u16string QuietModeFeaturePodController::GetQuietModeStateTooltip() {
  return l10n_util::GetStringUTF16(
      MessageCenter::Get()->IsQuietMode()
          ? IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_ON_STATE
          : IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_OFF_STATE);
}

void QuietModeFeaturePodController::RecordDisabledNotifierCount(
    int disabled_count) {
  if (!last_disabled_count_.has_value()) {
    last_disabled_count_ = disabled_count;
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.BlockedNotifiersOnOpen",
                             disabled_count);
    return;
  }

  if (*last_disabled_count_ == disabled_count)
    return;

  last_disabled_count_ = disabled_count;
  UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.BlockedNotifiersAfterUpdate",
                           disabled_count);
}

}  // namespace ash
