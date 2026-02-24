// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_test_util.h"

#include <optional>
#include <utility>
#include <vector>

#include "ui/base/models/menu_model.h"

namespace context_menu_test_util {

std::optional<std::pair<ui::MenuModel*, size_t>> GetMenuModelAndItemIndex(
    ui::MenuModel* search_model,
    int command_id) {
  std::vector<ui::MenuModel*> models_to_search;
  models_to_search.push_back(search_model);

  while (!models_to_search.empty()) {
    ui::MenuModel* model = models_to_search.back();
    models_to_search.pop_back();
    for (size_t i = 0; i < model->GetItemCount(); i++) {
      if (model->GetCommandIdAt(i) == command_id) {
        return std::make_optional(std::make_pair(model, i));
      }
      if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
        models_to_search.push_back(model->GetSubmenuModelAt(i));
      }
    }
  }

  return std::nullopt;
}

}  // namespace context_menu_test_util
