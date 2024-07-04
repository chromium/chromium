// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"

namespace ash {

using TransitionAction = crosapi::mojom::MagicBoostController::TransitionAction;

MagicBoostControllerAsh::MagicBoostControllerAsh() = default;

MagicBoostControllerAsh::~MagicBoostControllerAsh() = default;

void MagicBoostControllerAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::MagicBoostController> receiver) {
  // The receiver is only from lacros chrome as present, but more mojo clients
  // may be added in the future.
  receivers_.Add(this, std::move(receiver));
}

void MagicBoostControllerAsh::ShowDisclaimerUi(int64_t display_id,
                                               TransitionAction action,
                                               OptInFeatures opt_in_features) {
  opt_in_features_ = opt_in_features;

  if (disclaimer_widget_) {
    return;
  }

  disclaimer_widget_ = MagicBoostDisclaimerView::CreateWidget(
      display_id,
      /*press_accept_button_callback=*/
      base::BindRepeating(
          &MagicBoostControllerAsh::OnDisclaimerAcceptButtonPressed,
          weak_ptr_factory_.GetWeakPtr(), action),
      /*press_decline_button_callback=*/
      base::BindRepeating(
          &MagicBoostControllerAsh::OnDisclaimerDeclineButtonPressed,
          weak_ptr_factory_.GetWeakPtr()));
  disclaimer_widget_->Show();
}

void MagicBoostControllerAsh::CloseDisclaimerUi() {
  disclaimer_widget_.reset();
}

void MagicBoostControllerAsh::OnDisclaimerAcceptButtonPressed(
    TransitionAction action) {
  auto* magic_boost_state =
      static_cast<MagicBoostStateAsh*>(chromeos::MagicBoostState::Get());
  if (opt_in_features_ == OptInFeatures::kOrcaAndHmr) {
    magic_boost_state->EnableOrcaFeature();
  }
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);
  magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/true);

  switch (action) {
    case TransitionAction::kDoNothing:
      break;
    case TransitionAction::kShowEditorPanel:
      magic_boost_state->GetEditorPanelManager()->StartEditingFlow();
      break;
  }

  CloseDisclaimerUi();
}

void MagicBoostControllerAsh::OnDisclaimerDeclineButtonPressed() {
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  if (opt_in_features_ == OptInFeatures::kOrcaAndHmr) {
    magic_boost_state->DisableOrcaFeature();
  }
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kDeclined);
  magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/false);

  CloseDisclaimerUi();
}

}  // namespace ash
