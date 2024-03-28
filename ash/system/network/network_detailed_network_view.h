// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_tether_hosts_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

namespace {
using chromeos::network_config::mojom::NetworkType;
}  // namespace

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
  virtual NetworkListNetworkItemView* AddNetworkListItem(NetworkType type) = 0;

  // Creates, adds and returns a `HoverHighlightView`, which is the "Join Wifi
  // network" entry for the Wifi section if `NetworkType::kWiFi` is passed in or
  // the "Add eSIM" entry for the Mobile data section if `NetworkType::kMobile`
  // is passed in. The client is expected to use the returned pointer for
  // removing and rearranging this entry.
  virtual HoverHighlightView* AddConfigureNetworkEntry(NetworkType type) = 0;

  // Creates, adds and returns a Wifi sticky sub-header to the end of the
  // network list. The client is expected to use the returned pointer for
  // removing and rearranging the sub-header.
  virtual NetworkListWifiHeaderView* AddWifiSectionHeader() = 0;

  // Creates, adds and returns a Mobile sticky sub-header to the end of the
  // network list. The client is expected to use the returned pointer for
  // removing and rearranging the sub-header.
  virtual NetworkListMobileHeaderView* AddMobileSectionHeader() = 0;

  // Creates, adds and returns a Tether Hosts sticky sub-header to the end
  // of the network list. The client is expected to use the returned pointer
  // for removing and rearranging the sub-header.
  virtual NetworkListTetherHostsHeaderView* AddTetherHostsSectionHeader(
      NetworkListTetherHostsHeaderView::OnExpandedStateToggle callback) = 0;

  // Updates the scanning bar visibility.
  virtual void UpdateScanningBarVisibility(bool visible) = 0;

  // Returns the network list.
  virtual views::View* GetNetworkList(NetworkType type) = 0;

  // Reorders the container or list view based on the index.
  virtual void ReorderFirstListView(size_t index) = 0;
  virtual void ReorderNetworkTopContainer(size_t index) = 0;
  virtual void ReorderNetworkListView(size_t index) = 0;
  virtual void ReorderMobileTopContainer(size_t index) = 0;
  virtual void ReorderMobileListView(size_t index) = 0;
  virtual void ReorderTetherHostsListView(size_t index) = 0;

  // Removes the first list view if there's no child views in it.
  virtual void MaybeRemoveFirstListView() = 0;

  // Updates the containers, shows or hides the corresponding list view.
  virtual void UpdateWifiStatus(bool enabled) = 0;
  virtual void UpdateMobileStatus(bool enabled) = 0;
  virtual void UpdateTetherHostsStatus(bool enabled) = 0;

  // Provides some virtual methods to get and set the scroll view's position
  // before and after reordering the network list.
  virtual void ScrollToPosition(int position) {}
  virtual int GetScrollPosition();

 protected:
  explicit NetworkDetailedNetworkView(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 private:
  raw_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
