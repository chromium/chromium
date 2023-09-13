// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace ash::input_method {

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
  auto context = crosapi::mojom::EditorPanelContext::New();
  context->editor_panel_mode = crosapi::mojom::EditorPanelMode::kPromoCard;
  std::move(callback).Run(std::move(context));
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

}  // namespace ash::input_method
