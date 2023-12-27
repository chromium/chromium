// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_concept.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::help_app {

// This enum class is defined in the `search_handler.cc` with explanations.
namespace {
enum class CacheStatus;
}

// Handles search queries for the help app. Search() is expected to be invoked
// by the Launcher search UI. Search results are obtained by matching the
// provided query against search tags indexed in the LocalSearchService and
// cross-referencing results with SearchTagRegistry.
//
// Either `Update()` or `OnProfileDirAvailable()` needs to be called before
// `Search()` will work. Searches that do not work or do not provide any matches
// result in an empty results array.
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

  void OnProfileDirAvailable(const base::FilePath& profile_dir);

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
      const std::optional<std::vector<local_search_service::Result>>&
          local_search_service_results);

  void OnPersistenceReadComplete(std::vector<mojom::SearchConceptPtr> concepts);

  // Converts an LSS search result to the format used by this search handler.
  mojom::SearchResultPtr ResultToSearchResult(
      const local_search_service::Result& result) const;

  raw_ptr<SearchTagRegistry> search_tag_registry_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;

  CacheStatus cache_status_;

  // The time this class is constructed. This is also used as a flag to indicate
  // if the availability latency has been logged. After logging, the
  // `construction_time_` will be reset to null.
  base::TimeTicks construction_time_;

  // Note: Expected to have multiple clients, so ReceiverSet/RemoteSet are used.
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
  mojo::RemoteSet<mojom::SearchResultsObserver> observers_;

  std::unique_ptr<SearchConcept> persistence_;

  base::WeakPtrFactory<SearchHandler> weak_ptr_factory_{this};
};

}  // namespace ash::help_app

#endif  // ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_HANDLER_H_
