// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_factory_ash.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {

SearchControllerFactoryAsh::SearchControllerFactoryAsh() = default;
SearchControllerFactoryAsh::~SearchControllerFactoryAsh() = default;

void SearchControllerFactoryAsh::BindRemote(
    mojo::PendingRemote<mojom::SearchControllerFactory> remote) {
  if (search_controller_factory_.is_bound() &&
      search_controller_factory_.is_connected()) {
    LOG(ERROR) << "Search Controller Factory is already connected.";
    return;
  }

  search_controller_factory_.reset();
  search_controller_factory_.Bind(std::move(remote));

  for (Observer& observer : observers_) {
    observer.OnSearchControllerFactoryBound(this);
  }
}

void SearchControllerFactoryAsh::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
  if (IsBound()) {
    obs->OnSearchControllerFactoryBound(this);
  }
}

void SearchControllerFactoryAsh::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

std::unique_ptr<SearchControllerAsh>
SearchControllerFactoryAsh::CreateSearchControllerPicker(bool bookmarks,
                                                         bool history,
                                                         bool open_tabs) {
  if (!search_controller_factory_.is_bound()) {
    return nullptr;
  }

  mojo::PendingRemote<mojom::SearchController> remote;
  search_controller_factory_->CreateSearchControllerPicker(
      remote.InitWithNewPipeAndPassReceiver(), bookmarks, history, open_tabs);
  return std::make_unique<SearchControllerAsh>(std::move(remote));
}

bool SearchControllerFactoryAsh::IsBound() const {
  return search_controller_factory_.is_bound();
}

}  // namespace crosapi
