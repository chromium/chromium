// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/style/switch.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Button;
class ImageView;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class HoverHighlightView;
class RoundedContainer;

// The implementation of BluetoothDetailedView.
class ASH_EXPORT BluetoothDetailedViewImpl : public BluetoothDetailedView,
                                             public TrayDetailedView {
  METADATA_HEADER(BluetoothDetailedViewImpl, TrayDetailedView)

 public:
  BluetoothDetailedViewImpl(DetailedViewDelegate* detailed_view_delegate,
                            BluetoothDetailedView::Delegate* delegate);
  BluetoothDetailedViewImpl(const BluetoothDetailedViewImpl&) = delete;
  BluetoothDetailedViewImpl& operator=(const BluetoothDetailedViewImpl&) =
      delete;
  ~BluetoothDetailedViewImpl() override;

  // BluetoothDetailedView:
  views::View* GetAsView() override;
  void UpdateBluetoothEnabledState(
      const bluetooth_config::mojom::BluetoothSystemState system_state)
      override;
  BluetoothDeviceListItemView* AddDeviceListItem() override;
  views::View* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                      int text_id) override;
  void NotifyDeviceListChanged() override;
  views::View* device_list() override;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

 private:
  friend class BluetoothDetailedViewImplTest;

  // Creates the top rounded container, which contains the main on/off toggle.
  void CreateTopContainer();

  // Creates the main rounded container, which holds the pair new device row
  // and the device list.
  void CreateMainContainer();

  // Attempts to close the quick settings and open the Bluetooth settings.
  void OnSettingsClicked();

  // Handles clicks on the Bluetooth toggle button.
  void OnToggleClicked();

  // Handles toggling Bluetooth via the UI to `new_state`.
  void ToggleBluetoothState(bool new_state);

  // Owned by views hierarchy.
  raw_ptr<views::Button> settings_button_ = nullptr;
  raw_ptr<RoundedContainer> top_container_ = nullptr;
  raw_ptr<HoverHighlightView> toggle_row_ = nullptr;
  raw_ptr<views::ImageView> toggle_icon_ = nullptr;
  raw_ptr<Switch> toggle_button_ = nullptr;
  raw_ptr<RoundedContainer> main_container_ = nullptr;
  raw_ptr<HoverHighlightView> pair_new_device_view_ = nullptr;
  raw_ptr<views::ImageView> pair_new_device_icon_ = nullptr;
  raw_ptr<views::View> device_list_ = nullptr;

  base::WeakPtrFactory<BluetoothDetailedViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
