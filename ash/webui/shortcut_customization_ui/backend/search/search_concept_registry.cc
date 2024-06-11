// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"

#include <string>
#include <vector>

#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace ash::shortcut_ui {

SearchConceptRegistry::SearchConceptRegistry(
    local_search_service::LocalSearchServiceProxy& local_search_service_proxy) {
  local_search_service_proxy.GetIndex(
      local_search_service::IndexId::kShortcutsApp,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());
}

SearchConceptRegistry::~SearchConceptRegistry() = default;

void SearchConceptRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchConceptRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SearchConceptRegistry::SetSearchConcepts(
    std::vector<SearchConcept> search_concepts) {
  index_remote_->ClearIndex(base::BindOnce(
      &SearchConceptRegistry::SetSearchConceptsHelper,
      weak_ptr_factory_.GetWeakPtr(), std::move(search_concepts)));
}

void SearchConceptRegistry::SetSearchConceptsHelper(
    std::vector<SearchConcept> search_concepts) {
  // Reset the map since the search index has been reset.
  result_id_to_search_concept_.clear();

  std::vector<local_search_service::Data> local_search_service_data;
  local_search_service_data.reserve(search_concepts.size());

  for (auto& search_concept : search_concepts) {
    // The SearchConcept should not have been added yet.
    DCHECK(!result_id_to_search_concept_.contains(search_concept.id));

    // Add this SearchConcept to the local map and register it with the Local
    // Search Service index.
    local_search_service_data.emplace_back(SearchConceptToData(search_concept));
    result_id_to_search_concept_.insert(
        {search_concept.id, std::move(search_concept)});
  }

  index_remote_->AddOrUpdate(
      std::move(local_search_service_data),
      base::BindOnce(&SearchConceptRegistry::NotifyRegistryUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchConceptRegistry::NotifyRegistryUpdated() {
  for (auto& observer : observer_list_) {
    observer.OnRegistryUpdated();
  }
}

const SearchConcept* SearchConceptRegistry::GetSearchConceptById(
    const std::string& id) const {
  const auto& matching_search_concept_iterator =
      result_id_to_search_concept_.find(id);
  if (matching_search_concept_iterator == result_id_to_search_concept_.end()) {
    return nullptr;
  }
  return &matching_search_concept_iterator->second;
}

// Given a SearchConcept, return a Data object, which represents a single
// searchable item.
local_search_service::Data SearchConceptRegistry::SearchConceptToData(
    const SearchConcept& search_concept) {
  // One Data object contains a list of Content objects that can match against
  // queries.
  std::vector<local_search_service::Content> local_search_service_contents;
  // Reserve the size for the vector since we insert once for the description
  // and at most once more in the case of text accelerators.
  local_search_service_contents.reserve(2);

  // First, add this SearchConcept's description as searchable Content.
  // Note that the Content ID is prefixed with the SearchConcept ID, since
  // Content IDs have to be unique within an entire LSS index. However, it's not
  // necessary to store this Content ID since it only be used internally to the
  // LSS.
  local_search_service_contents.emplace_back(
      /*id=*/base::StrCat({search_concept.id, "-description"}),
      /*content=*/search_concept.accelerator_layout_info->description);

  // All SearchConcepts should contain at least one AcceleratorInfo.
  DCHECK(search_concept.accelerator_infos.size() > 0);

  // Get the first AcceleratorInfo to check if it's a text accelerator. Note
  // that text accelerators should only have one entry in accelerator_infos.
  const mojom::AcceleratorInfoPtr& first_accelerator_info =
      search_concept.accelerator_infos.at(0);

  // Only text accelerators should become searchable LSS Content.
  if (first_accelerator_info->layout_properties->is_text_accelerator()) {
    // Content->id needs to be unique across the entire index,
    // so we prefix it with the SearchConcept's id.
    std::string content_id =
        base::StrCat({search_concept.id, "-text-accelerator"});

    // To get the searchable part of a Text Accelerator, combine all of the
    // TextAcceleratorParts into one string.
    std::u16string content_string;
    for (const auto& part :
         first_accelerator_info->layout_properties->get_text_accelerator()
             ->parts) {
      base::StrAppend(&content_string, {part->text});
    }

    local_search_service_contents.emplace_back(
        /*id=*/content_id,
        /*content=*/content_string);
  }

  return local_search_service::Data(
      /*id=*/search_concept.id,
      /*contents=*/std::move(local_search_service_contents));
}

}  // namespace ash::shortcut_ui