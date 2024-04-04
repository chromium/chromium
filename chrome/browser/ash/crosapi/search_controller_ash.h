// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_ASH_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-shared.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

// Wrapper around a remote `SearchController` for omnibox search.
//
// Internally, this class implements one `SearchResultsPublisher` per search
// request (even though the search controller can only execute one search at a
// time) in order to notify old clients if their search is preempted. It's
// likely that this can be simplified in the future.
class SearchControllerAsh : public mojom::SearchResultsPublisher {
 public:
  using SearchResultsReceivedCallback =
      base::RepeatingCallback<void(std::vector<mojom::SearchResultPtr>)>;

  SearchControllerAsh();
  SearchControllerAsh(const SearchControllerAsh&) = delete;
  SearchControllerAsh& operator=(const SearchControllerAsh&) = delete;
  ~SearchControllerAsh() override;

  void RegisterSearchController(
      mojo::PendingRemote<mojom::SearchController> search_controller);

  bool IsSearchControllerConnected() const;

  // Sends search query to lacros. The callback will be called each time results
  // are received from lacros via OnSearchResultsReceived().
  // If a search query is called while there is an in-flight search query, the
  // in-flight search query will be cancelled (from lacros side) before the new
  // search query is executed.
  // When lacros finishes the search, it'll terminate the connection and no more
  // results will be sent.
  void Search(const std::u16string& query,
              SearchResultsReceivedCallback callback);

  // mojom::SearchResultsPublisher overrides:
  void OnSearchResultsReceived(
      mojom::SearchStatus status,
      std::optional<std::vector<mojom::SearchResultPtr>> results) override;

 private:
  void BindPublisher(
      SearchResultsReceivedCallback callback,
      mojo::PendingAssociatedReceiver<mojom::SearchResultsPublisher> publisher);

  // Since we only need one connection to fetch the results, we'll only support
  // one crosapi connection here.
  mojo::Remote<mojom::SearchController> search_controller_;

  mojo::AssociatedReceiverSet<mojom::SearchResultsPublisher,
                              SearchResultsReceivedCallback>
      publisher_receivers_;

  base::WeakPtrFactory<SearchControllerAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_ASH_H_
