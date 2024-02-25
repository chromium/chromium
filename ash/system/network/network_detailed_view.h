// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/system/network/network_info_bubble.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_tracker.h"

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
  METADATA_HEADER(NetworkDetailedView, TrayDetailedView)

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

  views::Button* info_button_for_testing() { return info_button_; }

 protected:
  NetworkDetailedView(DetailedViewDelegate* detailed_view_delegate,
                      Delegate* delegate,
                      NetworkDetailedViewListType list_type);

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
  void OnSettingsClicked();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // NetworkInfoBubble::Delegate:
  bool ShouldIncludeDeviceAddresses() override;
  void OnInfoBubbleDestroyed() override;

  int title_row_string_id_for_testing() { return title_row_string_id_; }

  // Type of list (all non-VPN netwoks, or only VPN networks).
  const NetworkDetailedViewListType list_type_;

  // Used to cache the login status on creation.
  const LoginStatus login_;

  // Used to track the existence of the `NetworkInfoBubble`
  views::ViewTracker info_bubble_tracker_;

  raw_ptr<TrayNetworkStateModel> model_;

  int title_row_string_id_;

  raw_ptr<views::Button> info_button_ = nullptr;
  raw_ptr<views::Button> settings_button_ = nullptr;

  raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<NetworkDetailedView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_H_
