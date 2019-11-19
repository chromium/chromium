// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_SECTION_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_SECTION_HEADER_VIEW_H_

#include "ash/system/network/network_row_title_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

class TrayNetworkStateModel;

namespace tray {

// A header row for sections in network detailed view which contains a title and
// a toggle button to turn on/off the section. Subclasses are given the
// opportunity to add extra buttons before the toggle button is added.
class NetworkSectionHeaderView : public views::View,
                                 public views::ButtonListener {
 public:
  explicit NetworkSectionHeaderView(int title_id);
  ~NetworkSectionHeaderView() override = default;

  // Modify visibility of section toggle
  void SetToggleVisibility(bool visible);

  // Modify enabled/disabled and on/off state of toggle.
  virtual void SetToggleState(bool toggle_enabled, bool is_on);

  // views::View:
  const char* GetClassName() const override;

 protected:
  void Init(bool enabled);

  // This is called before the toggle button is added to give subclasses an
  // opportunity to add more buttons before the toggle button. Subclasses can
  // add buttons to container() using AddChildView().
  virtual void AddExtraButtons(bool enabled);

  // Called when |toggle_| is clicked and toggled. Subclasses can override to
  // enabled/disable their respective technology, for example.
  virtual void OnToggleToggled(bool is_on) = 0;

  TrayNetworkStateModel* model() { return model_; }
  TriView* container() const { return container_; }

  // views::View:
  int GetHeightForWidth(int width) const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void InitializeLayout();
  void AddToggleButton(bool enabled);

  // Resource ID for the string to use as the title of the section and for the
  // accessible text on the section header toggle button.
  const int title_id_;

  TrayNetworkStateModel* model_;

  // View containing header row views, including title, toggle, and extra
  // buttons.
  TriView* container_ = nullptr;

  // View containing the header row view. Is a child of the CENTER of
  // |container_|.
  NetworkRowTitleView* network_row_title_view_ = nullptr;

  // ToggleButton to toggle section on or off.
  views::ToggleButton* toggle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NetworkSectionHeaderView);
};

// "Mobile Data" header row. Mobile Data reflects both Cellular state and
// Tether state. When both technologies are available, Cellular state takes
// precedence over Tether (but in some cases Tether state may be shown).
class MobileSectionHeaderView : public NetworkSectionHeaderView {
 public:
  MobileSectionHeaderView();
  ~MobileSectionHeaderView() override;

  // Updates mobile toggle state and returns the id of the status message
  // that should be shown while connecting to a network. Returns zero when no
  // message should be shown.
  int UpdateToggleAndGetStatusMessage(bool mobile_has_networks,
                                      bool tether_has_networks);

  // views::View:
  const char* GetClassName() const override;

 private:
  // NetworkListView::NetworkSectionHeaderView:
  void OnToggleToggled(bool is_on) override;

  // When Tether is disabled because Bluetooth is off, then enabling Bluetooth
  // will enable Tether. If enabling Bluetooth takes longer than some timeout
  // period, it is assumed that there was an error. In that case, Tether will
  // remain uninitialized and Mobile Data will remain toggled off.
  void EnableBluetooth();
  void OnEnableBluetoothTimeout();

  bool waiting_for_tether_initialize_ = false;
  base::OneShotTimer enable_bluetooth_timer_;

  base::WeakPtrFactory<MobileSectionHeaderView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MobileSectionHeaderView);
};

class WifiSectionHeaderView : public NetworkSectionHeaderView {
 public:
  WifiSectionHeaderView();
  ~WifiSectionHeaderView() override = default;

  // NetworkSectionHeaderView:
  void SetToggleState(bool toggle_enabled, bool is_on) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // NetworkSectionHeaderView:
  void OnToggleToggled(bool is_on) override;
  void AddExtraButtons(bool enabled) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // A button to invoke "Join Wi-Fi network" dialog.
  views::Button* join_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WifiSectionHeaderView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_SECTION_HEADER__VIEW_H_
