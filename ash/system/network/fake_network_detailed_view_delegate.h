// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_view.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::NetworkStatePropertiesPtr;

}

// Fake of NetworkDetailedView::Delegate implementation.
class ASH_EXPORT FakeNetworkDetailedViewDelegate
    : public NetworkDetailedView::Delegate {
 public:
  FakeNetworkDetailedViewDelegate();
  FakeNetworkDetailedViewDelegate(const FakeNetworkDetailedViewDelegate&) =
      delete;
  FakeNetworkDetailedViewDelegate& operator=(
      const FakeNetworkDetailedViewDelegate&) = delete;
  ~FakeNetworkDetailedViewDelegate() override;

  // NetworkDetailedView::Delegate:
  void OnNetworkListItemSelected(
      const NetworkStatePropertiesPtr& network) override;

  const NetworkStatePropertiesPtr& last_network_list_item_selected() const {
    return last_network_list_item_selected_;
  }

 private:
  NetworkStatePropertiesPtr last_network_list_item_selected_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_VIEW_DELEGATE_H_
