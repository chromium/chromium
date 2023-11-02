// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view.h"

namespace ash {

// This class defines the interface used to add, modify, and remove networks
// from the network list of the detailed network device page within the quick
// settings. This class includes the definition of the factory used to create
// instances of implementations of this class.
class ASH_EXPORT NetworkListViewController {
 public:
  class Factory {
   public:
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    static std::unique_ptr<NetworkListViewController> Create(
        NetworkDetailedNetworkView* network_detailed_network_view);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    Factory() = default;

    virtual std::unique_ptr<NetworkListViewController> CreateForTesting() = 0;
  };

  NetworkListViewController(const NetworkListViewController&) = delete;
  NetworkListViewController& operator=(const NetworkListViewController&) =
      delete;
  virtual ~NetworkListViewController() = default;

 protected:
  NetworkListViewController() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_H_
