// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "ash/webui/personalization_app/search/search_tag_registry.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace personalization_app {

class EnterprisePolicyDelegate;

class SearchHandler : public mojom::SearchHandler,
                      public SearchTagRegistry::Observer {
 public:
  SearchHandler(
      local_search_service::LocalSearchServiceProxy& local_search_service_proxy,
      PrefService* pref_service,
      std::unique_ptr<EnterprisePolicyDelegate> enterprise_policy_delegate);

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  ~SearchHandler() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SearchHandler> pending_receiver);

  // mojom::SearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<mojom::SearchResultsObserver> observer) override;

  // SearchTagRegistry::Observer
  void OnRegistryUpdated() override;

 protected:
  // For testing purposes only.
  SearchHandler();

 private:
  friend class PersonalizationAppSearchHandlerTest;

  void OnLocalSearchDone(
      SearchCallback callback,
      uint32_t max_num_results,
      local_search_service::ResponseStatus response_status,
      const std::optional<std::vector<local_search_service::Result>>&
          local_search_service_results);

  std::unique_ptr<SearchTagRegistry> search_tag_registry_;
  base::ScopedObservation<SearchTagRegistry, SearchTagRegistry::Observer>
      search_tag_registry_observer_{this};
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
  mojo::RemoteSet<mojom::SearchResultsObserver> observers_;
  base::WeakPtrFactory<SearchHandler> weak_ptr_factory_{this};
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_
