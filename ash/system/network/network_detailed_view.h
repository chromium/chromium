// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/system/network/network_info_bubble.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace views {

class View;

}  // namespace views

namespace ash {

class TrayNetworkStateModel;
class DetailedViewDelegate;
class Button;

// This class defines both the interface used to interact with the detailed
// Network page within the quick settings, including the view responsible for
// containing the network list. This class includes the declaration for the
// delegate interface it uses to propagate user interactions.
class ASH_EXPORT NetworkDetailedView : public TrayDetailedView,
                                       public NetworkInfoBubble::Delegate {
 public:
  // This class defines the interface that NetworkDetailedView will use to
  // propagate user interactions.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnNetworkListItemSelected(
        const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
            network) = 0;
  };

  NetworkDetailedView(const NetworkDetailedView&) = delete;
  NetworkDetailedView& operator=(const NetworkDetailedView&) = delete;
  ~NetworkDetailedView() override;

 protected:
  enum ListType { LIST_TYPE_NETWORK, LIST_TYPE_VPN };

  NetworkDetailedView(DetailedViewDelegate* detailed_view_delegate,
                      Delegate* delegate,
                      ListType list_type);

  TrayNetworkStateModel* model() { return model_; }

  views::Button* settings_button() { return settings_button_; }

  Delegate* delegate() { return delegate_; }

 private:
  friend class NetworkDetailedViewTest;
  friend class NetworkDetailedNetworkViewTest;

  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class NetworkDetailedViewChildId {
    kInfoButton = 1,
    kSettingsButton = 2,
  };

  void OnInfoClicked();
  bool CloseInfoBubble();
  void OnSettingsClicked();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // NetworkInfoBubble::Delegate:
  bool ShouldIncludeDeviceAddresses() override;
  void OnInfoBubbleDestroyed() override;

  // Type of list (all non-VPN netwoks, or only VPN networks).
  const ListType list_type_;

  // Used to cache the login status on creation.
  const LoginStatus login_;

  TrayNetworkStateModel* model_;

  views::Button* info_button_ = nullptr;
  views::Button* settings_button_ = nullptr;

  // A small bubble for displaying network info.
  NetworkInfoBubble* info_bubble_ = nullptr;

  Delegate* delegate_;

  base::WeakPtrFactory<NetworkDetailedView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_
