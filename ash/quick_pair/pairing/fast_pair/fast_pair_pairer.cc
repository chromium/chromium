// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include <memory>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

std::string MessageTypeToString(
    ash::quick_pair::FastPairMessageType message_type) {
  switch (message_type) {
    case ash::quick_pair::FastPairMessageType::kKeyBasedPairingRequest:
      return "Key-Based Pairing Request";
    case ash::quick_pair::FastPairMessageType::kKeyBasedPairingResponse:
      return "Key-Based Pairing Response";
    case ash::quick_pair::FastPairMessageType::kSeekersPasskey:
      return "Seeker's Passkey";
    case ash::quick_pair::FastPairMessageType::kProvidersPasskey:
      return "Providers' Passkey";
  }
}

std::string GattErrorToString(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GATT_ERROR_UNKNOWN:
      return "[GATT_ERROR_UNKNOWN]";
    case device::BluetoothGattService::GATT_ERROR_FAILED:
      return "[GATT_ERROR_FAILED]";
    case device::BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      return "[GATT_ERROR_IN_PROGRESS]";
    case device::BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return "[GATT_ERROR_INVALID_LENGTH]";
    case device::BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return "[GATT_ERROR_NOT_PERMITTED]";
    case device::BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return "[GATT_ERROR_NOT_AUTHORIZED]";
    case device::BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      return "[GATT_ERROR_NOT_PAIRED]";
    case device::BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return "[GATT_ERROR_NOT_SUPPORTED]";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairPairer::FastPairPairer(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete)
    : adapter_(std::move(adapter)),
      device_(std::move(device)),
      paired_callback_(std::move(paired_callback)),
      pair_failed_callback_(std::move(pair_failed_callback)),
      account_key_failure_callback_(std::move(account_key_failure_callback)),
      pairing_procedure_complete_(std::move(pairing_procedure_complete)) {
  adapter_observation_.Observe(adapter_.get());

  absl::optional<std::vector<uint8_t>> additional_data =
      device_->GetAdditionalData(Device::AdditionalDataType::kFastPairVersion);

  // If this is a v1 pairing, we pass off the responsibility to the Bluetooth
  // pairing dialog, and will listen for the
  // BluetoothAdapter::Observer::DevicePairedChanged event before firing the
  // |paired_callback|.
  if (additional_data.has_value() && additional_data->size() == 1 &&
      (*additional_data)[0] == 1) {
    Shell::Get()->system_tray_model()->client()->ShowBluetoothPairingDialog(
        device_->ble_address);
    return;
  }

  fast_pair_handshake_ = FastPairHandshakeLookup::GetInstance()->Get(device_);

  if (!fast_pair_handshake_) {
    QP_LOG(INFO) << __func__
                 << ": Failed to find handshake. This is only valid if we "
                    "lost the device before this class executed.";
    return;
  }

  DCHECK(fast_pair_handshake_->completed_successfully());

  std::string device_address = device_->classic_address().value();
  device::BluetoothDevice* bt_device = adapter_->GetDevice(device_address);

  switch (device_->protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      // Now that we have validated the decrypted response, we can attempt to
      // retrieve the device from the adapter by the address. If we are able
      // to retrieve the device in this way, we can pair directly. Often, we
      // will not be able to find the device this way, and we will have to
      // connect via address and add ourselves as a pairing delegate.

      QP_LOG(VERBOSE) << "Key-based pairing changed. Address: "
                      << device_address << ". Found device: "
                      << ((device != nullptr) ? "Yes" : "No") << ".";

      if (bt_device) {
        bt_device->Pair(this, base::BindOnce(&FastPairPairer::OnPairConnected,
                                             weak_ptr_factory_.GetWeakPtr()));
      } else {
        adapter_->AddPairingDelegate(
            this, device::BluetoothAdapter::PairingDelegatePriority::
                      PAIRING_DELEGATE_PRIORITY_HIGH);

        adapter_->ConnectDevice(device_address, /*address_type=*/absl::nullopt,
                                base::BindOnce(&FastPairPairer::OnConnectDevice,
                                               weak_ptr_factory_.GetWeakPtr()),
                                base::BindOnce(&FastPairPairer::OnConnectError,
                                               weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case Protocol::kFastPairRetroactive:
      // Because the devices are already bonded, BR/EDR bonding and
      // Passkey verification will be skipped and we will directly write an
      // account key to the Provider after a shared secret is established.
      adapter_->RemovePairingDelegate(this);
      SendAccountKey();
      break;
  }
}

FastPairPairer::~FastPairPairer() {
  adapter_->RemovePairingDelegate(this);
}

void FastPairPairer::OnPairConnected(
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error) {
  if (error) {
    QP_LOG(WARNING) << "Failed to starting pairing procedure by pairing to "
                       "device due to error: "
                    << error.value();
    std::move(pair_failed_callback_).Run(device_, PairFailure::kPairingConnect);
    return;
  }
  QP_LOG(VERBOSE) << "Pair to device successful.";
}

void FastPairPairer::OnConnectDevice(device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << "Connect device successful.";
}

void FastPairPairer::OnConnectError() {
  QP_LOG(WARNING) << "Failed to starting pairing procedure by connecting to "
                     "device address.";
  std::move(pair_failed_callback_).Run(device_, PairFailure::kAddressConnect);
}

void FastPairPairer::ConfirmPasskey(device::BluetoothDevice* device,
                                    uint32_t passkey) {
  pairing_device_address_ = device->GetAddress();
  expected_passkey_ = passkey;
  fast_pair_handshake_->fast_pair_gatt_service_client()->WritePasskeyAsync(
      /*message_type=*/0x02, /*passkey=*/expected_passkey_,
      fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairer::OnPasskeyResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairer::OnPasskeyResponse(std::vector<uint8_t> response_bytes,
                                       absl::optional<PairFailure> failure) {
  if (failure) {
    std::move(pair_failed_callback_).Run(device_, failure.value());
    return;
  }

  fast_pair_handshake_->fast_pair_data_encryptor()->ParseDecryptedPasskey(
      response_bytes, base::BindOnce(&FastPairPairer::OnParseDecryptedPasskey,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairer::OnParseDecryptedPasskey(
    const absl::optional<DecryptedPasskey>& passkey) {
  if (!passkey) {
    QP_LOG(WARNING) << "Missing decrypted passkey from parse.";
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyDecryptFailure);
    return;
  }

  if (passkey->message_type != FastPairMessageType::kProvidersPasskey) {
    QP_LOG(WARNING)
        << "Incorrect message type from decrypted passkey. Expected: "
        << MessageTypeToString(FastPairMessageType::kProvidersPasskey)
        << ". Actual: " << MessageTypeToString(passkey->message_type);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kIncorrectPasskeyResponseType);
    return;
  }

  if (passkey->passkey != expected_passkey_) {
    QP_LOG(ERROR) << "Passkeys do not match. Expected: " << expected_passkey_
                  << ". Actual: " << passkey->passkey;
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyMismatch);
    return;
  }

  device::BluetoothDevice* pairing_device =
      adapter_->GetDevice(pairing_device_address_);

  if (!pairing_device) {
    QP_LOG(ERROR) << "Bluetooth pairing device lost during write to passkey.";
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    return;
  }

  pairing_device->ConfirmPairing();
  std::move(paired_callback_).Run(device_);
  adapter_->RemovePairingDelegate(this);
  SendAccountKey();
}

void FastPairPairer::SendAccountKey() {
  // We only send the account key if we're doing an initial or retroactive
  // pairing.
  if (device_->protocol != Protocol::kFastPairInitial &&
      device_->protocol != Protocol::kFastPairRetroactive) {
    return;
  }

  std::array<uint8_t, 16> account_key;
  RAND_bytes(account_key.data(), account_key.size());
  account_key[0] = 0x04;

  fast_pair_handshake_->fast_pair_gatt_service_client()->WriteAccountKey(
      account_key, fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairer::OnWriteAccountKey,
                     weak_ptr_factory_.GetWeakPtr(), account_key));
}

void FastPairPairer::OnWriteAccountKey(
    std::array<uint8_t, 16> account_key,
    absl::optional<device::BluetoothGattService::GattErrorCode> error) {
  if (error) {
    QP_LOG(WARNING)
        << "Failed to write account key to device due to Gatt Error: "
        << GattErrorToString(error.value());
    std::move(account_key_failure_callback_)
        .Run(device_, AccountKeyFailure::kAccountKeyCharacteristicWrite);
    return;
  }

  FastPairRepository::Get()->AssociateAccountKey(
      device_, std::vector<uint8_t>(account_key.begin(), account_key.end()));

  QP_LOG(INFO) << "Account key written to device. Pairing procedure complete.";
  std::move(pairing_procedure_complete_).Run(device_);
}

void FastPairPairer::RequestPinCode(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairer::RequestPasskey(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairer::DisplayPinCode(device::BluetoothDevice* device,
                                    const std::string& pincode) {
  NOTREACHED();
}

void FastPairPairer::DisplayPasskey(device::BluetoothDevice* device,
                                    uint32_t passkey) {
  NOTREACHED();
}

void FastPairPairer::KeysEntered(device::BluetoothDevice* device,
                                 uint32_t entered) {
  NOTREACHED();
}

void FastPairPairer::AuthorizePairing(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairer::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                         device::BluetoothDevice* device,
                                         bool new_paired_status) {
  if (!new_paired_status || !paired_callback_)
    return;

  // This covers the case where we are pairing a v1 device and are using the
  // Bluetooth pairing dialog to do it.
  if (device->GetAddress() == device_->ble_address ||
      device->GetAddress() == device_->classic_address()) {
    std::move(paired_callback_).Run(device_);
  }
}

}  // namespace quick_pair
}  // namespace ash
