// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"

#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace ash::shortcut_ui {

namespace {

// Given text accelerator properties, return a string that will be searchable
// by the Local Search Service.
std::u16string TextAcceleratorToContentString(
    const mojom::TextAcceleratorPropertiesPtr& text_accelerator_properties) {
  // To get the searchable part of a Text Accelerator, combine all of the
  // TextAcceleratorParts into one string.
  std::u16string output;
  for (const auto& part : text_accelerator_properties->parts) {
    base::StrAppend(&output, {part->text});
  }
  return output;
}

// Given text accelerator properties, return an ID that will be used for the
// Local Search Service Content object, which represents a searchable piece of
// data.
std::string StandardAcceleratorToContentId(
    const mojom::StandardAcceleratorPropertiesPtr&
        standard_accelerator_properties) {
  // ID strings only need to be unique within a given SearchConcept,
  // so it's sufficient to create an ID from the accelerator modifiers and
  // key_code.
  return base::StrCat(
      {base::NumberToString(
           standard_accelerator_properties->accelerator.modifiers()),
       "-",
       base::NumberToString(
           standard_accelerator_properties->accelerator.key_code())});
}

// Given standard accelerator properties, return a string that will be
// searchable by the Local Search Service.
std::u16string StandardAcceleratorToContentString(
    const mojom::StandardAcceleratorPropertiesPtr&
        standard_accelerator_properties) {
  // TODO(cambickel) GetShortcutText outputs a shortcut with "+" as the
  // delimiter, e.g. "Ctrl+Shift+Q". We want to delimit with spaces, e.g. "Ctrl
  // Shift Q". This has a fair amount of edge cases so we'll handle this later.
  // TODO(cambickel) GetShortcutText also doesn't properly handle certain
  // "special" keys, e.g. BrightnessDown. For now, we'll use it, but we should
  // eventually switch to a more robust solution.
  return standard_accelerator_properties->accelerator.GetShortcutText();
}

}  // namespace

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
  // and once for each AcceleratorInfo.
  local_search_service_contents.reserve(
      search_concept.accelerator_infos.size() + 1);

  // First, add this SearchConcept's description as searchable Content.
  // Note that the Content ID is prefixed with the SearchConcept ID, since
  // Content IDs have to be unique within an entire LSS index. However, it's not
  // necessary to store this Content ID since it only be used internally to the
  // LSS.
  local_search_service_contents.emplace_back(
      /*id=*/base::StrCat({search_concept.id, "-description"}),
      /*content=*/search_concept.accelerator_layout_info->description);

  // Next, for each accelerator_info, register it (and its accelerators in the
  // case of standard accelerators) as searchable Content.
  for (const auto& accelerator_info : search_concept.accelerator_infos) {
    // Content->id needs to be unique across the entire index,
    // so we prefix it with the SearchConcept's id.
    // The part of the id besides the SearchConcept's id only
    // needs to be unique within that SearchConcept's accelerators.
    std::string content_id;
    std::u16string content_string;

    if (accelerator_info->layout_properties->is_text_accelerator()) {
      // The content_id for a text accelerator doesn't need to be based on the
      // content of the text accelerator because text accelerators have only
      // one entry in SearchConcept.accelerator_infos.
      content_id = base::StrCat({search_concept.id, "-text-accelerator"});
      content_string = TextAcceleratorToContentString(
          accelerator_info->layout_properties->get_text_accelerator());
    } else {
      content_id = base::StrCat(
          {search_concept.id, "-",
           StandardAcceleratorToContentId(accelerator_info->layout_properties
                                              ->get_standard_accelerator())});
      content_string = StandardAcceleratorToContentString(
          accelerator_info->layout_properties->get_standard_accelerator());
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