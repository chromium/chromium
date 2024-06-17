// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using bluetooth_config::GetPairedDeviceName;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kBluetoothToastIdPrefix[] = "cros_bluetooth_device_toast_id-";

}  // namespace

BluetoothDeviceStatusUiHandler::BluetoothDeviceStatusUiHandler(
    PrefService* local_state)
    : local_state_(local_state) {
  // Asynchronously bind to CrosBluetoothConfig so that we don't want to attempt
  // to bind to it before it has initialized.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothDeviceStatusUiHandler::BindToCrosBluetoothConfig,
                     weak_ptr_factory_.GetWeakPtr()));
  // `local_state` may be null in tests.
  if (!local_state_) {
    return;
  }
  device::MaybeRecordConnectionToastShownCount(local_state_,
                                               /*triggered_by_connect=*/false);
}

BluetoothDeviceStatusUiHandler::~BluetoothDeviceStatusUiHandler() = default;

// static:
void BluetoothDeviceStatusUiHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kBluetoothConnectionToastShownCount, 0);
  registry->RegisterTimePref(prefs::kBluetoothToastCountStartTime,
                             base::Time::Now().LocalMidnight());
}

void BluetoothDeviceStatusUiHandler::OnDevicePaired(
    PairedBluetoothDevicePropertiesPtr device) {
  BLUETOOTH_LOG(EVENT) << "Notifying device was paired: "
                       << device->device_properties->id;
  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      ash::ToastCatalogName::kBluetoothDevicePaired,
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
          GetPairedDeviceName(device)));

  ShowToast(std::move(toast_data));
  device::RecordUiSurfaceDisplayed(device::BluetoothUiSurface::kPairedToast);
}

void BluetoothDeviceStatusUiHandler::OnDeviceDisconnected(
    PairedBluetoothDevicePropertiesPtr device) {
  BLUETOOTH_LOG(EVENT) << "Notifying device was disconnected: "
                       << device->device_properties->id;

  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      ash::ToastCatalogName::kBluetoothDeviceDisconnected,
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCONNECTED_TOAST,
          GetPairedDeviceName(device)));
  ShowToast(std::move(toast_data));
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kDisconnectedToast);
}

void BluetoothDeviceStatusUiHandler::OnDeviceConnected(
    PairedBluetoothDevicePropertiesPtr device) {
  BLUETOOTH_LOG(EVENT) << "Notifying device was connected: "
                       << device->device_properties->id;

  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      ash::ToastCatalogName::kBluetoothDeviceConnected,
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
          GetPairedDeviceName(device)));
  ShowToast(std::move(toast_data));
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kConnectionToast);
  device::MaybeRecordConnectionToastShownCount(local_state_,
                                               /*triggered_by_connect=*/true);

  if (last_connection_timestamp_.has_value()) {
    device::RecordTimeIntervalBetweenConnections(
        base::TimeTicks::Now() - last_connection_timestamp_.value());
  }
  last_connection_timestamp_ = base::TimeTicks::Now();

  if (auto* hats_bluetooth_revamp_trigger = HatsBluetoothRevampTrigger::Get()) {
    hats_bluetooth_revamp_trigger->TryToShowSurvey();
  }
}

void BluetoothDeviceStatusUiHandler::ShowToast(ash::ToastData toast_data) {
  ash::ToastManager::Get()->Show(std::move(toast_data));
}

std::string BluetoothDeviceStatusUiHandler::GetToastId(
    const bluetooth_config::mojom::PairedBluetoothDeviceProperties*
        paired_device_properties) {
  return kBluetoothToastIdPrefix +
         base::ToLowerASCII(paired_device_properties->device_properties->id);
}

void BluetoothDeviceStatusUiHandler::BindToCrosBluetoothConfig() {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveDeviceStatusChanges(
      cros_bluetooth_device_status_observer_receiver_
          .BindNewPipeAndPassRemote());
}

}  // namespace ash
