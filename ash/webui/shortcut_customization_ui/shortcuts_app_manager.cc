// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::shortcut_ui {

// TODO(cambickel): Update this constructor to fetch the list of accelerators
// and use them to call SearchConceptRegistry.AddSearchConcepts, to populate
// the LSS index.
ShortcutsAppManager::ShortcutsAppManager(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy) {
  if (features::IsSearchInShortcutsAppEnabled()) {
    search_concept_registry_ =
        std::make_unique<SearchConceptRegistry>(*local_search_service_proxy);
    search_handler_ = std::make_unique<SearchHandler>();
  }
}

ShortcutsAppManager::~ShortcutsAppManager() = default;

void ShortcutsAppManager::Shutdown() {
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  search_handler_.reset();
  search_concept_registry_.reset();
}

}  // namespace ash::shortcut_ui
