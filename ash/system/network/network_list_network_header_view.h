// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/switch.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

namespace ash {

class TrayNetworkStateModel;

// This class is used for the headers of networks (Mobile and Wifi), and
// implements the functionality shared between the headers of Mobile and Wifi
// network headers.
class ASH_EXPORT NetworkListNetworkHeaderView : public NetworkListHeaderView {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnMobileToggleClicked(bool new_state) = 0;
    virtual void OnWifiToggleClicked(bool new_state) = 0;
  };

  NetworkListNetworkHeaderView(Delegate* delegate,
                               int label_id,
                               const gfx::VectorIcon& vector_icon);
  NetworkListNetworkHeaderView(const NetworkListNetworkHeaderView&) = delete;
  NetworkListNetworkHeaderView& operator=(const NetworkListNetworkHeaderView&) =
      delete;
  ~NetworkListNetworkHeaderView() override;

  virtual void SetToggleState(bool enabled, bool is_on, bool animate_toggle);

  void SetToggleVisibility(bool visible);

 protected:
  virtual void AddExtraButtons();

  // Called when `toggle_` is clicked and toggled. Subclasses should override to
  // enabled/disable their respective technology.
  virtual void OnToggleToggled(bool is_on);

  Delegate* delegate() const { return delegate_; }

  TrayNetworkStateModel* model() { return model_; }

  // Used for testing.
  static constexpr int kToggleButtonId =
      NetworkListHeaderView::kTitleLabelViewId + 1;
  static constexpr int kQsToggleButtonId = kToggleButtonId + 1;

 private:
  friend class NetworkListNetworkHeaderViewTest;
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListWifiHeaderViewTest;
  friend class NetworkListViewControllerTest;

  void ToggleButtonPressed();

  // NetworkListHeaderView:
  void UpdateToggleState(bool has_new_state) override;

  raw_ptr<TrayNetworkStateModel, ExperimentalAsh> model_;
  int const enabled_label_id_;

  // `ToggleButton` to toggle section on or off.
  raw_ptr<views::ToggleButton, ExperimentalAsh> toggle_ = nullptr;
  // `KnobSwitch` to toggle section on or off.
  raw_ptr<Switch, ExperimentalAsh> qs_toggle_ = nullptr;

  raw_ptr<Delegate, ExperimentalAsh> delegate_ = nullptr;

  base::WeakPtrFactory<NetworkListNetworkHeaderView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_
