// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_provider_ash.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
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

SearchControllerAsh* SearchProviderAsh::GetController() {
  return search_controller_.get();
}

void SearchProviderAsh::RegisterSearchController(
    mojo::PendingRemote<mojom::SearchController> search_controller) {
  if (search_controller_) {
    LOG(ERROR) << "Search Controller is already connected.";
    return;
  }
  search_controller_ =
      std::make_unique<SearchControllerAsh>(std::move(search_controller));
  search_controller_->AddDisconnectHandler(
      base::BindOnce(&SearchProviderAsh::OnSearchControllerDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void SearchProviderAsh::OnSearchControllerDisconnected(
    base::WeakPtr<SearchControllerAsh> controller) {
  // No other disconnect handler should have been added, so this controller
  // should always be valid.
  CHECK(controller);

  search_controller_.reset();
}

}  // namespace crosapi
