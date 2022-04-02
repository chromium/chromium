// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_view.h"

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

  // Returns the implementation casted to views::View*. This may be |nullptr|
  // when testing, where the implementation might not inherit from views::View.
  virtual views::View* GetAsView() = 0;

  // TODO(b/207089013): Add AddNetworkListItem() when NetworkListNetworkItemView
  // is available, return NetworkListNetworkItemView*, and also add a function
  // that creates NetworkListNetworkHeaderView and returns view when
  // NetworkListNetworkHeaderView is available.

 protected:
  explicit NetworkDetailedNetworkView(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 private:
  Delegate* delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
