// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_controller_factory_lacros.h"

#include <memory>
#include <utility>

#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/lacros/launcher_search/search_controller_lacros.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {

SearchControllerFactoryLacros::SearchControllerFactoryLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsSupported<mojom::SearchControllerFactory>()) {
    return;
  }
  service->BindPendingReceiverOrRemote<
      mojo::PendingRemote<mojom::SearchControllerFactory>,
      &crosapi::mojom::Crosapi::BindSearchControllerFactory>(
      receiver_.BindNewPipeAndPassRemote());
}

SearchControllerFactoryLacros::~SearchControllerFactoryLacros() = default;

void SearchControllerFactoryLacros::CreateSearchControllerPicker(
    mojo::PendingReceiver<mojom::SearchController> controller,
    bool bookmarks,
    bool history,
    bool open_tabs) {
  search_controller_receivers_.Add(
      std::make_unique<SearchControllerLacros>(
          crosapi::ProviderTypesPicker(bookmarks, history, open_tabs)),
      std::move(controller));
}

}  // namespace crosapi
