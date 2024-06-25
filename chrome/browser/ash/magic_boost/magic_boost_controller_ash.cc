// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "base/functional/bind.h"
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
                                               TransitionAction action) {
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
  chromeos::MagicBoostState::Get()->ShouldIncludeOrcaInOptIn(base::BindOnce(
      [](TransitionAction action, bool should_include_orca) {
        auto* magic_boost_state =
            static_cast<MagicBoostStateAsh*>(chromeos::MagicBoostState::Get());
        if (should_include_orca) {
          magic_boost_state->EnableOrcaFeature();
        }
        magic_boost_state->AsyncWriteConsentStatus(
            chromeos::HMRConsentStatus::kApproved);
        magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/true);

        switch (action) {
          case TransitionAction::kDoNothing:
            break;
          case TransitionAction::kShowEditorPanel:
            // TODO(b/349152608): Show Editor Panel when opt-in flow is
            // completed.
            break;
        }
      },
      action));

  CloseDisclaimerUi();
}

void MagicBoostControllerAsh::OnDisclaimerDeclineButtonPressed() {
  chromeos::MagicBoostState::Get()->ShouldIncludeOrcaInOptIn(
      base::BindOnce([](bool should_include_orca) {
        auto* magic_boost_state = chromeos::MagicBoostState::Get();
        if (should_include_orca) {
          magic_boost_state->DisableOrcaFeature();
        }
        magic_boost_state->AsyncWriteConsentStatus(
            chromeos::HMRConsentStatus::kDeclined);
        magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/false);
      }));

  CloseDisclaimerUi();
}

}  // namespace ash
