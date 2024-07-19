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
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace ash::input_method {

namespace {

crosapi::mojom::EditorPanelPresetQueryCategory ToEditorPanelQueryCategory(
    const orca::mojom::PresetTextQueryType query_type) {
  switch (query_type) {
    case orca::mojom::PresetTextQueryType::kUnknown:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kUnknown;
    case orca::mojom::PresetTextQueryType::kShorten:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kShorten;
    case orca::mojom::PresetTextQueryType::kElaborate:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kElaborate;
    case orca::mojom::PresetTextQueryType::kRephrase:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kRephrase;
    case orca::mojom::PresetTextQueryType::kFormalize:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kFormalize;
    case orca::mojom::PresetTextQueryType::kEmojify:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kEmojify;
    case orca::mojom::PresetTextQueryType::kProofread:
      return crosapi::mojom::EditorPanelPresetQueryCategory::kProofread;
  }
}

crosapi::mojom::EditorPanelMode GetEditorPanelMode(EditorMode editor_mode) {
  switch (editor_mode) {
    case EditorMode::kHardBlocked:
      return crosapi::mojom::EditorPanelMode::kHardBlocked;
    case EditorMode::kSoftBlocked:
      return crosapi::mojom::EditorPanelMode::kSoftBlocked;
    case EditorMode::kConsentNeeded:
      return crosapi::mojom::EditorPanelMode::kPromoCard;
    case EditorMode::kRewrite:
      return crosapi::mojom::EditorPanelMode::kRewrite;
    case EditorMode::kWrite:
      return crosapi::mojom::EditorPanelMode::kWrite;
  }
}

crosapi::mojom::EditorPanelPresetTextQueryPtr ToEditorPanelQuery(
    const orca::mojom::PresetTextQueryPtr& orca_query) {
  auto editor_panel_query = crosapi::mojom::EditorPanelPresetTextQuery::New();
  editor_panel_query->text_query_id = orca_query->id;
  editor_panel_query->name = orca_query->label;
  editor_panel_query->description = orca_query->description;
  editor_panel_query->category = ToEditorPanelQueryCategory(orca_query->type);
  return editor_panel_query;
}

}  // namespace

EditorPanelManager::EditorPanelManager(Delegate* delegate)
    : delegate_(delegate) {}

EditorPanelManager::~EditorPanelManager() = default;

void EditorPanelManager::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void EditorPanelManager::BindEditorClient() {
  if (editor_client_remote_.is_bound() &&
      !base::FeatureList::IsEnabled(ash::features::kOrcaServiceConnection)) {
    return;
  }

  editor_client_remote_.reset();
  delegate_->BindEditorClient(
      editor_client_remote_.BindNewPipeAndPassReceiver());
  editor_client_remote_.reset_on_disconnect();
}

void EditorPanelManager::GetEditorPanelContext(
    GetEditorPanelContextCallback callback) {
  const auto editor_panel_mode = GetEditorPanelMode(delegate_->GetEditorMode());

  if (editor_panel_mode != crosapi::mojom::EditorPanelMode::kSoftBlocked &&
      editor_panel_mode != crosapi::mojom::EditorPanelMode::kHardBlocked &&
      editor_client_remote_.is_bound()) {
    editor_client_remote_->GetPresetTextQueries(
        base::BindOnce(&EditorPanelManager::OnGetPresetTextQueriesResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       editor_panel_mode));
    return;
  }

  auto context = crosapi::mojom::EditorPanelContext::New();
  context->editor_panel_mode = editor_panel_mode;
  context->consent_status_settled =
      delegate_->GetConsentStatus() != ConsentStatus::kUnset;
  std::move(callback).Run(std::move(context));
}

void EditorPanelManager::OnPromoCardDismissed() {}

void EditorPanelManager::OnPromoCardDeclined() {
  delegate_->OnPromoCardDeclined();
  delegate_->GetMetricsRecorder()->LogEditorState(
      EditorStates::kPromoCardExplicitDismissal);
}

void EditorPanelManager::OnConsentRejected() {
  delegate_->ProcessConsentAction(ConsentAction::kDecline);
}

void EditorPanelManager::StartEditingFlow() {
  delegate_->HandleTrigger(/*preset_query_id=*/std::nullopt,
                           /*freeform_text=*/std::nullopt);
}

void EditorPanelManager::StartEditingFlowWithPreset(
    const std::string& text_query_id) {
  delegate_->HandleTrigger(/*preset_query_id=*/text_query_id,
                           /*freeform_text=*/std::nullopt);
}

void EditorPanelManager::StartEditingFlowWithFreeform(const std::string& text) {
  delegate_->HandleTrigger(/*preset_query_id=*/std::nullopt,
                           /*freeform_text=*/text);
}

void EditorPanelManager::OnGetPresetTextQueriesResult(
    GetEditorPanelContextCallback callback,
    crosapi::mojom::EditorPanelMode panel_mode,
    std::vector<orca::mojom::PresetTextQueryPtr> queries) {
  auto context = crosapi::mojom::EditorPanelContext::New();
  context->editor_panel_mode = panel_mode;
  context->consent_status_settled =
      delegate_->GetConsentStatus() != ConsentStatus::kUnset;
  for (const auto& query : queries) {
    context->preset_text_queries.push_back(ToEditorPanelQuery(query));
  }
  std::move(callback).Run(std::move(context));
}

void EditorPanelManager::OnEditorMenuVisibilityChanged(bool visible) {
  is_editor_menu_visible_ = visible;
}

bool EditorPanelManager::IsEditorMenuVisible() const {
  return is_editor_menu_visible_;
}

void EditorPanelManager::LogEditorMode(crosapi::mojom::EditorPanelMode mode) {
  EditorOpportunityMode opportunity_mode =
      delegate_->GetEditorOpportunityMode();
  EditorMetricsRecorder* logger = delegate_->GetMetricsRecorder();
  logger->SetMode(opportunity_mode);
  logger->SetTone(EditorTone::kUnset);
  if (opportunity_mode == EditorOpportunityMode::kRewrite ||
      opportunity_mode == EditorOpportunityMode::kWrite) {
    logger->LogEditorState(EditorStates::kNativeUIShowOpportunity);
  }

  if (mode == crosapi::mojom::EditorPanelMode::kRewrite ||
      mode == crosapi::mojom::EditorPanelMode::kWrite) {
    logger->LogEditorState(EditorStates::kNativeUIShown);
  }

  if (mode == crosapi::mojom::EditorPanelMode::kHardBlocked ||
      mode == crosapi::mojom::EditorPanelMode::kSoftBlocked) {
    logger->LogEditorState(EditorStates::kBlocked);
    for (EditorBlockedReason blocked_reason : delegate_->GetBlockedReasons()) {
      logger->LogEditorState(ToEditorStatesMetric(blocked_reason));
    }
  }

  if (mode == crosapi::mojom::EditorPanelMode::kPromoCard) {
    logger->LogEditorState(EditorStates::kPromoCardImpression);
  }
}

void EditorPanelManager::BindEditorObserver(
    mojo::PendingRemote<crosapi::mojom::EditorObserver>
        pending_observer_remote) {
  observer_remotes_.Add(std::move(pending_observer_remote));
}

void EditorPanelManager::AddObserver(EditorPanelManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void EditorPanelManager::RemoveObserver(
    EditorPanelManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EditorPanelManager::NotifyEditorModeChanged(const EditorMode& mode) {
  for (const mojo::Remote<crosapi::mojom::EditorObserver>& obs :
       observer_remotes_) {
    obs->OnEditorPanelModeChanged(GetEditorPanelMode(mode));
  }
  for (EditorPanelManager::Observer& obs : observers_) {
    obs.OnEditorModeChanged(mode);
  }
}

void EditorPanelManager::RequestCacheContext() {
  delegate_->CacheContext();
}

void EditorPanelManager::OnConsentApproved() {
  delegate_->ProcessConsentAction(ConsentAction::kApprove);
}

void EditorPanelManager::OnMagicBoostPromoCardDeclined() {
  // Reject consent and follow the normal workflow similar to when Orca's promo
  // card is declined.
  OnConsentRejected();
  OnPromoCardDeclined();
}

void EditorPanelManager::SetEditorClientForTesting(
    mojo::PendingRemote<orca::mojom::EditorClient> client_for_testing) {
  editor_client_remote_.Bind(std::move(client_for_testing));
}

}  // namespace ash::input_method
