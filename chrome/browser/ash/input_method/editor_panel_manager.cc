// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace ash::input_method {

namespace {

crosapi::mojom::EditorPanelPresetTextQueryPtr ToEditorPanelQuery(
    const orca::mojom::PresetTextQueryPtr& orca_query) {
  auto editor_panel_query = crosapi::mojom::EditorPanelPresetTextQuery::New();
  editor_panel_query->text_query_id = orca_query->id;
  editor_panel_query->name = orca_query->label;
  editor_panel_query->description = orca_query->description;
  // TODO(b/295059934): Add the actual preset query categories.
  editor_panel_query->category =
      crosapi::mojom::EditorPanelPresetQueryCategory::kUnknown;
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

void EditorPanelManager::GetEditorPanelContext(
    GetEditorPanelContextCallback callback) {
  // TODO(b/295059934): Get the panel mode from the editor mediator.
  const auto editor_panel_mode = crosapi::mojom::EditorPanelMode::kPromoCard;

  // TODO(b/295059934): Bind the editor client before getting the preset text
  // queries.
  if (editor_panel_mode == crosapi::mojom::EditorPanelMode::kRewrite &&
      editor_client_remote_.is_bound()) {
    editor_client_remote_->GetPresetTextQueries(
        base::BindOnce(&EditorPanelManager::OnGetPresetTextQueriesResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    auto context = crosapi::mojom::EditorPanelContext::New();
    context->editor_panel_mode = editor_panel_mode;
    std::move(callback).Run(std::move(context));
  }
}

void EditorPanelManager::OnPromoCardDismissed() {
}

void EditorPanelManager::OnPromoCardDeclined() {
  delegate_->OnPromoCardDeclined();
}

void EditorPanelManager::StartEditingFlow() {
  delegate_->HandleTrigger();
}

void EditorPanelManager::StartEditingFlowWithPreset(
    const std::string& text_query_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::StartEditingFlowWithFreeform(const std::string& text) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::OnGetPresetTextQueriesResult(
    GetEditorPanelContextCallback callback,
    std::vector<orca::mojom::PresetTextQueryPtr> queries) {
  auto context = crosapi::mojom::EditorPanelContext::New();
  context->editor_panel_mode = crosapi::mojom::EditorPanelMode::kRewrite;
  for (const auto& query : queries) {
    context->preset_text_queries.push_back(ToEditorPanelQuery(query));
  }
  std::move(callback).Run(std::move(context));
}

}  // namespace ash::input_method
