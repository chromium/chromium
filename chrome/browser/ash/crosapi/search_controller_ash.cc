// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_ash.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

SearchControllerAsh::SearchControllerAsh(
    mojo::PendingRemote<mojom::SearchController> search_controller)
    : search_controller_(std::move(search_controller)) {
  search_controller_.set_disconnect_handler(base::BindOnce(
      &SearchControllerAsh::HandleDisconnect, weak_factory_.GetWeakPtr()));
}

SearchControllerAsh::~SearchControllerAsh() = default;

void SearchControllerAsh::Search(const std::u16string& query,
                                 SearchResultsReceivedCallback callback) {
  if (search_controller_.is_connected()) {
    search_controller_->Search(
        query, base::BindOnce(&SearchControllerAsh::BindPublisher,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
  }
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
  return search_controller_.is_connected();
}

void SearchControllerAsh::AddDisconnectHandler(DisconnectCallback handler) {
  if (!IsConnected()) {
    std::move(handler).Run(weak_factory_.GetWeakPtr());
    // USE-AFTER-FREE SAFETY: As disconnect handlers may destroy `this`, we
    // cannot refer to `this` (including using members or methods) after this
    // point.
    return;
  }

  disconnect_callbacks_.push_back(std::move(handler));
}

void SearchControllerAsh::HandleDisconnect() {
  CHECK(!IsConnected());

  base::WeakPtr<SearchControllerAsh> local_weak_this =
      weak_factory_.GetWeakPtr();
  // Move the disconnect callbacks into a local variable to ensure that they do
  // not get destroyed if `this` is destroyed.
  std::vector<DisconnectCallback> local_disconnect_callbacks =
      std::move(disconnect_callbacks_);

  // USE-AFTER-FREE SAFETY: As disconnect handlers may destroy `this`, we cannot
  // refer to `this` (including using members or methods) after this point.
  //
  // Use verbose local variable names to ensure that members are not
  // accidentally used instead.
  for (DisconnectCallback& callback : local_disconnect_callbacks) {
    std::move(callback).Run(local_weak_this);
  }
}

void SearchControllerAsh::BindPublisher(
    SearchResultsReceivedCallback callback,
    mojo::PendingAssociatedReceiver<mojom::SearchResultsPublisher> publisher) {
  publisher_receivers_.Add(this, std::move(publisher), std::move(callback));
}

}  // namespace crosapi
