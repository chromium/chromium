// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"

#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kOneMinute = base::Minutes(1);
constexpr base::TimeDelta kOneSecond = base::Seconds(1);

std::u16string RemainingTimeString(base::TimeDelta remaining_time) {
  if (remaining_time > kOneMinute) {
    return l10n_util::GetStringFUTF16Int(
        IDS_ASH_STATUS_TRAY_NEARBY_SHARE_REMAINING_MINUTES,
        remaining_time.InMinutes() + 1);
  }

  return l10n_util::GetStringFUTF16Int(
      IDS_ASH_STATUS_TRAY_NEARBY_SHARE_REMAINING_SECONDS,
      static_cast<int>(remaining_time.InSeconds()) + 1);
}

std::u16string IconTooltipString(bool is_enabled) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NEARBY_SHARE_ICON_TOOLTIP,
      l10n_util::GetStringUTF16(
          is_enabled ? IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_ON
                     : IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_OFF));
}

}  // namespace

NearbyShareFeaturePodController::NearbyShareFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : countdown_timer_(
          FROM_HERE,
          kOneSecond,
          base::BindRepeating(&NearbyShareFeaturePodController::UpdateButton,
                              base::Unretained(this),
                              /*enabled=*/true)),
      tray_controller_(tray_controller),
      nearby_share_delegate_(Shell::Get()->nearby_share_delegate()),
      nearby_share_controller_(Shell::Get()->nearby_share_controller()) {
  DCHECK(tray_controller_);
  DCHECK(nearby_share_delegate_);
  DCHECK(nearby_share_controller_);
  nearby_share_controller_->AddObserver(this);
}

NearbyShareFeaturePodController::~NearbyShareFeaturePodController() {
  nearby_share_controller_->RemoveObserver(this);
}

std::unique_ptr<FeatureTile> NearbyShareFeaturePodController::CreateTile(
    bool compact) {
  std::unique_ptr<FeatureTile> tile;
  if (chromeos::features::IsQuickShareV2Enabled()) {
    tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnLabelPressed,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  tile_ = tile.get();

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  if (chromeos::features::IsQuickShareV2Enabled()) {
    const bool target_visibility =
        nearby_share_delegate_->IsPodButtonVisible() &&
        session_controller->IsActiveUserSessionStarted() &&
        session_controller->IsUserPrimary() &&
        !session_controller->IsUserSessionBlocked();
    tile_->SetVisible(target_visibility);
    if (target_visibility) {
      TrackVisibilityUMA();
    }
    tile_->SetLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL));
    tile_->CreateDecorativeDrillInArrow();
    tile_->SetIconClickable(true);
    tile_->SetIconClickCallback(
        base::BindRepeating(&NearbyShareFeaturePodController::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()));

    // Set tile appearance.
    UpdateQSv2Button();
  } else {
    const bool target_visibility =
        nearby_share_delegate_->IsPodButtonVisible() &&
        session_controller->IsActiveUserSessionStarted() &&
        session_controller->IsUserPrimary() &&
        !session_controller->IsUserSessionBlocked() &&
        nearby_share_delegate_->IsEnabled();
    tile_->SetVisible(target_visibility);
    if (target_visibility) {
      TrackVisibilityUMA();
    }
    const std::u16string feature_name =
        nearby_share_delegate_->GetPlaceholderFeatureName();
    tile_->SetLabel(feature_name.empty()
                        ? l10n_util::GetStringUTF16(
                              IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL)
                        : l10n_util::GetStringFUTF16(
                              IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_PH,
                              feature_name));
    tile_->SetTooltipText(
        feature_name.empty()
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TOGGLE_TOOLTIP)
            : l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TOGGLE_TOOLTIP_PH,
                  feature_name));
    bool enabled = nearby_share_delegate_->IsHighVisibilityOn();
    OnHighVisibilityEnabledChanged(enabled);
  }
  return tile;
}

QsFeatureCatalogName NearbyShareFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kNearbyShare;
}

void NearbyShareFeaturePodController::OnIconPressed() {
  if (chromeos::features::IsQuickShareV2Enabled()) {
    CHECK(nearby_share_delegate_);

    if (nearby_share_delegate_->IsOnboardingComplete()) {
      nearby_share_delegate_->SetEnabled(!nearby_share_delegate_->IsEnabled());
      UpdateQSv2Button();
      return;
    }

    nearby_share_delegate_->ShowOnboardingPage();
    return;
  }

  TrackToggleUMA(
      /*target_toggle_state=*/!nearby_share_delegate_->IsHighVisibilityOn());
  if (nearby_share_delegate_->IsHighVisibilityOn()) {
    nearby_share_delegate_->DisableHighVisibility();
  } else {
    nearby_share_delegate_->EnableHighVisibility();
  }
}

void NearbyShareFeaturePodController::OnLabelPressed() {
  if (chromeos::features::IsQuickShareV2Enabled()) {
    CHECK(nearby_share_delegate_);
    if (nearby_share_delegate_->IsOnboardingComplete()) {
      tray_controller_->ShowNearbyShareDetailedView();
      return;
    }

    nearby_share_delegate_->ShowOnboardingPage();
    return;
  }

  TrackDiveInUMA();
  nearby_share_delegate_->ShowNearbyShareSettings();
}

void NearbyShareFeaturePodController::OnHighVisibilityEnabledChanged(
    bool enabled) {
  if (chromeos::features::IsQuickShareV2Enabled()) {
    enabled ? countdown_timer_.Reset() : countdown_timer_.Stop();
    return;
  }

  if (enabled) {
    shutoff_time_ = nearby_share_delegate_->HighVisibilityShutoffTime();
    countdown_timer_.Reset();
  } else {
    countdown_timer_.Stop();
  }
  UpdateButton(enabled);
}

void NearbyShareFeaturePodController::OnNearbyShareEnabledChanged(
    bool enabled) {
  UpdateQSv2Button();
}

void NearbyShareFeaturePodController::OnVisibilityChanged(
    ::nearby_share::mojom::Visibility visibility) {
  UpdateQSv2Button();
}

void NearbyShareFeaturePodController::UpdateButton(bool enabled) {
  tile_->SetToggled(enabled);

  auto& icon = nearby_share_delegate_->GetIcon(/*on_icon=*/enabled);
  if (enabled) {
    tile_->SetVectorIcon(icon.is_empty() ? kQuickSettingsNearbyShareOnIcon
                                         : icon);
    tile_->SetSubLabel(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_ON_STATE,
        RemainingTimeString(RemainingHighVisibilityTime())));

  } else {
    tile_->SetVectorIcon(icon.is_empty() ? kQuickSettingsNearbyShareOffIcon
                                         : icon);
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_OFF_STATE));
  }
}

void NearbyShareFeaturePodController::UpdateQSv2Button() {
  if (!chromeos::features::IsQuickShareV2Enabled()) {
    return;
  }
  CHECK(nearby_share_delegate_);

  nearby_share_delegate_->IsEnabled() ? ToggleTileOn() : ToggleTileOff();
}

void NearbyShareFeaturePodController::ToggleTileOn() {
  CHECK(nearby_share_delegate_);
  CHECK(tile_);

  auto& on_icon = nearby_share_delegate_->GetIcon(/*on_icon=*/true);
  tile_->SetVectorIcon(on_icon.is_empty() ? kQuickSettingsNearbyShareOnIcon
                                          : on_icon);
  tile_->SetIconButtonTooltipText(IconTooltipString(/*is_enabled=*/true));
  tile_->SetToggled(true);

  const std::u16string remaining_time_string =
      RemainingTimeString(RemainingHighVisibilityTime());

  if (nearby_share_delegate_->IsHighVisibilityOn()) {
    tile_->SetSubLabel(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_ON_STATE, remaining_time_string));
    tile_->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_TOOLTIP_HIGH_VISIBILITY,
        remaining_time_string));
    return;
  }

  switch (nearby_share_delegate_->GetVisibility()) {
    case ::nearby_share::mojom::Visibility::kYourDevices:
      tile_->SetSubLabel(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_YOUR_DEVICES));
      tile_->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_TOOLTIP_ENABLED,
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_YOUR_DEVICES)));
      break;
    case ::nearby_share::mojom::Visibility::kAllContacts:
      tile_->SetSubLabel(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_CONTACTS));
      tile_->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_TOOLTIP_ENABLED,
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_CONTACTS)));
      break;
    case ::nearby_share::mojom::Visibility::kNoOne:
      tile_->SetSubLabel(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_HIDDEN));
      tile_->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_TOOLTIP_ENABLED,
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_HIDDEN)));
      break;
    default:
      break;
  }
}

void NearbyShareFeaturePodController::ToggleTileOff() {
  CHECK(nearby_share_delegate_);
  CHECK(tile_);
  auto& off_icon = nearby_share_delegate_->GetIcon(/*on_icon=*/false);
  tile_->SetVectorIcon(off_icon.is_empty() ? kQuickSettingsNearbyShareOffIcon
                                           : off_icon);
  tile_->SetToggled(false);

  tile_->SetIconButtonTooltipText(IconTooltipString(/*is_enabled=*/false));
  tile_->SetSubLabel(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL_OFF));
  tile_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_TOOLTIP_DISABLED));
}

base::TimeDelta NearbyShareFeaturePodController::RemainingHighVisibilityTime()
    const {
  base::TimeTicks now = base::TimeTicks::Now();
  if (chromeos::features::IsQuickShareV2Enabled()) {
    const base::TimeTicks shutoff_time =
        nearby_share_delegate_->HighVisibilityShutoffTime();
    return shutoff_time > now ? shutoff_time - now : base::Seconds(0);
  }

  return shutoff_time_ > now ? shutoff_time_ - now : base::Seconds(0);
}

}  // namespace ash
