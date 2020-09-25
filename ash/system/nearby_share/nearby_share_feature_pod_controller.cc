// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"

#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr base::TimeDelta kOneMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);

base::string16 RemainingTimeString(base::TimeDelta remaining_time) {
  if (remaining_time > kOneMinute) {
    return base::ASCIIToUTF16(
        base::NumberToString(remaining_time.InMinutes() + 1) + " min");
  }

  return base::ASCIIToUTF16(
      base::NumberToString(remaining_time.InSeconds() + 1) + " sec");
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

FeaturePodButton* NearbyShareFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  button_->SetVisible(nearby_share_delegate_->IsPodButtonVisible() &&
                      session_controller->IsActiveUserSessionStarted() &&
                      session_controller->IsUserPrimary() &&
                      !session_controller->IsScreenLocked());
  button_->SetLabel(base::ASCIIToUTF16("Nearby Share"));
  button_->SetLabelTooltip(base::ASCIIToUTF16("Show Nearby Share Settings."));
  bool enabled = nearby_share_delegate_->IsHighVisibilityOn();
  OnHighVisibilityEnabledChanged(enabled);
  return button_;
}

void NearbyShareFeaturePodController::OnIconPressed() {
  if (nearby_share_delegate_->IsHighVisibilityOn()) {
    nearby_share_delegate_->DisableHighVisibility();
  } else {
    nearby_share_delegate_->EnableHighVisibility();
  }
}

void NearbyShareFeaturePodController::OnLabelPressed() {
  nearby_share_delegate_->ShowNearbyShareSettings();
}

SystemTrayItemUmaType NearbyShareFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NEARBY_SHARE;
}

void NearbyShareFeaturePodController::OnHighVisibilityEnabledChanged(
    bool enabled) {
  if (enabled) {
    shutoff_time_ = nearby_share_delegate_->HighVisibilityShutoffTime();
    countdown_timer_.Reset();
  } else {
    countdown_timer_.Stop();
  }
  UpdateButton(enabled);
}

void NearbyShareFeaturePodController::UpdateButton(bool enabled) {
  // TODO(crrev/c/2401842): l10n strings

  button_->SetToggled(enabled);
  button_->SetVectorIcon(enabled ? kUnifiedMenuNearbyShareVisibleIcon
                                 : kUnifiedMenuNearbyShareNotVisibleIcon);

  if (enabled) {
    button_->SetSubLabel(base::ASCIIToUTF16("On, ") +
                         RemainingTimeString(RemainingHighVisibilityTime()));
  } else {
    button_->SetSubLabel(base::ASCIIToUTF16("Off"));
  }

  base::string16 tooltip_state =
      enabled ? base::ASCIIToUTF16("High visibility on.")
              : base::ASCIIToUTF16("High visibility off");
  button_->SetIconTooltip(
      base::ASCIIToUTF16("Toggle Nearby Share high visibility.") +
      tooltip_state);
}

base::TimeDelta NearbyShareFeaturePodController::RemainingHighVisibilityTime()
    const {
  base::TimeTicks now = base::TimeTicks::Now();
  return shutoff_time_ > now ? shutoff_time_ - now
                             : base::TimeDelta::FromSeconds(0);
}

}  // namespace ash
