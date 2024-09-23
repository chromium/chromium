// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_FACTORY_LACROS_H_
#define CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_FACTORY_LACROS_H_

#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace crosapi {

// When constructed, registers with Ash's `SearchControllerFactoryRegistry`.
class SearchControllerFactoryLacros : public mojom::SearchControllerFactory {
 public:
  SearchControllerFactoryLacros();
  ~SearchControllerFactoryLacros() override;

 private:
  // mojom::SearchControllerFactory:
  void CreateSearchControllerPicker(
      mojo::PendingReceiver<mojom::SearchController> controller,
      bool bookmarks,
      bool history,
      bool open_tabs) override;

  mojo::UniqueReceiverSet<mojom::SearchController> search_controller_receivers_;
  mojo::Receiver<mojom::SearchControllerFactory> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_CONTROLLER_FACTORY_LACROS_H_
