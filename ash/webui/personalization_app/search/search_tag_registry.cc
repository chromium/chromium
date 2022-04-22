// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_tag_registry.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace personalization_app {

namespace {

std::string SearchConceptToId(const SearchConcept& search_concept) {
  return base::NumberToString(search_concept.message_id);
}

std::vector<int> GetMessageIds(const SearchConcept& search_concept) {
  std::vector<int> message_ids = {search_concept.message_id};
  for (auto alt_message_id : search_concept.alternate_message_ids) {
    if (alt_message_id == 0) {
      // |alternate_message_ids| defaults to 0 when out of message ids, stop
      // searching here.
      break;
    }
    message_ids.push_back(alt_message_id);
  }
  return message_ids;
}

::chromeos::local_search_service::Data SearchConceptToData(
    const SearchConcept& search_concept) {
  std::vector<::chromeos::local_search_service::Content> content_vector;

  for (auto message_id : GetMessageIds(search_concept)) {
    content_vector.emplace_back(base::NumberToString(message_id),
                                l10n_util::GetStringUTF16(message_id));
  }

  return ::chromeos::local_search_service::Data(
      /*id=*/SearchConceptToId(search_concept), std::move(content_vector));
}

std::vector<::chromeos::local_search_service::Data>
SearchConceptVectorToDataVector(
    const std::vector<const SearchConcept>& search_concepts) {
  std::vector<::chromeos::local_search_service::Data> result;
  result.reserve(search_concepts.size());
  std::transform(std::begin(search_concepts), std::end(search_concepts),
                 std::back_inserter(result), SearchConceptToData);
  return result;
}

const std::vector<const SearchConcept>& GetPersonalizationSearchConcepts() {
  static const base::NoDestructor<std::vector<const SearchConcept>>
      search_concepts(
          {{.message_id = IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE,
            .alternate_message_ids =
                {IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT1,
                 IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT2},
            .relative_url = ""}});
  return *search_concepts;
}

}  // namespace

SearchTagRegistry::SearchTagRegistry(
    ::chromeos::local_search_service::LocalSearchServiceProxy&
        local_search_service_proxy) {
  local_search_service_proxy.GetIndex(
      ::chromeos::local_search_service::IndexId::kPersonalization,
      ::chromeos::local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());
  AddSearchConcepts(GetPersonalizationSearchConcepts());
}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::AddSearchConcepts(
    const std::vector<const SearchConcept>& search_concepts) {
  for (const auto& concept : search_concepts) {
    result_id_to_search_concept_[SearchConceptToId(concept)] = &concept;
  }
  index_remote_->AddOrUpdate(
      SearchConceptVectorToDataVector(search_concepts),
      base::BindOnce(&SearchTagRegistry::OnIndexUpdateComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

const SearchConcept* SearchTagRegistry::GetSearchConceptById(
    const std::string& id) const {
  const auto it = result_id_to_search_concept_.find(id);
  if (it == result_id_to_search_concept_.end()) {
    return nullptr;
  }
  return it->second;
}

void SearchTagRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchTagRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SearchTagRegistry::OnIndexUpdateComplete() {
  for (auto& observer : observer_list_) {
    observer.OnRegistryUpdated();
  }
}

}  // namespace personalization_app
}  // namespace ash
