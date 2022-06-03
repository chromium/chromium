// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_tag_registry.h"

#include <utility>

#include "ash/webui/help_app_ui/search/search_metadata.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"

namespace ash {
namespace help_app {
namespace {

// Converts from search concept to the format required by LSS for indexing.
std::vector<local_search_service::Data> ConceptVectorToDataVector(
    const std::vector<mojom::SearchConceptPtr>& search_tags) {
  std::vector<local_search_service::Data> data_list;

  for (const auto& concept : search_tags) {
    // Create a list of Content objects, which use the stringified version of
    // the tag list index as identifiers.
    std::vector<local_search_service::Content> content_list;
    int tag_num = 0;
    for (const std::u16string& tag : concept->tags) {
      content_list.emplace_back(
          /*id=*/base::NumberToString(tag_num),
          /*content=*/tag);
      ++tag_num;
    }

    data_list.emplace_back(concept->id, std::move(content_list),
                           concept->tag_locale);
  }
  return data_list;
}

}  // namespace

const SearchMetadata SearchTagRegistry::not_found_ = SearchMetadata();

SearchTagRegistry::SearchTagRegistry(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy) {
  local_search_service_proxy->GetIndex(
      local_search_service::IndexId::kHelpAppLauncher,
      local_search_service::Backend::kInvertedIndex,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());
}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchTagRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SearchTagRegistry::Update(
    const std::vector<mojom::SearchConceptPtr>& search_tags,
    base::OnceCallback<void()> callback) {
  for (const auto& concept : search_tags) {
    // This replaces the value if the key already exists.
    result_id_to_metadata_list_map_[concept->id] =
        SearchMetadata(concept->title, concept->main_category,
                       concept->url_path_with_parameters);
  }

  index_remote_->AddOrUpdate(
      ConceptVectorToDataVector(search_tags),
      base::BindOnce(&SearchTagRegistry::NotifyRegistryAdded,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

const SearchMetadata& SearchTagRegistry::GetTagMetadata(
    const std::string& result_id) const {
  const auto it = result_id_to_metadata_list_map_.find(result_id);
  if (it == result_id_to_metadata_list_map_.end()) {
    return not_found_;
  }
  return it->second;
}

void SearchTagRegistry::NotifyRegistryUpdated() {
  for (auto& observer : observer_list_) {
    observer.OnRegistryUpdated();
  }
}

void SearchTagRegistry::NotifyRegistryAdded() {
  NotifyRegistryUpdated();
}

}  // namespace help_app
}  // namespace ash
