// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Implements crosapi interface for launcher search controller.
class SearchControllerLacros : public mojom::SearchController {
 public:
  SearchControllerLacros();
  SearchControllerLacros(const SearchControllerLacros&) = delete;
  SearchControllerLacros& operator=(const SearchControllerLacros&) = delete;
  ~SearchControllerLacros() override;

 private:
  // mojom::SearchController:
  void Search(const std::u16string& query, SearchCallback callback) override;

  mojo::AssociatedRemote<mojom::SearchResultsPublisher> publisher_;
  mojo::Receiver<mojom::SearchController> receiver_{this};

  base::WeakPtrFactory<SearchControllerLacros> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_LACROS_H_
