// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"

#include "ash/lobster/lobster_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/shell.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash {

namespace {

MagicBoostController* g_instance = nullptr;

// The disclaimer terms of service and learn more urls.
constexpr char kDisclaimerTOSURL[] = "https://policies.google.com/terms";
constexpr char kLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=settings_help_me_read_write";

}  // namespace

// static
MagicBoostController* MagicBoostController::Get() {
  return g_instance;
}

MagicBoostController::MagicBoostController() {
  DCHECK(!g_instance);
  g_instance = this;
}

MagicBoostController::~MagicBoostController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

MagicBoostControllerImpl::MagicBoostControllerImpl() = default;

MagicBoostControllerImpl::~MagicBoostControllerImpl() = default;

void MagicBoostControllerImpl::ShowDisclaimerUi(
    int64_t display_id,
    magic_boost::TransitionAction action,
    magic_boost::OptInFeatures opt_in_features) {
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
          &MagicBoostControllerImpl::OnDisclaimerAcceptButtonPressed,
          weak_ptr_factory_.GetWeakPtr(), display_id, action),
      /*press_decline_button_callback=*/
      base::BindRepeating(
          &MagicBoostControllerImpl::OnDisclaimerDeclineButtonPressed,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MagicBoostControllerImpl::OnLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(), kDisclaimerTOSURL),
      base::BindRepeating(&MagicBoostControllerImpl::OnLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(), kLearnMoreURL));
  disclaimer_widget_->Show();

  RecordDisclaimerViewActionMetrics(opt_in_features_,
                                    DisclaimerViewAction::kShow);
}

void MagicBoostControllerImpl::CloseDisclaimerUi() {
  disclaimer_widget_.reset();
}

void MagicBoostControllerImpl::OnDisclaimerAcceptButtonPressed(
    int64_t display_id,
    magic_boost::TransitionAction action) {
  auto* magic_boost_state =
      static_cast<MagicBoostState*>(chromeos::MagicBoostState::Get());
  if (opt_in_features_ == magic_boost::OptInFeatures::kOrcaAndHmr) {
    magic_boost_state->EnableOrcaFeature();
  }
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);

  switch (action) {
    case magic_boost::TransitionAction::kDoNothing:
      break;
    case magic_boost::TransitionAction::kShowEditorPanel:
      magic_boost_state->GetEditorPanelManager()->StartEditingFlow();
      break;
    case magic_boost::TransitionAction::kShowHmrPanel:
      chromeos::MahiManager::Get()->OpenMahiPanel(
          display_id, disclaimer_widget_->GetWindowBoundsInScreen());
      break;
    case magic_boost::TransitionAction::kShowLobsterPanel:
      LobsterController* lobster_controller =
          ash::Shell::Get()->lobster_controller();
      if (lobster_controller != nullptr) {
        lobster_controller->LoadUIFromCachedContext();
      }
      break;
  }

  RecordDisclaimerViewActionMetrics(opt_in_features_,
                                    DisclaimerViewAction::kAcceptButtonPressed);

  CloseDisclaimerUi();
}

void MagicBoostControllerImpl::OnDisclaimerDeclineButtonPressed() {
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  if (opt_in_features_ == magic_boost::OptInFeatures::kOrcaAndHmr) {
    magic_boost_state->DisableOrcaFeature();
    magic_boost_state->DisableLobsterSettings();
  }
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kDeclined);
  magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/false);

  RecordDisclaimerViewActionMetrics(
      opt_in_features_, DisclaimerViewAction::kDeclineButtonPressed);

  CloseDisclaimerUi();
}

void MagicBoostControllerImpl::OnLinkPressed(const std::string& url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);

  CloseDisclaimerUi();
}

}  // namespace ash
