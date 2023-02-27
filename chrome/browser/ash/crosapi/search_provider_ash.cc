// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_provider_ash.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"

namespace crosapi {

SearchProviderAsh::SearchProviderAsh() = default;
SearchProviderAsh::~SearchProviderAsh() = default;

void SearchProviderAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SearchControllerRegistry> pending_receiver) {
  registry_receivers_.Add(this, std::move(pending_receiver));
}

void SearchProviderAsh::Search(const std::u16string& query,
                               SearchResultsReceivedCallback callback) {
  if (search_controller_.is_bound() && search_controller_.is_connected()) {
    search_controller_->Search(
        query, base::BindOnce(&SearchProviderAsh::BindPublisher,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void SearchProviderAsh::RegisterSearchController(
    mojo::PendingRemote<mojom::SearchController> search_controller) {
  if (search_controller_.is_bound() && search_controller_.is_connected()) {
    LOG(ERROR) << "Search Controller is already connected.";
    return;
  }

  search_controller_.reset();
  search_controller_.Bind(std::move(search_controller));
}

void SearchProviderAsh::OnSearchResultsReceived(
    mojom::SearchStatus status,
    absl::optional<std::vector<mojom::SearchResultPtr>> results) {
  switch (status) {
    case mojom::SearchStatus::kError: {
      LOG(ERROR) << "Search failed.";
      publisher_receivers_.Remove(publisher_receivers_.current_receiver());
      return;
    }
    case mojom::SearchStatus::kDone: {
      const auto& callback = publisher_receivers_.current_context();
      if (results.has_value() && !callback.is_null())
        callback.Run(std::move(results.value()));
      return;
    }
    case mojom::SearchStatus::kInProgress:
    case mojom::SearchStatus::kCancelled:
    case mojom::SearchStatus::kBackendUnavailable: {
      return;
    }
  }
}

bool SearchProviderAsh::IsSearchControllerConnected() const {
  return search_controller_.is_bound() && search_controller_.is_connected();
}

void SearchProviderAsh::BindPublisher(
    SearchResultsReceivedCallback callback,
    mojo::PendingAssociatedReceiver<mojom::SearchResultsPublisher> publisher) {
  publisher_receivers_.Add(this, std::move(publisher), std::move(callback));
}

}  // namespace crosapi
