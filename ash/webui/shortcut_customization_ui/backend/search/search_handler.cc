// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/check.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash::shortcut_ui {

SearchHandler::SearchHandler() = default;
SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<shortcut_customization::mojom::SearchHandler>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const std::u16string& query,
                           uint32_t max_num_results,
                           SearchCallback callback) {
  // Searching is disabled unless the flag kSearchInShortcutsApp is enabled.
  DCHECK(features::IsSearchInShortcutsAppEnabled());

  // Until we implement real search using the LocalSearchService, temporarily
  // return fake search results.
  // TODO(cambickel): Replace these fake results with an actual call to the
  // LocalSearchService.
  std::move(callback).Run(fake_search_data::CreateFakeSearchResultList());
}

}  // namespace ash::shortcut_ui