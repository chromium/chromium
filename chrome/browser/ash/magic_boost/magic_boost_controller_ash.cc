// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"

namespace ash {

namespace {

MagicBoostControllerAsh* g_instance = nullptr;

// The disclaimer terms of service and learn more urls.
constexpr char kDisclaimerTOSURL[] = "https://policies.google.com/terms";
constexpr char kLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=settings_help_me_read_write";

}  // namespace

using TransitionAction = crosapi::mojom::MagicBoostController::TransitionAction;

// static
MagicBoostControllerAsh* MagicBoostControllerAsh::Get() {
  return g_instance;
}

MagicBoostControllerAsh::MagicBoostControllerAsh() {
  DCHECK(!g_instance);
  g_instance = this;
}

MagicBoostControllerAsh::~MagicBoostControllerAsh() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

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

  // Destroy the existing `disclaimer_widget_`, if any. We always create a new
  // widget to ensure the correct disclaimer view position.
  if (disclaimer_widget_) {
    CloseDisclaimerUi();
  }

  disclaimer_widget_ = MagicBoostDisclaimerView::CreateWidget(
      display_id,
      /*press_accept_button_callback=*/
      base::BindRepeating(
          &MagicBoostControllerAsh::OnDisclaimerAcceptButtonPressed,
          weak_ptr_factory_.GetWeakPtr(), display_id, action),
      /*press_decline_button_callback=*/
      base::BindRepeating(
          &MagicBoostControllerAsh::OnDisclaimerDeclineButtonPressed,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MagicBoostControllerAsh::OnLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(), kDisclaimerTOSURL),
      base::BindRepeating(&MagicBoostControllerAsh::OnLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(), kLearnMoreURL));
  disclaimer_widget_->Show();

  RecordDisclaimerViewActionMetrics(opt_in_features_,
                                    DisclaimerViewAction::kShow);
}

void MagicBoostControllerAsh::CloseDisclaimerUi() {
  disclaimer_widget_.reset();
}

void MagicBoostControllerAsh::OnDisclaimerAcceptButtonPressed(
    int64_t display_id,
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
    case TransitionAction::kShowHmrPanel:
      chromeos::MahiManager::Get()->OpenMahiPanel(
          display_id, disclaimer_widget_->GetWindowBoundsInScreen());
      break;
  }

  RecordDisclaimerViewActionMetrics(opt_in_features_,
                                    DisclaimerViewAction::kAcceptButtonPressed);

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

  RecordDisclaimerViewActionMetrics(
      opt_in_features_, DisclaimerViewAction::kDeclineButtonPressed);

  CloseDisclaimerUi();
}

void MagicBoostControllerAsh::OnLinkPressed(const std::string& url) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);

  CloseDisclaimerUi();
}

}  // namespace ash
