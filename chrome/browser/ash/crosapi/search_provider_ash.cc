// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_provider_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {

SearchProviderAsh::SearchProviderAsh() = default;
SearchProviderAsh::~SearchProviderAsh() = default;

void SearchProviderAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SearchControllerRegistry> pending_receiver) {
  registry_receivers_.Add(this, std::move(pending_receiver));
}

void SearchProviderAsh::Search(const std::u16string& query,
                               SearchResultsReceivedCallback callback) {
  if (search_controller_) {
    search_controller_->Search(query, std::move(callback));
  }
}

void SearchProviderAsh::RegisterSearchController(
    mojo::PendingRemote<mojom::SearchController> search_controller) {
  if (search_controller_ && search_controller_->IsConnected()) {
    LOG(ERROR) << "Search Controller is already connected.";
    return;
  }
  search_controller_ =
      std::make_unique<SearchControllerAsh>(std::move(search_controller));
}

bool SearchProviderAsh::IsSearchControllerConnected() const {
  return search_controller_ && search_controller_->IsConnected();
}

}  // namespace crosapi
