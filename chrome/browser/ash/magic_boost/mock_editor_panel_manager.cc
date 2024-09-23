// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

namespace ash {

MockEditorPanelManager::MockEditorPanelManager()
    : input_method::EditorPanelManager(/*delegate=*/nullptr) {}

MockEditorPanelManager::~MockEditorPanelManager() = default;

}  // namespace ash
