// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/weak_ptr.h"

namespace views {
class Button;
class ToggleButton;
class View;
}  // namespace views

namespace ash {

class BluetoothDeviceListItemView;
class BluetoothDisabledDetailedView;
class DetailedViewDelegate;
class TriView;

// BluetoothDetailedView implementation.
class ASH_EXPORT BluetoothDetailedViewImpl : public BluetoothDetailedView,
                                             public TrayDetailedView {
 public:
  BluetoothDetailedViewImpl(DetailedViewDelegate* detailed_view_delegate,
                            BluetoothDetailedView::Delegate* delegate);
  BluetoothDetailedViewImpl(const BluetoothDetailedViewImpl&) = delete;
  BluetoothDetailedViewImpl& operator=(const BluetoothDetailedViewImpl&) =
      delete;
  ~BluetoothDetailedViewImpl() override;

 private:
  friend class BluetoothDetailedViewTest;

  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class BluetoothDetailedViewChildId {
    kToggleButton = 1,
    kDisabledView = 2,
    kSettingsButton = 3,
    kPairNewDeviceView = 4,
    kPairNewDeviceClickableView = 5,
  };

  // BluetoothDetailedView:
  views::View* GetAsView() override;
  void UpdateBluetoothEnabledState(bool enabled) override;
  BluetoothDeviceListItemView* AddDeviceListItem() override;
  ash::TriView* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                       int text_id) override;
  void NotifyDeviceListChanged() override;
  views::View* device_list() override;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // views::View:
  const char* GetClassName() const override;

  // Creates and configures the Bluetooth disabled view.
  void CreateDisabledView();

  // Creates and configures the pair new device view containing the button and
  // the following separator line.
  void CreatePairNewDeviceView();

  // Creates and configures the Bluetooth toggle button and the settings button.
  void CreateTitleRowButtons();

  // Attempts to close the quick settings and open the Bluetooth settings.
  void OnSettingsClicked();

  // Propagates user interaction with the Bluetooth toggle button.
  void OnToggleClicked();

  views::Button* settings_button_ = nullptr;
  views::ToggleButton* toggle_button_ = nullptr;
  views::View* pair_new_device_view_ = nullptr;
  BluetoothDisabledDetailedView* disabled_view_ = nullptr;

  base::WeakPtrFactory<BluetoothDetailedViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
