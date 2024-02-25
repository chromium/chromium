// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class ASH_EXPORT DisableBluetoothDialogControllerImpl
    : public DisableBluetoothDialogController,
      public views::WidgetObserver {
 public:
  DisableBluetoothDialogControllerImpl();

  DisableBluetoothDialogControllerImpl(
      const DisableBluetoothDialogControllerImpl&) = delete;
  DisableBluetoothDialogControllerImpl& operator=(
      const DisableBluetoothDialogControllerImpl&) = delete;

  ~DisableBluetoothDialogControllerImpl() override;

  // DisableBluetoothDialogController:
  void ShowDialog(const DeviceNamesList& devices,
                  ShowDialogCallback callback) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class DisableBluetoothDialogControllerTest;

  void OnConfirmDialog();
  void OnCancelDialog();

  const SystemDialogDelegateView* GetSystemDialogViewForTesting() const;

  DisableBluetoothDialogController::ShowDialogCallback show_dialog_callback_;

  // Pointer to the widget (if any) that contains the current dialog.
  raw_ptr<views::Widget> dialog_widget_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};

  base::WeakPtrFactory<DisableBluetoothDialogControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_IMPL_H_
