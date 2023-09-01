// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"

namespace ash::input_method {

EditorPanelPresetTextQuery::EditorPanelPresetTextQuery() = default;

EditorPanelPresetTextQuery::~EditorPanelPresetTextQuery() = default;

EditorPanelContext::EditorPanelContext() = default;

EditorPanelContext::~EditorPanelContext() = default;

EditorPanelManager::EditorPanelManager() = default;

EditorPanelManager::~EditorPanelManager() = default;

void EditorPanelManager::GetEditorPanelContext(
    GetEditorPanelContextCallback callback) {
  std::move(callback).Run(EditorPanelContext());
}

void EditorPanelManager::OnConsentScreenDismissed() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::OnConsentDeclined() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::StartEditingFlow() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::StartEditingFlowWithPreset(
    std::string_view text_query_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void EditorPanelManager::StartEditingFlowWithFreeform(std::string_view text) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ash::input_method
