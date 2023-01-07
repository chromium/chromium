// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"
#include "ui/views/view.h"

namespace ash {

class DetailedViewDelegate;

// This class defines both the interface used to interact with the
// NetworkDetailedView and declares delegate interface it uses to propagate user
// interactions. It also defines the factory used to create instances of
// implementations of this class.
class ASH_EXPORT NetworkDetailedNetworkView {
 public:
  // This class defines the interface that NetworkDetailedNetworkView will use
  // to propagate user interactions.
  class Delegate : public NetworkDetailedView::Delegate {
   public:
    Delegate() = default;
    ~Delegate() override;

    virtual void OnWifiToggleClicked(bool new_state) = 0;
    virtual void OnMobileToggleClicked(bool new_state) = 0;
  };

  class Factory {
   public:
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    static std::unique_ptr<NetworkDetailedNetworkView> Create(
        DetailedViewDelegate* detailed_view_delegate,
        Delegate* delegate);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    Factory() = default;

    virtual std::unique_ptr<NetworkDetailedNetworkView> CreateForTesting(
        Delegate* delegate) = 0;
  };

  NetworkDetailedNetworkView(const NetworkDetailedNetworkView&) = delete;
  NetworkDetailedNetworkView& operator=(const NetworkDetailedNetworkView&) =
      delete;
  virtual ~NetworkDetailedNetworkView() = default;

  // Notifies that the network list has changed and the layout is invalid.
  virtual void NotifyNetworkListChanged() = 0;

  // Returns the implementation casted to views::View*. This may be |nullptr|
  // when testing, where the implementation might not inherit from views::View.
  virtual views::View* GetAsView() = 0;

  // Creates, adds and returns a new network list item. The client is
  // expected to use the returned pointer for removing and rearranging
  // the list item.
  virtual NetworkListNetworkItemView* AddNetworkListItem() = 0;

  // Creates, adds and returns a Wifi sticky sub-header to the end of the
  // network list. The client is expected to use the returned pointer for
  // removing and rearranging the sub-header.
  virtual NetworkListWifiHeaderView* AddWifiSectionHeader() = 0;

  // Creates, adds and returns a Mobile sticky sub-header to the end of the
  // network list. The client is expected to use the returned pointer for
  // removing and rearranging the sub-header.
  virtual NetworkListMobileHeaderView* AddMobileSectionHeader() = 0;

  // Updates the scanning bar visibility.
  virtual void UpdateScanningBarVisibility(bool visible) = 0;

  // Returns the network list.
  virtual views::View* network_list() = 0;

 protected:
  explicit NetworkDetailedNetworkView(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 private:
  Delegate* delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
