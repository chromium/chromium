// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SEARCH_PROVIDER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SEARCH_PROVIDER_ASH_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for launcher search. Lives ash-chrome on UI
// thread.
// IMPORTANT: This search API should only be used by the launcher, because
// in-flight queries will be cancelled whenever a new query is issued.
class SearchProviderAsh : public mojom::SearchResultsPublisher,
                          public mojom::SearchControllerRegistry {
 public:
  SearchProviderAsh();
  SearchProviderAsh(const SearchProviderAsh&) = delete;
  SearchProviderAsh& operator=(const SearchProviderAsh&) = delete;
  ~SearchProviderAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver);

  using SearchResultsReceivedCallback =
      base::RepeatingCallback<void(std::vector<mojom::SearchResultPtr>)>;
  // Sends search query to lacros. The callback will be called each time results
  // are received from lacros via OnSearchResultsReceived().
  // If a search query is called while there is an in-flight search query, the
  // in-flight search query will be cancelled (from lacros side) before the new
  // search query is executed.
  // When lacros finishes the search, it'll terminate the connection and no more
  // results will be sent.
  void Search(const std::u16string& query,
              SearchResultsReceivedCallback callback);

  // mojom::SearchControllerRegistry overrides:
  void RegisterSearchController(
      mojo::PendingRemote<mojom::SearchController> search_controller) override;

  // mojom::SearchResultsPublisher overrides:
  void OnSearchResultsReceived(
      mojom::SearchStatus status,
      absl::optional<std::vector<mojom::SearchResultPtr>> results) override;

 private:
  void BindPublisher(
      SearchResultsReceivedCallback callback,
      mojo::PendingAssociatedReceiver<mojom::SearchResultsPublisher> publisher);

  // Since we only need one connection to fetch the results, we'll only support
  // one crosapi connection here.
  mojo::Remote<mojom::SearchController> search_controller_;

  mojo::ReceiverSet<mojom::SearchControllerRegistry> registry_receivers_;
  mojo::AssociatedReceiverSet<mojom::SearchResultsPublisher,
                              SearchResultsReceivedCallback>
      publisher_receivers_;

  base::WeakPtrFactory<SearchProviderAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SEARCH_PROVIDER_ASH_H_
