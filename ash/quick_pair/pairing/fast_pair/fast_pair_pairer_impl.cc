// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"

#include <memory>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

// 15s timeout chosen to align with Android's Fast Pair implementation.
constexpr base::TimeDelta kCreateBondTimeout = base::Seconds(15);

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

bool ShouldBeEnabledForLoginStatus(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairPairerImpl::Factory* FastPairPairerImpl::Factory::g_test_factory_ =
    nullptr;

// static
std::unique_ptr<FastPairPairer> FastPairPairerImpl::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    base::OnceCallback<void(scoped_refptr<Device>)> handshake_complete_callback,
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(scoped_refptr<Device>)>
        pairing_procedure_complete) {
  if (g_test_factory_)
    return g_test_factory_->CreateInstance(
        std::move(adapter), std::move(device),
        std::move(handshake_complete_callback), std::move(paired_callback),
        std::move(pair_failed_callback),
        std::move(account_key_failure_callback),
        std::move(pairing_procedure_complete));

  return base::WrapUnique(new FastPairPairerImpl(
      std::move(adapter), std::move(device),
      std::move(handshake_complete_callback), std::move(paired_callback),
      std::move(pair_failed_callback), std::move(account_key_failure_callback),
      std::move(pairing_procedure_complete)));
}

// static
void FastPairPairerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPairerImpl::Factory::~Factory() = default;

FastPairPairerImpl::FastPairPairerImpl(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    base::OnceCallback<void(scoped_refptr<Device>)> handshake_complete_callback,
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete)
    : adapter_(std::move(adapter)),
      device_(std::move(device)),
      handshake_complete_callback_(std::move(handshake_complete_callback)),
      paired_callback_(std::move(paired_callback)),
      pair_failed_callback_(std::move(pair_failed_callback)),
      account_key_failure_callback_(std::move(account_key_failure_callback)),
      pairing_procedure_complete_(std::move(pairing_procedure_complete)) {
  adapter_observation_.Observe(adapter_.get());

  // If this is a v1 pairing, we pass off the responsibility to the Bluetooth
  // pairing dialog, and will listen for the
  // BluetoothAdapter::Observer::DevicePairedChanged event before firing the
  // |paired_callback|. V1 devices only support the "initial pairing" protocol,
  // not the "retroactive" or "subsequent" pairing protocols, so only
  // "initial pairing" metrics are emitted to here.
  if (device_->version().value() == DeviceFastPairVersion::kV1) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kV1DeviceDetected);
    RecordFastPairInitializePairingProcessEvent(
        *device_,
        FastPairInitializePairingProcessEvent::kPassedToPairingDialog);
    Shell::Get()->system_tray_model()->client()->ShowBluetoothPairingDialog(
        device_->ble_address);
    return;
  }

  fast_pair_handshake_ = FastPairHandshakeLookup::GetInstance()->Get(device_);

  if (fast_pair_handshake_) {
    // Handle cases where we are retrying pair after a non-handshake related
    // error occurs.
    if (fast_pair_handshake_->completed_successfully()) {
      QP_LOG(VERBOSE) << __func__
                      << ": Reusing handshake for retried pair attempt.";
      RecordFastPairInitializePairingProcessEvent(
          *device_, FastPairInitializePairingProcessEvent::kHandshakeReused);
      OnHandshakeComplete(device_, /*failure=*/absl::nullopt);
      return;
    }

    // Handles cases where we are retrying pair after an error occurred when
    // creating the handshake.
    QP_LOG(VERBOSE) << __func__
                    << ": Clearing failed handshake for retried pair attempt.";
    FastPairHandshakeLookup::GetInstance()->Erase(device_);
    fast_pair_handshake_ = nullptr;
  }

  QP_LOG(VERBOSE) << __func__ << ": Creating new handshake for pair attempt.";
  FastPairHandshakeLookup::GetInstance()->Create(
      adapter_, device_,
      base::BindOnce(&FastPairPairerImpl::OnHandshakeComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnHandshakeComplete(
    scoped_refptr<Device> device,
    absl::optional<PairFailure> failure) {
  // TODO(b/259429032) : Log with `RecordInitializationRetriesBeforeSuccess`
  // the number of handshake retries occurred before success. Log with
  // `FastPairInitializePairingProcessEvent` if we have exhausted the retries.

  if (failure.has_value()) {
    QP_LOG(WARNING) << __func__ << ": Handshake failed with " << device
                    << " because: " << failure.value();
    RecordInitializationFailureReason(*device, failure.value());
    std::move(pair_failed_callback_).Run(device_, failure.value());
    // |this| may be destroyed after this line.
    return;
  }

  // During handshake, the device address can be set to null.
  if (!device_->classic_address()) {
    QP_LOG(WARNING) << __func__ << ": Device lost during handshake.";
    RecordInitializationFailureReason(*device, PairFailure::kPairingDeviceLost);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    // |this| may be destroyed after this line.
    return;
  }

  fast_pair_handshake_ = FastPairHandshakeLookup::GetInstance()->Get(device_);

  DCHECK(fast_pair_handshake_);
  DCHECK(fast_pair_handshake_->completed_successfully());

  std::move(handshake_complete_callback_).Run(device_);

  fast_pair_gatt_service_client_ =
      fast_pair_handshake_->fast_pair_gatt_service_client();

  // If we have a valid handshake, we already have a GATT connection that we
  // maintain in order to prevent addresses changing for some devices when the
  // connection ends.
  StartPairing();
}

FastPairPairerImpl::~FastPairPairerImpl() {
  adapter_->RemovePairingDelegate(this);
}

void FastPairPairerImpl::StartPairing() {
  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPairingStarted,
                            *device_);

  std::string device_address = device_->classic_address().value();
  device::BluetoothDevice* bt_device = adapter_->GetDevice(device_address);
  switch (device_->protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      // Now that we have validated the decrypted response, we can attempt to
      // retrieve the device from the adapter by the address. If we are able
      // to get the device, and it's not already paired, we can pair directly.
      // Often, we will not be able to find the device this way, and we will
      // have to connect via address and add ourselves as a pairing delegate.
      QP_LOG(VERBOSE) << "Sending pair request to device. Address: "
                      << device_address << ". Found device: "
                      << ((bt_device != nullptr) ? "Yes" : "No") << ".";

      if (bt_device && bt_device->IsBonded()) {
        QP_LOG(INFO) << __func__
                     << ": Trying to pair to device that is already paired; "
                        "returning success.";
        RecordProtocolPairingStep(FastPairProtocolPairingSteps::kAlreadyPaired,
                                  *device_);
        RecordProtocolPairingStep(
            FastPairProtocolPairingSteps::kPairingComplete, *device_);
        AttemptRecordingFastPairEngagementFlow(
            *device_,
            FastPairEngagementFlowEvent::kPairingSucceededAlreadyPaired);

        std::move(paired_callback_).Run(device_);
        AttemptSendAccountKey();
        return;
      }

      create_bond_timeout_timer_.Start(
          FROM_HERE, kCreateBondTimeout,
          base::BindOnce(&FastPairPairerImpl::OnCreateBondTimeout,
                         base::Unretained(this)));

      if (bt_device) {
        bt_device->Pair(this,
                        base::BindOnce(&FastPairPairerImpl::OnPairConnected,
                                       weak_ptr_factory_.GetWeakPtr()));
      } else {
        adapter_->AddPairingDelegate(
            this, device::BluetoothAdapter::PairingDelegatePriority::
                      PAIRING_DELEGATE_PRIORITY_HIGH);
        adapter_->ConnectDevice(
            device_address,
            /*address_type=*/absl::nullopt,
            base::BindOnce(&FastPairPairerImpl::OnConnectDevice,
                           weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(&FastPairPairerImpl::OnConnectError,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case Protocol::kFastPairRetroactive:
      // Because the devices are already bonded, BR/EDR bonding and
      // Passkey verification will be skipped and we will directly write an
      // account key to the Provider after a shared secret is established.
      adapter_->RemovePairingDelegate(this);
      AttemptSendAccountKey();
      break;
  }
}

void FastPairPairerImpl::OnPairConnected(
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error) {
  if (!StopCreateBondTimer(__func__))
    return;

  QP_LOG(INFO) << __func__;
  RecordPairDeviceResult(/*success=*/!error.has_value());

  if (error) {
    QP_LOG(WARNING) << "Failed to start pairing procedure by pairing to "
                       "device due to error: "
                    << error.value();
    std::move(pair_failed_callback_).Run(device_, PairFailure::kPairingConnect);
    // |this| may be destroyed after this line.
    RecordPairDeviceErrorReason(error.value());
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kBondSuccessful,
                            *device_);
  ask_confirm_passkey_initial_time_ = base::TimeTicks::Now();
}

void FastPairPairerImpl::OnConnectDevice(device::BluetoothDevice* device) {
  if (!StopCreateBondTimer(__func__))
    return;

  QP_LOG(INFO) << __func__;
  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kBondSuccessful,
                            *device_);
  ask_confirm_passkey_initial_time_ = base::TimeTicks::Now();
  RecordConnectDeviceResult(/*success=*/true);
  // The device ID can change between device discovery and connection, so
  // ensure that device images are mapped to the current device ID.
  FastPairRepository::Get()->FetchDeviceImages(device_);
}

void FastPairPairerImpl::OnConnectError(const std::string& error_message) {
  if (!StopCreateBondTimer(__func__))
    return;

  QP_LOG(WARNING) << __func__ << " " << error_message;
  RecordConnectDeviceResult(/*success=*/false);
  std::move(pair_failed_callback_).Run(device_, PairFailure::kAddressConnect);
  // |this| may be destroyed after this line.
}

void FastPairPairerImpl::ConfirmPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  QP_LOG(INFO) << __func__;
  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyNegotiated,
                            *device_);
  RecordConfirmPasskeyAskTime(base::TimeTicks::Now() -
                              ask_confirm_passkey_initial_time_);
  confirm_passkey_initial_time_ = base::TimeTicks::Now();

  // TODO(b/251281330): Make handling this edge case more robust.
  //
  // We can get to this point where the BLE instance of the device is lost
  // (due to device specific flaky ADV), thus the FastPairHandshake is null,
  // and |fast_pair_handshake_| is garbage memory, but the classic Bluetooth
  // pairing continues. We stop the pairing in this case and show an error to
  // the user.
  if (!FastPairHandshakeLookup::GetInstance()->Get(device_)) {
    QP_LOG(ERROR) << __func__
                  << ": BLE device instance lost during passkey exchange";
    device->CancelPairing();
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kBleDeviceLostMidPair);
    return;
  }

  pairing_device_address_ = device->GetAddress();
  expected_passkey_ = passkey;
  fast_pair_gatt_service_client_->WritePasskeyAsync(
      /*message_type=*/0x02, /*passkey=*/expected_passkey_,
      fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairerImpl::OnPasskeyResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnPasskeyResponse(
    std::vector<uint8_t> response_bytes,
    absl::optional<PairFailure> failure) {
  QP_LOG(INFO) << __func__;
  RecordWritePasskeyCharacteristicResult(/*success=*/!failure.has_value());
  RecordProtocolPairingStep(
      FastPairProtocolPairingSteps::kRecievedPasskeyResponse, *device_);

  if (failure) {
    QP_LOG(WARNING) << __func__
                    << ": Failed to write passkey. Error: " << failure.value();
    RecordWritePasskeyCharacteristicPairFailure(failure.value());
    std::move(pair_failed_callback_).Run(device_, failure.value());
    // |this| may be destroyed after this line.
    return;
  }

  fast_pair_handshake_->fast_pair_data_encryptor()->ParseDecryptedPasskey(
      response_bytes,
      base::BindOnce(&FastPairPairerImpl::OnParseDecryptedPasskey,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void FastPairPairerImpl::OnParseDecryptedPasskey(
    base::TimeTicks decrypt_start_time,
    const absl::optional<DecryptedPasskey>& passkey) {
  if (!passkey) {
    QP_LOG(WARNING) << "Missing decrypted passkey from parse.";
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyDecryptFailure);
    // |this| may be destroyed after this line.
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    return;
  }

  if (passkey->message_type != FastPairMessageType::kProvidersPasskey) {
    QP_LOG(WARNING)
        << "Incorrect message type from decrypted passkey. Expected: "
        << MessageTypeToString(FastPairMessageType::kProvidersPasskey)
        << ". Actual: " << MessageTypeToString(passkey->message_type);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kIncorrectPasskeyResponseType);
    // |this| may be destroyed after this line.
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyValidated,
                            *device_);

  if (passkey->passkey != expected_passkey_) {
    QP_LOG(ERROR) << "Passkeys do not match. Expected: " << expected_passkey_
                  << ". Actual: " << passkey->passkey;
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyMismatch);
    // |this| may be destroyed after this line.
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyConfirmed,
                            *device_);
  RecordPasskeyCharacteristicDecryptResult(/*success=*/true);
  RecordPasskeyCharacteristicDecryptTime(base::TimeTicks::Now() -
                                         decrypt_start_time);
  RecordConfirmPasskeyConfirmTime(base::TimeTicks::Now() -
                                  confirm_passkey_initial_time_);

  device::BluetoothDevice* pairing_device =
      adapter_->GetDevice(pairing_device_address_);

  if (!pairing_device) {
    QP_LOG(ERROR) << "Bluetooth pairing device lost during write to passkey.";
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    // |this| may be destroyed after this line.
    return;
  }

  QP_LOG(INFO) << __func__ << ": Passkeys match, confirming pairing";
  pairing_device->ConfirmPairing();
  // DevicePairedChanged() is expected to be called following pairing
  // confirmation.
}

void FastPairPairerImpl::AttemptSendAccountKey() {
  // We only send the account key if we're doing an initial or retroactive
  // pairing. For subsequent pairing, we have to save the account key
  // locally so that we can refer to it in API calls to the server.
  if (device_->protocol == Protocol::kFastPairSubsequent) {
    QP_LOG(INFO) << __func__
                 << ": Saving Account Key locally for subsequent pair";
    FastPairRepository::Get()->AssociateAccountKeyLocally(device_);

    // If the Saved Devices feature is enabled and we are utilizing a "loose"
    // interpretation of a user's opt-in status, then we will opt-in the user
    // whenever they pair a Fast Pair device to saving devices to their account.
    // Although we don't surface the user's opt-in status in the Settings'
    // sub-page, this will surface on Android, and show devices saved to the
    // user's account. For subsequent pairing, we opt in the user after they
    // elect to pair with a device already saved to their account.
    if (features::IsFastPairSavedDevicesEnabled() &&
        !features::IsFastPairSavedDevicesStrictOptInEnabled()) {
      QP_LOG(VERBOSE) << __func__ << ": attempting to opt-in the user";
      FastPairRepository::Get()->UpdateOptInStatus(
          nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
          base::BindOnce(&FastPairPairerImpl::OnUpdateOptInStatus,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    std::move(pairing_procedure_complete_).Run(device_);
    return;
  }

  // If there is no signed in user, don't send the account key. This can only
  // happen in an initial pairing scenario since the retroactive pairing
  // scenario is disabled in the RetroactivePairingDetector for users who are
  // not signed in. Because this check happens a long time after the
  // FastPairPairerImpl is instantiated unlike other classes that disable
  // certain paths for users who are not signed in, we do not need to check for
  // a delayed login. At this point, if the user is not logged in, they will not
  // be.
  if (!ShouldBeEnabledForLoginStatus(
          Shell::Get()->session_controller()->login_status())) {
    if (device_->protocol == Protocol::kFastPairInitial) {
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kGuestModeDetected);
    }

    QP_LOG(VERBOSE) << __func__ << ": No logged in user to save account key to";
    std::move(pairing_procedure_complete_).Run(device_);
    return;
  }

  // We want to verify the opt in status if the flag is enabled before we write
  // an account key.
  if (features::IsFastPairSavedDevicesEnabled() &&
      features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    FastPairRepository::Get()->CheckOptInStatus(
        base::BindOnce(&FastPairPairerImpl::OnCheckOptInStatus,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // It's possible that the user has opted to initial pair to a device that
  // already has an account key saved. We check to see if this is the case
  // before writing a new account key.
  FastPairRepository::Get()->IsDeviceSavedToAccount(
      device_->classic_address().value(),
      base::BindOnce(&FastPairPairerImpl::OnIsDeviceSavedToAccount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnCheckOptInStatus(
    nearby::fastpair::OptInStatus status) {
  QP_LOG(INFO) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    QP_LOG(INFO) << __func__
                 << ": User is not opted in to save devices to their account";
    std::move(pairing_procedure_complete_).Run(device_);
    return;
  }

  // It's possible that the user has opted to initial pair to a device that
  // already has an account key saved. We check to see if this is the case
  // before writing a new account key.
  FastPairRepository::Get()->IsDeviceSavedToAccount(
      device_->classic_address().value(),
      base::BindOnce(&FastPairPairerImpl::OnIsDeviceSavedToAccount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnIsDeviceSavedToAccount(
    bool is_device_saved_to_account) {
  if (is_device_saved_to_account) {
    // If the device is saved to Footprints, don't write a new account key to
    // the device, and return that we've finished the pairing procedure
    // successfully. We could rework some of our APIs here so that we can call
    // AssociateAccountKeyLocally similar to how we handle Subsequent pairing
    // above. However, the first time a not discoverable advertisement for this
    // device is found we'll add the account key to our SavedDeviceRegistry as
    // expected.
    QP_LOG(INFO) << __func__
                 << ": Device is already saved, skipping write account key. "
                    "Pairing procedure complete.";

    if (device_->protocol == Protocol::kFastPairInitial) {
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kDeviceAlreadyAssociatedToAccount);
    }

    std::move(pairing_procedure_complete_).Run(device_);
    return;
  }

  // If we can't load the user's saved devices for some reason (e.g. offline)
  // |is_device_saved_to_account| will return false even though we didn't
  // properly check Footprints. This will cause us to write a new account key to
  // the device. This may cause problems since the device will have a different
  // account key than what is stored in Footprints, causing the not discoverable
  // advertisement to not be recognized.
  WriteAccountKey();
}

void FastPairPairerImpl::WriteAccountKey() {
  std::array<uint8_t, 16> account_key;
  RAND_bytes(account_key.data(), account_key.size());
  account_key[0] = 0x04;

  if (device_->protocol == Protocol::kFastPairInitial) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey);
  }

  fast_pair_gatt_service_client_->WriteAccountKey(
      account_key, fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairerImpl::OnWriteAccountKey,
                     weak_ptr_factory_.GetWeakPtr(), account_key));
}

void FastPairPairerImpl::OnWriteAccountKey(
    std::array<uint8_t, 16> account_key,
    absl::optional<AccountKeyFailure> failure) {
  RecordWriteAccountKeyCharacteristicResult(/*success=*/!failure.has_value());

  if (failure) {
    QP_LOG(WARNING) << "Failed to write account key to device due to error: "
                    << failure.value();
    std::move(account_key_failure_callback_).Run(device_, failure.value());
    return;
  }

  FastPairRepository::Get()->AssociateAccountKey(
      device_, std::vector<uint8_t>(account_key.begin(), account_key.end()));

  // If the Saved Devices feature is enabled and we are utilizing a "loose"
  // interpretation of a user's opt-in status, then we will opt-in the user
  // whenever they pair a Fast Pair device to saving devices to their account.
  // Although we don't surface the user's opt-in status in the Settings'
  // sub-page, this will surface on Android, and show devices saved to the
  // user's account. For initial pairing and retroactive pairing, we opt in the
  // user after after we successfully save an account key to their account.
  if (features::IsFastPairSavedDevicesEnabled() &&
      !features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    QP_LOG(VERBOSE) << __func__ << ": attempting to opt-in the user";
    FastPairRepository::Get()->UpdateOptInStatus(
        nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
        base::BindOnce(&FastPairPairerImpl::OnUpdateOptInStatus,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  QP_LOG(INFO)
      << __func__
      << ": Account key written to device. Pairing procedure complete.";

  if (device_->protocol == Protocol::kFastPairInitial) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kAccountKeyWritten);
  }

  std::move(pairing_procedure_complete_).Run(device_);
}

void FastPairPairerImpl::OnUpdateOptInStatus(bool success) {
  RecordSavedDevicesUpdatedOptInStatusResult(/*device=*/*device_,
                                             /*success=*/success);

  if (!success) {
    QP_LOG(WARNING) << __func__ << ": failure";
    return;
  }

  QP_LOG(VERBOSE) << __func__ << ": success";
}

void FastPairPairerImpl::RequestPinCode(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairerImpl::RequestPasskey(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairerImpl::DisplayPinCode(device::BluetoothDevice* device,
                                        const std::string& pincode) {
  NOTREACHED();
}

void FastPairPairerImpl::DisplayPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  NOTREACHED();
}

void FastPairPairerImpl::KeysEntered(device::BluetoothDevice* device,
                                     uint32_t entered) {
  NOTREACHED();
}

void FastPairPairerImpl::AuthorizePairing(device::BluetoothDevice* device) {
  NOTREACHED();
}

void FastPairPairerImpl::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device,
                                             bool new_paired_status) {
  if (!new_paired_status || !paired_callback_)
    return;

  if (device->GetAddress() == device_->ble_address ||
      device->GetAddress() == device_->classic_address()) {
    QP_LOG(INFO) << __func__ << ": Completing pairing procedure " << device_;
    RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPairingComplete,
                              *device_);

    std::move(paired_callback_).Run(device_);

    // For V2 devices we still need to remove the Pairing Delegate and write the
    // account key. `AttemptSendAccountKey()` will call
    // |pairing_procedure_complete_| whereas V1 devices need to run the callback
    // in this function since they don't write account keys, and their pairing
    // procedure is not complete at this point.
    if (device_->version().has_value() &&
        device_->version().value() == DeviceFastPairVersion::kHigherThanV1) {
      adapter_->RemovePairingDelegate(this);
      AttemptSendAccountKey();
    } else if (pairing_procedure_complete_) {
      // This covers the case where we are pairing a v1 device and are using the
      // Bluetooth pairing dialog to do it.
      std::move(pairing_procedure_complete_).Run(device_);
    }
  }
}

void FastPairPairerImpl::OnCreateBondTimeout() {
  QP_LOG(WARNING) << __func__
                  << ": Timeout while attempting to create bond with device.";
  std::move(pair_failed_callback_)
      .Run(device_, PairFailure::kCreateBondTimeout);
}

bool FastPairPairerImpl::StopCreateBondTimer(const std::string& callback_name) {
  if (create_bond_timeout_timer_.IsRunning()) {
    create_bond_timeout_timer_.Stop();
    return true;
  }

  QP_LOG(WARNING) << __func__ << ": " << callback_name
                  << " called after an attempt to create a bond with device"
                     "with classic address "
                  << device_->classic_address().value() << " has timed out.";
  return false;
}

}  // namespace quick_pair
}  // namespace ash
