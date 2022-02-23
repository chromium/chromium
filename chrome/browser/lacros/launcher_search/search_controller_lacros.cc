// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_controller_lacros.h"

#include <utility>

#include "chromeos/lacros/lacros_service.h"

namespace crosapi {

SearchControllerLacros::SearchControllerLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<mojom::SearchControllerRegistry>())
    return;
  service->GetRemote<mojom::SearchControllerRegistry>()
      ->RegisterSearchController(receiver_.BindNewPipeAndPassRemote());
}

SearchControllerLacros::~SearchControllerLacros() = default;

void SearchControllerLacros::Search(const std::u16string& query,
                                    SearchCallback callback) {
  // Reset the remote and send a new pending receiver to ash.
  publisher_.reset();
  std::move(callback).Run(publisher_.BindNewEndpointAndPassReceiver());

  // TODO(crbug/1228587): Fill the results here.
}

}  // namespace crosapi
