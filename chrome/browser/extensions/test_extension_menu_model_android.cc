// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_menu_model_android.h"

#include "chrome/browser/renderer_context_menu/context_menu_test_util.h"

namespace extensions {

TestExtensionMenuModel::TestExtensionMenuModel(
    content::RenderFrameHost& frame,
    const content::ContextMenuParams& params)
    : ExtensionMenuModel(frame, params) {}

std::optional<std::pair<ui::MenuModel*, size_t>>
TestExtensionMenuModel::GetMenuModelAndItemIndex(int command_id) {
  return context_menu_test_util::GetMenuModelAndItemIndex(this, command_id);
}

}  // namespace extensions
