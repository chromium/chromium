// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_

#include <vector>

#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::shortcut_ui {

// Handles search queries for the ChromeOS Shortcuts app.
//
// Search() is expected to be invoked by the Shortcuts UI as well as the
// Launcher search UI.
//
// Search results are obtained by matching the provided query against
// SearchConcepts indexed in the LocalSearchService and cross-referencing
// results with SearchConceptRegistry.
//
// Searches which do not provide any matches result in an empty results array.
class SearchHandler : public shortcut_customization::mojom::SearchHandler,
                      SearchConceptRegistry::Observer {
 public:
  SearchHandler(SearchConceptRegistry* search_concept_registry,
                local_search_service::LocalSearchServiceProxy*
                    local_search_service_proxy);
  ~SearchHandler() override;

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  void BindInterface(
      mojo::PendingReceiver<shortcut_customization::mojom::SearchHandler>
          pending_receiver);

  // shortcut_customization::mojom::SearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override;
  void AddSearchResultsAvailabilityObserver(
      mojo::PendingRemote<
          shortcut_customization::mojom::SearchResultsAvailabilityObserver>
          observer) override;

  // SearchConceptRegistry::Observer::
  void OnRegistryUpdated() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchHandlerTest, CompareSearchResults);

  void OnFindComplete(
      SearchCallback callback,
      local_search_service::ResponseStatus response_status,
      const std::optional<std::vector<local_search_service::Result>>&
          local_search_service_results);

  std::vector<shortcut_customization::mojom::SearchResultPtr>
  GenerateSearchResultsArray(const std::vector<local_search_service::Result>&
                                 local_search_service_results) const;

  shortcut_customization::mojom::SearchResultPtr ResultToSearchResult(
      const local_search_service::Result& result) const;

  // Returns true if |first| should be ranked before |second|.
  static bool CompareSearchResults(
      const shortcut_customization::mojom::SearchResultPtr& first,
      const shortcut_customization::mojom::SearchResultPtr& second);

  // Note: Expected to have multiple clients, so ReceiverSet/RemoteSet are used.
  mojo::ReceiverSet<shortcut_customization::mojom::SearchHandler> receivers_;
  mojo::RemoteSet<
      shortcut_customization::mojom::SearchResultsAvailabilityObserver>
      observers_;

  raw_ptr<SearchConceptRegistry> search_concept_registry_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  base::WeakPtrFactory<SearchHandler> weak_ptr_factory_{this};
};

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_