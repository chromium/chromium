// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_

#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace help_app {

// Handles search queries for the help app. Search() is expected to be invoked
// by the Launcher search UI. Search results are obtained by matching the
// provided query against search tags indexed in the LocalSearchService and
// cross-referencing results with SearchTagRegistry.
//
// Searches that do not provide any matches result in an empty results array.
class SearchHandler : public mojom::SearchHandler,
                      public SearchTagRegistry::Observer {
 public:
  SearchHandler(SearchTagRegistry* search_tag_registry,
                local_search_service::LocalSearchServiceProxy*
                    local_search_service_proxy);
  ~SearchHandler() override;

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::SearchHandler> pending_receiver);

  // mojom::SearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override;
  void Update(std::vector<mojom::SearchConceptPtr> concepts,
              UpdateCallback callback) override;
  void Observe(
      mojo::PendingRemote<mojom::SearchResultsObserver> observer) override;

 private:
  // SearchTagRegistry::Observer:
  void OnRegistryUpdated() override;

  std::vector<mojom::SearchResultPtr> GenerateSearchResultsArray(
      const std::vector<local_search_service::Result>&
          local_search_service_results,
      uint32_t max_num_results) const;

  void OnFindComplete(
      SearchCallback callback,
      uint32_t max_num_results,
      local_search_service::ResponseStatus response_status,
      const absl::optional<std::vector<local_search_service::Result>>&
          local_search_service_results);

  // Converts an LSS search result to the format used by this search handler.
  mojom::SearchResultPtr ResultToSearchResult(
      const local_search_service::Result& result) const;

  SearchTagRegistry* search_tag_registry_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;

  // Whether or not the first Update has finished yet, which means the Search
  // Handler is ready to search.
  bool is_ready_;

  // Note: Expected to have multiple clients, so ReceiverSet/RemoteSet are used.
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
  mojo::RemoteSet<mojom::SearchResultsObserver> observers_;

  base::WeakPtrFactory<SearchHandler> weak_ptr_factory_{this};
};

}  // namespace help_app
}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_
