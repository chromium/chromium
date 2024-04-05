// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_ash.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

SearchControllerAsh::SearchControllerAsh() = default;
SearchControllerAsh::~SearchControllerAsh() = default;

void SearchControllerAsh::Search(const std::u16string& query,
                                 SearchResultsReceivedCallback callback) {
  if (search_controller_.is_bound() && search_controller_.is_connected()) {
    search_controller_->Search(
        query, base::BindOnce(&SearchControllerAsh::BindPublisher,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void SearchControllerAsh::RegisterSearchController(
    mojo::PendingRemote<mojom::SearchController> search_controller) {
  if (search_controller_.is_bound() && search_controller_.is_connected()) {
    LOG(ERROR) << "Search Controller is already connected.";
    return;
  }

  search_controller_.reset();
  search_controller_.Bind(std::move(search_controller));
}

void SearchControllerAsh::OnSearchResultsReceived(
    mojom::SearchStatus status,
    std::optional<std::vector<mojom::SearchResultPtr>> results) {
  switch (status) {
    case mojom::SearchStatus::kError: {
      LOG(ERROR) << "Search failed.";
      publisher_receivers_.Remove(publisher_receivers_.current_receiver());
      return;
    }
    case mojom::SearchStatus::kDone: {
      const auto& callback = publisher_receivers_.current_context();
      if (results.has_value() && !callback.is_null()) {
        callback.Run(std::move(results.value()));
      }
      return;
    }
    case mojom::SearchStatus::kInProgress:
    case mojom::SearchStatus::kCancelled:
    case mojom::SearchStatus::kBackendUnavailable: {
      return;
    }
  }
}

bool SearchControllerAsh::IsConnected() const {
  return search_controller_.is_bound() && search_controller_.is_connected();
}

void SearchControllerAsh::BindPublisher(
    SearchResultsReceivedCallback callback,
    mojo::PendingAssociatedReceiver<mojom::SearchResultsPublisher> publisher) {
  publisher_receivers_.Add(this, std::move(publisher), std::move(callback));
}

}  // namespace crosapi
