// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/lobster/lobster_metrics_recorder.h"
#include "ash/lobster/lobster_session_impl.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_client_factory.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/shell.h"

namespace ash {

namespace {

bool HasOpportunityToShow(
    const ash::LobsterSystemState::SystemChecks& system_checks) {
  return (!system_checks.Has(ash::LobsterSystemCheck::kInvalidFeatureFlags) &&
      !system_checks.Has(ash::LobsterSystemCheck::kUnsupportedHardware) &&
      !system_checks.Has(ash::LobsterSystemCheck::kUnsupportedInKioskMode) &&
      !system_checks.Has(ash::LobsterSystemCheck::kInvalidInputField));
}

void RecordEntryPointImpressionMetric(LobsterEntryPoint entry_point) {
  switch (entry_point) {
    case LobsterEntryPoint::kQuickInsert:
      RecordLobsterState(LobsterMetricState::kQuickInsertTriggerImpression);
      return;
    case LobsterEntryPoint::kRightClickMenu:
      RecordLobsterState(LobsterMetricState::kRightClickTriggerImpression);
      return;
  }
}

void RecordEntryPointConsentRequiredMetric(LobsterEntryPoint entry_point) {
  switch (entry_point) {
    case LobsterEntryPoint::kQuickInsert:
      RecordLobsterState(LobsterMetricState::kQuickInsertTriggerNeedsConsent);
      return;
    case LobsterEntryPoint::kRightClickMenu:
      RecordLobsterState(LobsterMetricState::kRightClickTriggerNeedsConsent);
      return;
  }
}

}  // namespace

LobsterController::Trigger::Trigger(
    std::unique_ptr<LobsterClient> client,
    LobsterEntryPoint entry_point,
    LobsterMode mode,
    const LobsterTextInputContext& text_input_context)
    : client_(std::move(client)),
      state_(State::kReady),
      entry_point_(entry_point),
      mode_(mode),
      text_input_context_(text_input_context) {}

LobsterController::Trigger::~Trigger() = default;

void LobsterController::Trigger::Fire(std::optional<std::string> query) {
  if (state_ == State::kDisabled) {
    return;
  }

  state_ = State::kDisabled;

  if (ash::Shell::Get() == nullptr) {
    return;
  }

  LobsterController* controller = ash::Shell::Get()->lobster_controller();

  if (controller == nullptr) {
    return;
  }

  controller->StartSession(std::move(client_), std::move(query), entry_point_,
                           mode_, text_input_context_);
}

LobsterController::LobsterController() = default;

LobsterController::~LobsterController() = default;

void LobsterController::SetClientFactory(LobsterClientFactory* client_factory) {
  client_factory_ = client_factory;
}

std::unique_ptr<LobsterController::Trigger> LobsterController::CreateTrigger(
    LobsterEntryPoint entry_point,
    ui::TextInputClient* text_input_client) {
  if (client_factory_ == nullptr) {
    return nullptr;
  }
  std::unique_ptr<LobsterClient> client = client_factory_->CreateClient();
  if (client == nullptr) {
    return nullptr;
  }

  // Lobster is only triggered from focused text fields. If no text input client
  // is found, never create a trigger.
  if (text_input_client == nullptr) {
    return nullptr;
  }

  LobsterTextInputContext text_input_context(
      /*text_input_type=*/text_input_client->GetTextInputType(),
      /*caret_bounds=*/text_input_client->GetCaretBounds(),
      /*support_image_insertion=*/text_input_client->CanInsertImage());

  LobsterSystemState system_state = client->GetSystemState(text_input_context);

  // Records the Opportunity metric. This metric is recorded in the following
  // cases:
  // * Lobster is shown.
  // * Lobster is being blocked, but is likely to be shown for the current
  // context in the future.
  if (HasOpportunityToShow(system_state.failed_checks)) {
    RecordLobsterState(LobsterMetricState::kShownOpportunity);
  }

  if (system_state.status == LobsterStatus::kBlocked) {
    RecordLobsterState(LobsterMetricState::kBlocked);
    for (auto failed_reason : system_state.failed_checks) {
      RecordLobsterBlockedReason(failed_reason);
    }
    return nullptr;
  }

  if (system_state.status == LobsterStatus::kConsentNeeded) {
    RecordEntryPointConsentRequiredMetric(entry_point);
  }

  // Creates the trigger point if the status is either enabled, or requires
  // consent.
  RecordEntryPointImpressionMetric(entry_point);
  return std::make_unique<Trigger>(std::move(client), entry_point,
                                   text_input_context.support_image_insertion
                                       ? LobsterMode::kInsert
                                       : LobsterMode::kDownload,
                                   std::move(text_input_context));
}

void LobsterController::LoadUIFromCachedContext() {
  if (active_session_ == nullptr) {
    return;
  }
  active_session_->LoadUIFromCachedContext();
}

void LobsterController::StartSession(
    std::unique_ptr<LobsterClient> client,
    std::optional<std::string> query,
    LobsterEntryPoint entry_point,
    LobsterMode mode,
    const LobsterTextInputContext& text_input_context) {
  // Before creating a new session, we need to inform the lobster client and
  // lobster session to clear their pointer to the session that is about to be
  // destroyed. This is to prevent them from holding a dangling pointer to the
  // old session when a new session is created.
  // TODO: crbug.com/371484317 - refactors the following logic to handle the
  // session creation better.
  client->SetActiveSession(nullptr);
  LobsterClient* lobster_client_ptr = client.get();
  active_session_ = std::make_unique<LobsterSessionImpl>(std::move(client),
                                                         entry_point, mode);
  lobster_client_ptr->SetActiveSession(active_session_.get());

  LobsterStatus lobster_status =
      lobster_client_ptr->GetSystemState(text_input_context).status;
  // When LobsterForceShowDisclaimer flag is enabled, we will show the Lobster
  // Disclaimer screen even when Lobster consent status has been approved
  // before.
  if (lobster_status == LobsterStatus::kEnabled &&
      ash::features::IsLobsterAlwaysShowDisclaimerForTesting()) {
    lobster_status = LobsterStatus::kConsentNeeded;
  }

  switch (lobster_status) {
    case LobsterStatus::kConsentNeeded:
      active_session_->ShowDisclaimerUIAndCacheContext(
          query, text_input_context.caret_bounds);
      return;
    case LobsterStatus::kEnabled:
      active_session_->LoadUI(query, mode, text_input_context.caret_bounds);
      return;
    case LobsterStatus::kBlocked:
      return;
  }
}

}  // namespace ash
