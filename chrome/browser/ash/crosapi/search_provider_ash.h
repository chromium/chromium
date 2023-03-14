// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SEARCH_PROVIDER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SEARCH_PROVIDER_ASH_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// IMPORTANT: This search API should only be used by the launcher, because
// in-flight queries will be cancelled whenever a new query is issued.
//
// This class is the crosapi entry-point for launcher omnibox search. It lives
// in ash-chrome on the UI thread. Omnibox results are obtained as follows:
//
//   1) On creation, each lacros instance requests a `SearchControllerRegistry`
//      (all of which are implemented by the same instance of this class), and
//      sends across a handle to its search controller.
//
//      TODO(1329709): Currently, a search controller is accepted exactly if a
//      new lacros instance is created after the previous instance is dead.
//      Correct this.
//
//   2) Once the search controller handle has been received, clients (i.e. code
//      in ash-chrome) can use the `Search` method to fetch omnibox results,
//      which delegates to the search controller over Mojo.
//
//      Internally, this class implements one `SearchResultsPublisher` per
//      search request (even though the search controller can only execute one
//      search at a time) in order to notify old clients if their search is
//      preempted. It's likely that this can be simplified in the future.
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

  bool IsSearchControllerConnected() const;

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
