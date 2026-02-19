// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_test_util.h"

#include <vector>

#include "ui/base/models/menu_model.h"

namespace context_menu_test_util {

bool GetMenuModelAndItemIndex(ui::MenuModel* search_model,
                              int command_id,
                              raw_ptr<ui::MenuModel>* found_model,
                              size_t* found_index) {
  std::vector<ui::MenuModel*> models_to_search;
  models_to_search.push_back(search_model);

  while (!models_to_search.empty()) {
    ui::MenuModel* model = models_to_search.back();
    models_to_search.pop_back();
    for (size_t i = 0; i < model->GetItemCount(); i++) {
      if (model->GetCommandIdAt(i) == command_id) {
        *found_model = model;
        *found_index = i;
        return true;
      }
      if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
        models_to_search.push_back(model->GetSubmenuModelAt(i));
      }
    }
  }

  return false;
}

}  // namespace context_menu_test_util
