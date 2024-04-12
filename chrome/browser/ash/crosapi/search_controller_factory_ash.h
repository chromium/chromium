// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_

#include <memory>

#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

class SearchControllerAsh;

class SearchControllerFactoryAsh {
 public:
  SearchControllerFactoryAsh();
  ~SearchControllerFactoryAsh();

  void BindRemote(mojo::PendingRemote<mojom::SearchControllerFactory> remote);

  // Creates a `SearchControllerAsh` for Picker with the given parameters.
  // This function returns nullptr iff `!IsBound()`.
  std::unique_ptr<SearchControllerAsh>
  CreateSearchControllerPicker(bool bookmarks, bool history, bool open_tabs);

  bool IsBound() const;

 private:
  mojo::Remote<mojom::SearchControllerFactory> search_controller_factory_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_
