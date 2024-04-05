// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_provider_ash.h"

#include <string>
#include <utility>

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
  search_controller_.Search(query, std::move(callback));
}

void SearchProviderAsh::RegisterSearchController(
    mojo::PendingRemote<mojom::SearchController> search_controller) {
  search_controller_.RegisterSearchController(std::move(search_controller));
}

bool SearchProviderAsh::IsSearchControllerConnected() const {
  return search_controller_.IsConnected();
}

}  // namespace crosapi
