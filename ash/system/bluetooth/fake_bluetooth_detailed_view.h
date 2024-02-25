// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DETAILED_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class BluetoothDeviceListItemView;

// Fake BluetoothDetailedView implementation.
class ASH_EXPORT FakeBluetoothDetailedView : public BluetoothDetailedView,
                                             public ViewClickListener,
                                             public views::View {
 public:
  explicit FakeBluetoothDetailedView(Delegate* delegate);
  FakeBluetoothDetailedView(const FakeBluetoothDetailedView&) = delete;
  FakeBluetoothDetailedView& operator=(const FakeBluetoothDetailedView&) =
      delete;
  ~FakeBluetoothDetailedView() override;

  size_t notify_device_list_changed_call_count() const {
    return notify_device_list_changed_call_count_;
  }

  const std::optional<bool>& last_bluetooth_enabled_state() const {
    return last_bluetooth_enabled_state_;
  }

  const BluetoothDeviceListItemView* last_clicked_device_list_item() const {
    return last_clicked_device_list_item_;
  }

 private:
  // BluetoothDetailedView:
  views::View* GetAsView() override;
  void UpdateBluetoothEnabledState(
      const bluetooth_config::mojom::BluetoothSystemState system_state)
      override;
  BluetoothDeviceListItemView* AddDeviceListItem() override;
  views::View* AddDeviceListSubHeader(const gfx::VectorIcon& /*icon*/,
                                      int text_id) override;
  void NotifyDeviceListChanged() override;
  views::View* device_list() override;

  // ViewClickListener:
  void OnViewClicked(views::View* view) override;

  size_t notify_device_list_changed_call_count_ = 0;
  std::optional<bool> last_bluetooth_enabled_state_;
  std::unique_ptr<views::View> device_list_;
  raw_ptr<BluetoothDeviceListItemView, DanglingUntriaged>
      last_clicked_device_list_item_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_FAKE_BLUETOOTH_DETAILED_VIEW_H_
