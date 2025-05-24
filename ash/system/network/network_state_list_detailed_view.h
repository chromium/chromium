// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

#include <memory>
#include <string>

#include "ash/login_status.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Button;
}

namespace ash {
class NetworkStateListDetailedView;
class TrayNetworkStateModel;

bool CanNetworkConnect(
    chromeos::network_config::mojom::ConnectionStateType connection_state,
    chromeos::network_config::mojom::NetworkType type,
    chromeos::network_config::mojom::ActivationStateType activation_state,
    bool is_connectable,
    std::string sim_eid);

// A bubble which displays network info.
class NetworkStateListInfoBubble : public views::BubbleDialogDelegateView {
 public:
  NetworkStateListInfoBubble(views::View* anchor,
                             std::unique_ptr<views::View> content,
                             NetworkStateListDetailedView* detailed_view);
  NetworkStateListInfoBubble(const NetworkStateListInfoBubble&) = delete;
  NetworkStateListInfoBubble& operator=(const NetworkStateListInfoBubble&) =
      delete;
  ~NetworkStateListInfoBubble() override;

  void OnNetworkStateListDetailedViewIsDeleting();

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  // Not owned.
  raw_ptr<NetworkStateListDetailedView> detailed_view_;
};

// Exported for tests.
class ASH_EXPORT NetworkStateListDetailedView
    : public TrayDetailedView,
      public TrayNetworkStateObserver {
  METADATA_HEADER(NetworkStateListDetailedView, TrayDetailedView)

 public:
  NetworkStateListDetailedView(const NetworkStateListDetailedView&) = delete;
  NetworkStateListDetailedView& operator=(const NetworkStateListDetailedView&) =
      delete;

  ~NetworkStateListDetailedView() override;

  void Init();

  // Asks the info bubble to close, if it exists. Returns whether it existed.
  bool ResetInfoBubble();

  // Restores activation to this view's widget.
  void OnInfoBubbleDestroyed();

  void ToggleInfoBubbleForTesting();

 protected:
  NetworkStateListDetailedView(DetailedViewDelegate* delegate,
                               NetworkDetailedViewListType list_type,
                               LoginStatus login);

  // Refreshes the network list.
  virtual void UpdateNetworkList() = 0;

  // Checks whether |view| represents a network in the list. If yes, sets
  // |guid| to the network's guid and returns |true|. Otherwise,
  // leaves |guid| unchanged and returns |false|.
  virtual bool IsNetworkEntry(views::View* view, std::string* guid) const = 0;

  // Called when the network model changes or when a network icon changes.
  void Update();

  TrayNetworkStateModel* model() { return model_; }

 private:
  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // Implementation of 'HandleViewClicked' once networks are received.
  void HandleViewClickedImpl(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Update info and settings buttons in header.
  void UpdateHeaderButtons();

  // Update scanning progress bar.
  void UpdateScanningBar();

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  std::unique_ptr<views::View> CreateNetworkInfoView();

  // Scan and start timer to periodically request a network scan.
  void ScanAndStartTimer();

  // Request a network scan.
  void CallRequestScan();

  bool IsWifiEnabled();

  // Type of list (all networks or vpn)
  NetworkDetailedViewListType list_type_;

  // Track login state.
  LoginStatus login_;

  raw_ptr<TrayNetworkStateModel> model_;

  raw_ptr<views::Button> info_button_;
  raw_ptr<views::Button> settings_button_;

  // A small bubble for displaying network info.
  raw_ptr<NetworkStateListInfoBubble> info_bubble_;

  // Timer for starting and stopping network scans.
  base::RepeatingTimer network_scan_repeating_timer_;

  base::WeakPtrFactory<NetworkStateListDetailedView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
