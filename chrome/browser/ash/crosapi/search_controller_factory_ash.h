// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

class SearchControllerAsh;

class SearchControllerFactoryAsh {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSearchControllerFactoryBound(
        SearchControllerFactoryAsh* factory) = 0;
  };

  SearchControllerFactoryAsh();
  ~SearchControllerFactoryAsh();

  void BindRemote(mojo::PendingRemote<mojom::SearchControllerFactory> remote);

  // Calls `OnSearchControllerFactoryBound` immediately if this instance is
  // already bound.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Creates a `SearchControllerAsh` for Picker with the given parameters.
  // This function returns nullptr iff `!IsBound()`.
  std::unique_ptr<SearchControllerAsh>
  CreateSearchControllerPicker(bool bookmarks, bool history, bool open_tabs);

  bool IsBound() const;

 private:
  mojo::Remote<mojom::SearchControllerFactory> search_controller_factory_;
  base::ObserverList<Observer> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SEARCH_CONTROLLER_FACTORY_ASH_H_
