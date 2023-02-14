// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/check.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash::shortcut_ui {

namespace {

// TODO(cambickel): Remove this helper function when we implement real querying.
ash::mojom::AcceleratorInfoPtr CreateFakeAcceleratorInfo() {
  ui::Accelerator* accelerator = new ui::Accelerator();
  ash::mojom::AcceleratorInfoPtr info_mojom =
      ash::mojom::AcceleratorInfo::New();
  info_mojom->locked = true;
  info_mojom->type = ash::mojom::AcceleratorType::kDefault;
  info_mojom->state = ash::mojom::AcceleratorState::kEnabled;
  info_mojom->layout_properties =
      ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
          ash::mojom::StandardAcceleratorProperties::New(*accelerator,
                                                         u"FakeKey"));
  return info_mojom;
}

// TODO(cambickel): Remove this helper function when we implement real querying.
ash::mojom::AcceleratorLayoutInfoPtr CreateFakeAcceleratorLayoutInfo(
    ash::mojom::AcceleratorCategory category,
    ash::mojom::AcceleratorSubcategory sub_category,
    const std::u16string& description,
    ash::mojom::AcceleratorSource source,
    uint32_t action) {
  ash::mojom::AcceleratorLayoutInfoPtr layout_info_mojom =
      ash::mojom::AcceleratorLayoutInfo::New();
  layout_info_mojom->category = category;
  layout_info_mojom->sub_category = sub_category;
  layout_info_mojom->description = description;
  layout_info_mojom->style = ash::mojom::AcceleratorLayoutStyle::kDefault;
  layout_info_mojom->source = source;
  layout_info_mojom->action = action;
  return layout_info_mojom;
}

// TODO(cambickel): Remove this helper function when we implement real querying.
shortcut_customization::mojom::SearchResultPtr CreateSearchResult(
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_infos,
    ash::mojom::AcceleratorLayoutInfoPtr accelerator_layout_info) {
  shortcut_customization::mojom::SearchResultPtr result =
      shortcut_customization::mojom::SearchResult::New();
  result->accelerator_infos = std::move(accelerator_infos);
  result->accelerator_layout_info = std::move(accelerator_layout_info);
  result->relevance_score = 0.5;
  return result;
}

// TODO(cambickel): Remove this helper function when we implement real querying.
std::vector<shortcut_customization::mojom::SearchResultPtr>
CreateFakeSearchResultList() {
  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results =
      {};

  std::vector<ash::mojom::AcceleratorInfoPtr>
      accelerator_infos_for_first_result = {};
  accelerator_infos_for_first_result.push_back(CreateFakeAcceleratorInfo());
  search_results.push_back(CreateSearchResult(
      std::move(accelerator_infos_for_first_result),
      CreateFakeAcceleratorLayoutInfo(
          ash::mojom::AcceleratorCategory::kDebug,
          ash::mojom::AcceleratorSubcategory::kGeneral, u"first result",
          ash::mojom::AcceleratorSource::kAsh, 1)));

  std::vector<ash::mojom::AcceleratorInfoPtr>
      accelerator_infos_for_second_result = {};
  accelerator_infos_for_second_result.push_back(CreateFakeAcceleratorInfo());
  search_results.push_back(CreateSearchResult(
      std::move(accelerator_infos_for_second_result),
      CreateFakeAcceleratorLayoutInfo(
          ash::mojom::AcceleratorCategory::kDebug,
          ash::mojom::AcceleratorSubcategory::kGeneral, u"second result",
          ash::mojom::AcceleratorSource::kAsh, 2)));

  return search_results;
}

}  // namespace

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
  std::move(callback).Run(CreateFakeSearchResultList());
}

}  // namespace ash::shortcut_ui