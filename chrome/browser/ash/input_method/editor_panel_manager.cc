// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_text_selection_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {

chromeos::editor_menu::PresetQueryCategory ToPresetQueryCategory(
    const orca::mojom::PresetTextQueryType query_type) {
  switch (query_type) {
    case orca::mojom::PresetTextQueryType::kUnknown:
      return chromeos::editor_menu::PresetQueryCategory::kUnknown;
    case orca::mojom::PresetTextQueryType::kShorten:
      return chromeos::editor_menu::PresetQueryCategory::kShorten;
    case orca::mojom::PresetTextQueryType::kElaborate:
      return chromeos::editor_menu::PresetQueryCategory::kElaborate;
    case orca::mojom::PresetTextQueryType::kRephrase:
      return chromeos::editor_menu::PresetQueryCategory::kRephrase;
    case orca::mojom::PresetTextQueryType::kFormalize:
      return chromeos::editor_menu::PresetQueryCategory::kFormalize;
    case orca::mojom::PresetTextQueryType::kEmojify:
      return chromeos::editor_menu::PresetQueryCategory::kEmojify;
    case orca::mojom::PresetTextQueryType::kProofread:
      return chromeos::editor_menu::PresetQueryCategory::kProofread;
  }
}

EditorPanelManagerImpl::EditorPanelManagerImpl(Delegate* delegate)
    : delegate_(delegate) {}

EditorPanelManagerImpl::~EditorPanelManagerImpl() = default;

void EditorPanelManagerImpl::BindEditorClient() {
  if (editor_client_remote_.is_bound() &&
      !base::FeatureList::IsEnabled(ash::features::kOrcaServiceConnection)) {
    return;
  }

  editor_client_remote_.reset();
  delegate_->BindEditorClient(
      editor_client_remote_.BindNewPipeAndPassReceiver());
  editor_client_remote_.reset_on_disconnect();
}

void EditorPanelManagerImpl::GetEditorPanelContext(
    GetEditorPanelContextCallback callback) {
  chromeos::editor_menu::EditorMode editor_panel_mode =
      delegate_->GetEditorMode();

  if (editor_panel_mode != chromeos::editor_menu::EditorMode::kSoftBlocked &&
      editor_panel_mode != chromeos::editor_menu::EditorMode::kHardBlocked &&
      editor_client_remote_.is_bound()) {
    editor_client_remote_->GetPresetTextQueries(
        base::BindOnce(&EditorPanelManagerImpl::OnGetPresetTextQueriesResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       editor_panel_mode));
    return;
  }

  std::move(callback).Run(chromeos::editor_menu::EditorContext(
      /*mode=*/editor_panel_mode,
      /*text_selection_mode=*/delegate_->GetEditorTextSelectionMode(),
      /*consent_status_settled=*/delegate_->GetConsentStatus() !=
          chromeos::editor_menu::EditorConsentStatus::kUnset,
      chromeos::editor_menu::PresetTextQueries()));
}

void EditorPanelManagerImpl::OnPromoCardDismissed() {}

void EditorPanelManagerImpl::OnPromoCardDeclined() {
  delegate_->OnPromoCardDeclined();
  delegate_->GetMetricsRecorder()->LogEditorState(
      EditorStates::kPromoCardExplicitDismissal);
}

void EditorPanelManagerImpl::OnConsentRejected() {
  delegate_->ProcessConsentAction(ConsentAction::kDecline);
}

void EditorPanelManagerImpl::StartEditingFlow() {
  delegate_->HandleTrigger(/*preset_query_id=*/std::nullopt,
                           /*freeform_text=*/std::nullopt);
}

void EditorPanelManagerImpl::StartEditingFlowWithPreset(
    const std::string& text_query_id) {
  delegate_->HandleTrigger(/*preset_query_id=*/text_query_id,
                           /*freeform_text=*/std::nullopt);
}

void EditorPanelManagerImpl::StartEditingFlowWithFreeform(
    const std::string& text) {
  delegate_->HandleTrigger(/*preset_query_id=*/std::nullopt,
                           /*freeform_text=*/text);
}

void EditorPanelManagerImpl::OnGetPresetTextQueriesResult(
    GetEditorPanelContextCallback callback,
    chromeos::editor_menu::EditorMode mode,
    std::vector<orca::mojom::PresetTextQueryPtr> queries) {
  chromeos::editor_menu::PresetTextQueries text_queries;

  for (const auto& query : queries) {
    text_queries.push_back(chromeos::editor_menu::PresetTextQuery(
        query->id, base::UTF8ToUTF16(query->label),
        ToPresetQueryCategory(query->type)));
  }

  std::move(callback).Run(chromeos::editor_menu::EditorContext(
      mode, /*text_selection_mode=*/delegate_->GetEditorTextSelectionMode(),
      /*consent_status_settled=*/delegate_->GetConsentStatus() !=
          chromeos::editor_menu::EditorConsentStatus::kUnset,

      text_queries));
}

void EditorPanelManagerImpl::OnEditorMenuVisibilityChanged(bool visible) {
  is_editor_menu_visible_ = visible;
}

bool EditorPanelManagerImpl::IsEditorMenuVisible() const {
  return is_editor_menu_visible_;
}

void EditorPanelManagerImpl::LogEditorMode(
    chromeos::editor_menu::EditorMode mode) {
  EditorOpportunityMode opportunity_mode =
      delegate_->GetEditorOpportunityMode();
  EditorMetricsRecorder* logger = delegate_->GetMetricsRecorder();
  logger->SetMode(opportunity_mode);
  logger->SetTone(EditorTone::kUnset);
  if (opportunity_mode == EditorOpportunityMode::kRewrite ||
      opportunity_mode == EditorOpportunityMode::kWrite) {
    logger->LogEditorState(EditorStates::kNativeUIShowOpportunity);
  }

  if (mode == chromeos::editor_menu::EditorMode::kRewrite ||
      mode == chromeos::editor_menu::EditorMode::kWrite) {
    logger->LogEditorState(EditorStates::kNativeUIShown);
  }

  if (mode == chromeos::editor_menu::EditorMode::kHardBlocked ||
      mode == chromeos::editor_menu::EditorMode::kSoftBlocked) {
    logger->LogEditorState(EditorStates::kBlocked);
    for (EditorBlockedReason blocked_reason : delegate_->GetBlockedReasons()) {
      logger->LogEditorState(ToEditorStatesMetric(blocked_reason));
    }
  }

  if (mode == chromeos::editor_menu::EditorMode::kConsentNeeded) {
    logger->LogEditorState(EditorStates::kPromoCardImpression);
  }
}

void EditorPanelManagerImpl::AddObserver(
    EditorPanelManagerImpl::Observer* observer) {
  observers_.AddObserver(observer);
}

void EditorPanelManagerImpl::RemoveObserver(
    EditorPanelManagerImpl::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EditorPanelManagerImpl::NotifyEditorModeChanged(chromeos::editor_menu::EditorMode mode) {
  for (EditorPanelManagerImpl::Observer& obs : observers_) {
    obs.OnEditorModeChanged(mode);
  }
}

void EditorPanelManagerImpl::RequestCacheContext() {
  delegate_->CacheContext();
}

void EditorPanelManagerImpl::OnConsentApproved() {
  delegate_->ProcessConsentAction(ConsentAction::kApprove);
}

void EditorPanelManagerImpl::OnMagicBoostPromoCardDeclined() {
  // Reject consent and follow the normal workflow similar to when Orca's promo
  // card is declined.
  OnConsentRejected();
  OnPromoCardDeclined();
}

bool EditorPanelManagerImpl::ShouldOptInEditor() {
  chromeos::editor_menu::EditorMode editor_panel_mode =
      delegate_->GetEditorMode();
  chromeos::editor_menu::EditorConsentStatus consent_status =
      delegate_->GetConsentStatus();

  return editor_panel_mode != chromeos::editor_menu::EditorMode::kHardBlocked &&
         consent_status == chromeos::editor_menu::EditorConsentStatus::kUnset;
}

void EditorPanelManagerImpl::SetEditorClientForTesting(
    mojo::PendingRemote<orca::mojom::EditorClient> client_for_testing) {
  editor_client_remote_.Bind(std::move(client_for_testing));
}

}  // namespace ash::input_method
