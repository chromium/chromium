// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/devicetype.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller.h"
#include "chromeos/constants/devicetype.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kUnknownDeviceTypeName[] = "ChromeOS device";

// The dialog that is shown should have a maximum of three devices listed.
constexpr size_t kMaxDeviceListLength = 3;

}  // namespace

DisableBluetoothDialogControllerImpl::DisableBluetoothDialogControllerImpl() {
  CHECK(features::IsBluetoothDisconnectWarningEnabled());
}

DisableBluetoothDialogControllerImpl::~DisableBluetoothDialogControllerImpl() {
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    dialog_widget_->CloseNow();
  }
}

void DisableBluetoothDialogControllerImpl::ShowDialog(
    const DeviceNamesList& devices,
    DisableBluetoothDialogController::ShowDialogCallback callback) {
  show_dialog_callback_ = std::move(callback);

  // We should not get here with an active dialog.
  DCHECK_EQ(dialog_widget_, nullptr);
  CHECK(!devices.empty());

  std::string device_type = DeviceTypeToString(chromeos::GetDeviceType());
  if (device_type.empty()) {
    device_type = kUnknownDeviceTypeName;
  }

  std::u16string dialog_description;

  if (devices.size() == 1) {
    dialog_description = l10n_util::GetStringFUTF16(
        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_DESCRIPTION_ONE_DEVICE,
        base::UTF8ToUTF16(device_type));
  } else if (devices.size() == 2) {
    dialog_description = l10n_util::GetStringFUTF16(
        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_DESCRIPTION_TWO_DEVICES,
        base::UTF8ToUTF16(device_type));
  } else {
    dialog_description = l10n_util::GetStringFUTF16(
        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_DESCRIPTION_MULTIPLE_DEVICES,
        base::NumberToString16(devices.size()), base::UTF8ToUTF16(device_type));
  }

  auto dialog = views::Builder<SystemDialogDelegateView>()
                    .SetTitleText(l10n_util::GetStringUTF16(
                        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_TITLE))
                    .SetDescription(dialog_description)
                    .SetAcceptButtonText(l10n_util::GetStringUTF16(
                        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_TURN_OFF))
                    .SetCancelButtonText(l10n_util::GetStringUTF16(
                        IDS_ASH_DISCONNECT_BLUETOOTH_WARNING_DIALOG_KEEP_ON))
                    .SetCancelCallback(base::BindOnce(
                        &DisableBluetoothDialogControllerImpl::OnCancelDialog,
                        weak_ptr_factory_.GetWeakPtr()))
                    .SetAcceptCallback(base::BindOnce(
                        &DisableBluetoothDialogControllerImpl::OnConfirmDialog,
                        weak_ptr_factory_.GetWeakPtr()))
                    .Build();

  dialog->SetProperty(views::kElementIdentifierKey,
                      kWarnBeforeDisconnectingBluetoothDialogElementId);

  std::vector<std::u16string> texts;
  const size_t count = std::min(devices.size(), kMaxDeviceListLength);
  for (size_t i = 0; i < count; i++) {
    texts.push_back(base::UTF8ToUTF16(devices[i]));
  }

  // Create the BulletedLabelListView with the generated texts.
  std::unique_ptr<views::BulletedLabelListView> list_view =
      std::make_unique<views::BulletedLabelListView>(
          std::move(texts), views::style::TextStyle::STYLE_SECONDARY);

  dialog->SetModalType(ui::mojom::ModalType::kSystem);
  dialog->SetShowCloseButton(false);
  dialog->SetMiddleContentView(std::move(list_view));
  dialog->SetMiddleContentAlignment(views::LayoutAlignment::kStart);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = Shell::GetPrimaryRootWindow();
  params.delegate = dialog.release();
  dialog_widget_ = new views::Widget();
  dialog_widget_->Init(std::move(params));

  dialog_widget_->Show();
  dialog_widget_observation_.Observe(dialog_widget_.get());

  // Ensure that if ChromeVox is enabled, it focuses on the dialog.
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (accessibility_controller->spoken_feedback().enabled()) {
    accessibility_controller->SetA11yOverrideWindow(
        dialog_widget_->GetNativeWindow());
  }
}

void DisableBluetoothDialogControllerImpl::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
}

void DisableBluetoothDialogControllerImpl::OnConfirmDialog() {
  CHECK(show_dialog_callback_);
  std::move(show_dialog_callback_).Run(true);
}

void DisableBluetoothDialogControllerImpl::OnCancelDialog() {
  CHECK(show_dialog_callback_);
  std::move(show_dialog_callback_).Run(false);
}

const SystemDialogDelegateView*
DisableBluetoothDialogControllerImpl::GetSystemDialogViewForTesting() const {
  CHECK(dialog_widget_);
  return static_cast<SystemDialogDelegateView*>(
      dialog_widget_->GetContentsView());
}

}  // namespace ash
