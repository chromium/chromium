// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_TAG_REGISTRY_H_
#define ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_metadata.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::help_app {

// Processes all registered search tags by adding them to the LocalSearchService
// index and providing the additional metadata needed for search results via
// GetTagMetadata().
class SearchTagRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnRegistryUpdated() = 0;
  };

  SearchTagRegistry(local_search_service::LocalSearchServiceProxy*
                        local_search_service_proxy);
  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;
  virtual ~SearchTagRegistry();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the tag metadata associated with |result_id|, which is the ID
  // returned by the LocalSearchService. Returns SearchTagRegistry::not_found_
  // if no metadata is available.
  const SearchMetadata& GetTagMetadata(const std::string& result_id) const;

  // Adds search concepts to the index.
  // Callbacks when the LSS index is done updating.
  void Update(const std::vector<mojom::SearchConceptPtr>& search_tags,
              base::OnceCallback<void()> callback);

  // Clear the cached search concepts first. Then adds search concepts to the
  // index. Callbacks when the LSS index is done updating.
  void ClearAndUpdate(std::vector<mojom::SearchConceptPtr> search_tags,
                      base::OnceCallback<void()> callback);

  // Returned by GetTagMetadata if the id was not found.
  static const SearchMetadata not_found_;

 private:
  void NotifyRegistryUpdated();
  void NotifyRegistryAdded();

  // Index used by the LocalSearchService for string matching.
  mojo::Remote<local_search_service::mojom::Index> index_remote_;

  // In-memory cache of all results which have been added to the
  // LocalSearchService. Contents are kept in sync with |index_remote_|.
  std::unordered_map<std::string, SearchMetadata>
      result_id_to_metadata_list_map_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<SearchTagRegistry> weak_ptr_factory_{this};
};

}  // namespace ash::help_app

#endif  // ASH_WEBUI_HELP_APP_UI_SEARCH_SEARCH_TAG_REGISTRY_H_
