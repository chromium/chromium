// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

// 15s timeouts chosen to align with Android's Fast Pair implementation.
constexpr base::TimeDelta kCreateBondTimeout = base::Seconds(15);
// Advertisement flag indicating BR/EDR support
constexpr uint8_t kBrEdrNotSupportedFlag = 0x04;
// Key-based Pairing Extended Response Flag indicating if the Provider is LE
// only device
constexpr uint8_t kLEOnly = 0x80;
// Key-based Pairing Extended Response Flag indicating if the Provider prefers
// LE bonding
constexpr uint8_t kPrefersLEBonding = 0x40;

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
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
    base::OnceCallback<void(scoped_refptr<Device>)>
        pairing_procedure_complete) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(
        std::move(adapter), std::move(device), std::move(paired_callback),
        std::move(pair_failed_callback),
        std::move(account_key_failure_callback), std::move(display_passkey),
        std::move(pairing_procedure_complete));
  }

  return base::WrapUnique(new FastPairPairerImpl(
      std::move(adapter), std::move(device), std::move(paired_callback),
      std::move(pair_failed_callback), std::move(account_key_failure_callback),
      std::move(display_passkey), std::move(pairing_procedure_complete)));
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
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
    base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete)
    : adapter_(std::move(adapter)),
      device_(std::move(device)),
      paired_callback_(std::move(paired_callback)),
      pair_failed_callback_(std::move(pair_failed_callback)),
      account_key_failure_callback_(std::move(account_key_failure_callback)),
      display_passkey_(std::move(display_passkey)),
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
        device_->ble_address());
    return;
  }

  fast_pair_handshake_ = FastPairHandshakeLookup::GetInstance()->Get(device_);

  DCHECK(fast_pair_handshake_);
  DCHECK(fast_pair_handshake_->completed_successfully());

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
  uint8_t pairing_flags = device_->key_based_pairing_flags().value_or(0);
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      floss::features::IsFlossEnabled() && pairing_flags & kPrefersLEBonding) {
    device_address = device_->ble_address();
  }
  device::BluetoothDevice* bt_device = adapter_->GetDevice(device_address);
  switch (device_->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      // Now that we have validated the decrypted response, we can attempt to
      // retrieve the device from the adapter by the address. If we are able
      // to get the device, and it's not already paired, we can pair directly.
      // Often, we will not be able to find the device this way, and we will
      // have to connect via address and add ourselves as a pairing delegate.
      CD_LOG(VERBOSE, Feature::FP)
          << "Sending pair request to device. Address: " << device_address
          << ". Found device: " << ((bt_device != nullptr) ? "Yes" : "No")
          << ".";

      if (bt_device && bt_device->IsBonded()) {
        CD_LOG(VERBOSE, Feature::FP)
            << __func__
            << ": Trying to pair to device that is already paired; "
               "returning success.";

        RecordProtocolPairingStep(FastPairProtocolPairingSteps::kAlreadyPaired,
                                  *device_);
        RecordProtocolPairingStep(
            FastPairProtocolPairingSteps::kPairingComplete, *device_);
        AttemptRecordingFastPairEngagementFlow(
            *device_,
            FastPairEngagementFlowEvent::kPairingSucceededAlreadyPaired);

        if (!bt_device->IsConnected()) {
          CD_LOG(VERBOSE, Feature::FP)
              << __func__ << ": connecting a paired device";
          create_bond_start_time_ = base::TimeTicks::Now();
          create_bond_timeout_timer_.Start(
              FROM_HERE, kCreateBondTimeout,
              base::BindOnce(&FastPairPairerImpl::OnCreateBondTimeout,
                             weak_ptr_factory_.GetWeakPtr()));
          // On Floss, always connect via classic unless device explicitly
          // doesn't support BREDR or indicates it prefers LE.
          if (floss::features::IsFlossEnabled() &&
              !(bt_device->GetAdvertisingDataFlags().value_or(0) &
                    kBrEdrNotSupportedFlag ||
                pairing_flags & kLEOnly || pairing_flags & kPrefersLEBonding)) {
            bt_device->ConnectClassic(
                /*pairing_delegate=*/this,
                base::BindOnce(&FastPairPairerImpl::OnConnected,
                               weak_ptr_factory_.GetWeakPtr()));
            return;
          }
          bt_device->Connect(/*pairing_delegate=*/this,
                             base::BindOnce(&FastPairPairerImpl::OnConnected,
                                            weak_ptr_factory_.GetWeakPtr()));
          return;
        }

        std::move(paired_callback_).Run(device_);

        RecordProtocolPairingStep(
            FastPairProtocolPairingSteps::kDeviceConnected, *device_);
        AttemptSendAccountKey();
        return;
      }

      // There are two flows a device can go through for V2 pairing:
      // `device::BluetoothAdapter::ConnectDevice` and
      // `device::BluetoothDevice::Pair`. The flows for each are as follows:
      //
      // ConnectDevice : `ConnectDevice` -> `OnConnectDevice -> `ConfirmPasskey`
      //  -> `WritePasskeyAsync` -> `OnPasskeyResponse` -> `DevicePairedChanged`
      //
      // Pair: `Pair` -> `ConfirmPasskey` -> `WritePasskeyAsync` ->
      // `OnPasskeyResponse` -> `DevicePairedChanged` -> `OnPairConnected` ->
      // `Connect` -> `OnConnected`
      //
      // This timer captures a bonding timeout for the both scenarios.
      create_bond_start_time_ = base::TimeTicks::Now();
      create_bond_timeout_timer_.Start(
          FROM_HERE, kCreateBondTimeout,
          base::BindOnce(&FastPairPairerImpl::OnCreateBondTimeout,
                         weak_ptr_factory_.GetWeakPtr()));

      // TODO(b/266502308): Re-evaluate how we can force a Bluetooth profile
      // for a device to avoid `device::BluetoothAdapter::ConnectDevice` API.
      //
      // The Sony SRS-XB13 is expected to fail `ConnectDevice`, due to SDP
      // collisions, and succeed on a second retry with
      // `device::BluetoothDevice::Pair` because the device profile is ready.
      if (bt_device) {
        pairing_flow_ = FastPairPairingFlow::kPair;
        // On Floss, always connect via classic unless device explicitly
        // doesn't support BREDR or indicates it prefers LE. ConnectClassic is
        // equivalent to Pair.
        if (floss::features::IsFlossEnabled() &&
            !(bt_device->GetAdvertisingDataFlags().value_or(0) &
                  kBrEdrNotSupportedFlag ||
              pairing_flags & kLEOnly || pairing_flags & kPrefersLEBonding)) {
          bt_device->ConnectClassic(
              /*pairing_delegate=*/this,
              base::BindOnce(&FastPairPairerImpl::OnPairConnected,
                             weak_ptr_factory_.GetWeakPtr()));
          return;
        }
        bt_device->Pair(/*pairing_delegate=*/this,
                        base::BindOnce(&FastPairPairerImpl::OnPairConnected,
                                       weak_ptr_factory_.GetWeakPtr()));
      } else {
        pairing_flow_ = FastPairPairingFlow::kConnectDevice;
        adapter_->AddPairingDelegate(
            this, device::BluetoothAdapter::PairingDelegatePriority::
                      PAIRING_DELEGATE_PRIORITY_HIGH);
        adapter_->ConnectDevice(
            device_address,
            /*address_type=*/std::nullopt,
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

void FastPairPairerImpl::OnConnectDevice(device::BluetoothDevice* device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  if (floss::features::IsFlossEnabled()) {
    // On Floss, ConnectDevice behaves like CreateDevice. It only creates
    // a new device object so we have to follow up with actually Pair()-ing
    // to it.
    CD_LOG(INFO, Feature::FP) << __func__ << " on Floss";

    // Always connect via classic unless device explicitly
    // doesn't support BREDR or indicates it prefers LE. ConnectClassic is
    // equivalent to Pair.
    uint8_t pairing_flags = device_->key_based_pairing_flags().value_or(0);
    if (!(device->GetAdvertisingDataFlags().value_or(0) &
              kBrEdrNotSupportedFlag ||
          pairing_flags & kLEOnly || pairing_flags & kPrefersLEBonding)) {
      device->ConnectClassic(
          /*pairing_delegate=*/this,
          base::BindOnce(&FastPairPairerImpl::OnPairConnected,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    device->Pair(/*pairing_delegate=*/this,
                 base::BindOnce(&FastPairPairerImpl::OnPairConnected,
                                weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kDeviceConnected,
                            *device_);
  RecordConnectDeviceResult(/*success=*/true);
}

void FastPairPairerImpl::OnConnectError(const std::string& error_message) {
  if (!StopCreateBondTimer(__func__)) {
    return;
  }

  CD_LOG(WARNING, Feature::FP) << __func__ << " " << error_message;
  RecordConnectDeviceResult(/*success=*/false);
  std::move(pair_failed_callback_).Run(device_, PairFailure::kAddressConnect);
  // |this| may be destroyed after this line.
}

void FastPairPairerImpl::ConfirmPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyNegotiated,
                            *device_);

  auto* ble_device = adapter_->GetDevice(device_->ble_address());

  // TODO(b/251281330): Make handling this edge case more robust.
  //
  // We can get to this point where the BLE instance of the device is lost
  // (due to device specific flaky ADV), thus the FastPairHandshake is null,
  // and |fast_pair_handshake_| is garbage memory, but the classic Bluetooth
  // pairing continues. We stop the pairing in this case and show an error to
  // the user.
  if (!FastPairHandshakeLookup::GetInstance()->Get(device_) || !ble_device) {
    CD_LOG(ERROR, Feature::FP)
        << __func__ << ": BLE device instance lost during passkey exchange";

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
    device->CancelPairing();
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kBleDeviceLostMidPair);
    return;
  }

  pairing_device_address_ = device->GetAddress();
  expected_passkey_ = passkey;

  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(ble_device);
  CHECK(fast_pair_gatt_service_client);

  fast_pair_gatt_service_client->WritePasskeyAsync(
      /*message_type=*/0x02, /*passkey=*/expected_passkey_,
      fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairerImpl::OnPasskeyResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnPasskeyResponse(std::vector<uint8_t> response_bytes,
                                           std::optional<PairFailure> failure) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  RecordWritePasskeyCharacteristicResult(/*success=*/!failure.has_value());
  RecordProtocolPairingStep(
      FastPairProtocolPairingSteps::kRecievedPasskeyResponse, *device_);

  if (failure) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to write passkey. Error: " << failure.value();

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
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
    const std::optional<DecryptedPasskey>& passkey) {
  if (!passkey) {
    CD_LOG(WARNING, Feature::FP) << "Missing decrypted passkey from parse.";

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyDecryptFailure);
    // |this| may be destroyed after this line.
    return;
  }

  if (passkey->message_type != FastPairMessageType::kProvidersPasskey) {
    CD_LOG(WARNING, Feature::FP)
        << "Incorrect message type from decrypted passkey. Expected: "
        << MessageTypeToString(FastPairMessageType::kProvidersPasskey)
        << ". Actual: " << MessageTypeToString(passkey->message_type);

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kIncorrectPasskeyResponseType);
    // |this| may be destroyed after this line.
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyValidated,
                            *device_);

  if (passkey->passkey != expected_passkey_) {
    CD_LOG(ERROR, Feature::FP)
        << "Passkeys do not match. Expected: " << expected_passkey_
        << ". Actual: " << passkey->passkey;

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
    RecordPasskeyCharacteristicDecryptResult(/*success=*/false);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPasskeyMismatch);
    // |this| may be destroyed after this line.
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPasskeyConfirmed,
                            *device_);
  RecordPasskeyCharacteristicDecryptResult(/*success=*/true);

  device::BluetoothDevice* pairing_device =
      adapter_->GetDevice(pairing_device_address_);

  if (!pairing_device) {
    CD_LOG(WARNING, Feature::FP)
        << "Bluetooth pairing device lost during write to passkey.";

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state.
    StopCreateBondTimer(__func__);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    // |this| may be destroyed after this line.
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Passkeys match, confirming pairing";
  pairing_device->ConfirmPairing();
  // DevicePairedChanged() is expected to be called following pairing
  // confirmation.
}

void FastPairPairerImpl::AttemptSendAccountKey() {
  // We only send the account key if we're doing an initial or retroactive
  // pairing. For subsequent pairing, we have to save the account key
  // locally so that we can refer to it in API calls to the server.
  if (device_->protocol() == Protocol::kFastPairSubsequent) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Saving Account Key locally for subsequent pair";
    FastPairRepository::Get()->WriteAccountAssociationToLocalRegistry(device_);

    // If the Saved Devices feature is enabled and we are utilizing a "loose"
    // interpretation of a user's opt-in status, then we will opt-in the user
    // whenever they pair a Fast Pair device to saving devices to their account.
    // Although we don't surface the user's opt-in status in the Settings'
    // sub-page, this will surface on Android, and show devices saved to the
    // user's account. For subsequent pairing, we opt in the user after they
    // elect to pair with a device already saved to their account.
    if (features::IsFastPairSavedDevicesEnabled() &&
        !features::IsFastPairSavedDevicesStrictOptInEnabled()) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": attempting to opt-in the user";
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
    if (device_->protocol() == Protocol::kFastPairInitial) {
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kGuestModeDetected);
    }

    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": No logged in user to save account key to";
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
  // (b/266953410) This check is performed previously in the Retroactive Pairing
  // Flow, in `RetroactivePairingDetectorImpl::DevicePairedChanged`. To avoid
  // making this redundant request to Footprints, |IsDeviceSavedToAccount| is
  // called only in the Initial Pair scenario.
  if (device_->protocol() != Protocol::kFastPairRetroactive) {
    FastPairRepository::Get()->IsDeviceSavedToAccount(
        device_->classic_address().value(),
        base::BindOnce(&FastPairPairerImpl::OnIsDeviceSavedToAccount,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // If the BLE address has rotated writing the account key is guaranteed to
    // fail. Instead of proceeding, call the callback and return.
    if (ash::features::IsFastPairBleRotationEnabled() &&
        fast_pair_handshake_->DidBleAddressRotate()) {
      // TODO (b/268055837): add metric for when we get in this scenario.
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": BLE Address rotated, running callback";
      fast_pair_handshake_->RunBleAddressRotationCallback();
      return;
    }
    WriteAccountKey();
  }
}

void FastPairPairerImpl::OnCheckOptInStatus(
    nearby::fastpair::OptInStatus status) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
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
    // `WriteAccountAssociationToLocalRegistry` similar to how we handle
    // Subsequent pairing above. However, the first time a not discoverable
    // advertisement for this device is found we'll add the account key to our
    // SavedDeviceRegistry as expected.
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Device is already saved, skipping write account key. "
           "Pairing procedure complete.";

    if (device_->protocol() == Protocol::kFastPairInitial) {
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

  if (device_->protocol() == Protocol::kFastPairInitial) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey);
  }

  auto* device = adapter_->GetDevice(device_->ble_address());
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": device lost when attempting to retrieve GATT service client.";
    std::move(account_key_failure_callback_)
        .Run(device_, AccountKeyFailure::kGattErrorFailed);
    return;
  }

  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(device);
  CHECK(fast_pair_gatt_service_client);

  fast_pair_gatt_service_client->WriteAccountKey(
      account_key, fast_pair_handshake_->fast_pair_data_encryptor(),
      base::BindOnce(&FastPairPairerImpl::OnWriteAccountKey,
                     weak_ptr_factory_.GetWeakPtr(), account_key));
}

void FastPairPairerImpl::OnWriteAccountKey(
    std::array<uint8_t, 16> account_key,
    std::optional<AccountKeyFailure> failure) {
  RecordWriteAccountKeyCharacteristicResult(/*success=*/!failure.has_value());

  if (failure) {
    CD_LOG(WARNING, Feature::FP)
        << "Failed to write account key to device due to error: "
        << failure.value();
    std::move(account_key_failure_callback_).Run(device_, failure.value());
    return;
  }

  if (ash::features::IsFastPairSavedDevicesNicknamesEnabled() &&
      device_->classic_address().has_value() &&
      adapter_->GetDevice(device_->classic_address().value())) {
    device_->set_display_name(
        (adapter_->GetDevice(device_->classic_address().value()))->GetName());
  }

  const std::vector<uint8_t> account_key_vec(account_key.begin(),
                                             account_key.end());

  device_->set_account_key(account_key_vec);
  if (!FastPairRepository::Get()->WriteAccountAssociationToLocalRegistry(
          device_)) {
    CD_LOG(WARNING, Feature::FP)
        << "Failed to write account association to Local Registry.";
  }

  // Devices in the Retroactive Pair scenario are not written to Footprints
  // on account key write, but when the user hits 'Save' on the retroactive pair
  // notification.
  if (device_->protocol() != Protocol::kFastPairRetroactive) {
    FastPairRepository::Get()->WriteAccountAssociationToFootprints(
        device_, account_key_vec);
  }

  // If the Saved Devices feature is enabled and we are utilizing a "loose"
  // interpretation of a user's opt-in status, then we will opt-in the user
  // whenever they pair a Fast Pair device to saving devices to their account.
  // Although we don't surface the user's opt-in status in the Settings'
  // sub-page, this will surface on Android, and show devices saved to the
  // user's account. For initial pairing and retroactive pairing, we opt in the
  // user after after we successfully save an account key to their account.
  if (features::IsFastPairSavedDevicesEnabled() &&
      !features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": attempting to opt-in the user";
    FastPairRepository::Get()->UpdateOptInStatus(
        nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
        base::BindOnce(&FastPairPairerImpl::OnUpdateOptInStatus,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": Account key written to device. Pairing procedure complete.";

  if (device_->protocol() == Protocol::kFastPairInitial) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kAccountKeyWritten);
  }

  std::move(pairing_procedure_complete_).Run(device_);
}

void FastPairPairerImpl::OnUpdateOptInStatus(bool success) {
  RecordSavedDevicesUpdatedOptInStatusResult(/*device=*/*device_,
                                             /*success=*/success);

  if (!success) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": failure";
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": success";
}

void FastPairPairerImpl::RequestPinCode(device::BluetoothDevice* device) {
  // Left unimplemented.
}

void FastPairPairerImpl::RequestPasskey(device::BluetoothDevice* device) {
  // Left unimplemented.
}

void FastPairPairerImpl::DisplayPinCode(device::BluetoothDevice* device,
                                        const std::string& pincode) {
  // Left unimplemented.
}

void FastPairPairerImpl::DisplayPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      floss::features::IsFlossEnabled()) {
    std::move(display_passkey_).Run(device->GetNameForDisplay(), passkey);
  }
}

void FastPairPairerImpl::KeysEntered(device::BluetoothDevice* device,
                                     uint32_t entered) {
  // Left unimplemented.
}

void FastPairPairerImpl::AuthorizePairing(device::BluetoothDevice* device) {
  // Left unimplemented.
}

void FastPairPairerImpl::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device,
                                             bool new_paired_status) {
  if (!new_paired_status || !paired_callback_) {
    return;
  }

  if ((device_->classic_address().has_value() &&
       device->GetAddress() == device_->classic_address().value()) ||
      device->GetAddress() == device_->ble_address()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Completing pairing procedure " << device_;

    // V1 devices do not set the classic_address() field anywhere else, which is
    // needed to map device addresses to persisted device images. Set the
    // classic address here, which has to happen before paired_callback_ is
    // fired. V2 devices can also set a missing classic address here, although
    // that is not expected to happen.
    if (!device_->classic_address() &&
        device->GetAddressType() ==
            device::BluetoothDevice::AddressType::ADDR_TYPE_PUBLIC) {
      device_->set_classic_address(device->GetAddress());
    }

    bool is_v1_device =
        device_->version().has_value() &&
        device_->version().value() == DeviceFastPairVersion::kV1;
    bool is_pair_branch = pairing_flow_ == FastPairPairingFlow::kPair;

    // DevicePairedChanged is called in the middle of the "kPair" pairing flow,
    // so we don't `AttemptSendAccountKey` until after `OnConnected` fires.
    if (is_pair_branch && !is_v1_device) {
      return;
    }

    // Log and notify that we have successfully paired to the device.
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Successfully paired to device " << device_;
    RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPairingComplete,
                              *device_);
    std::move(paired_callback_).Run(device_);

    // For V1 devices, this is the end of the pairing fow, since we are using
    // the Bluetooth pairing dialog. V1 devices need to run the
    // |pairing_procedure_complete_| callback in this function since they don't
    // write account keys.
    if (is_v1_device) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": pairing procedure completed for V1 device.";
      std::move(pairing_procedure_complete_).Run(device_);
      return;
    }

    // For V2+ devices in the `ConnectDevice` flow, this is the end of the flow.
    // Stop the timer since we have reached a terminal state of success, remove
    // the Pairing Delegate, and write the account key.
    StopCreateBondTimer(__func__);
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Stopping create bond timer and attempting to send "
           "account key for ConnectDevice flow";
    adapter_->RemovePairingDelegate(this);
    AttemptSendAccountKey();
  }
}

void FastPairPairerImpl::OnPairConnected(
    std::optional<device::BluetoothDevice::ConnectErrorCode> error) {
  // Check that the timer is still running before continuing. If the timer has
  // expired, then we already have surface an error through
  // `OnCreateBondTimeout` and we should not continue here. This handles the
  // case where this object has not been destroyed yet following a PairFailure,
  // and the `OnPairConnected` callback executes.
  if (!create_bond_timeout_timer_.IsRunning()) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__;

  if (error) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to start pairing procedure by pairing to "
           "device due to error: "
        << error.value();

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state for the `Pair` flow.
    StopCreateBondTimer(__func__);
    RecordPairDeviceErrorReason(error.value());
    RecordPairDeviceResult(/*success=*/false);
    // |this| may be destroyed after this line.
    std::move(pair_failed_callback_).Run(device_, PairFailure::kPairingConnect);
    return;
  }

  std::string device_address = device_->classic_address().value();
  uint8_t pairing_flags = device_->key_based_pairing_flags().value_or(0);
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      floss::features::IsFlossEnabled() && pairing_flags & kPrefersLEBonding) {
    device_address = device_->ble_address();
  }
  device::BluetoothDevice* bt_device = adapter_->GetDevice(device_address);
  if (!bt_device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Bluetooth pairing device lost during during device connection";

    // Stop create bond timer on error because at this point, the pairing is
    // in a terminal state for the `Pair` flow.
    StopCreateBondTimer(__func__);
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    // |this| may be destroyed after this line.
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kPairingComplete,
                            *device_);

  if (floss::features::IsFlossEnabled()) {
    // On Floss, Pair is exactly the same as Connect. Therefore we skip calling
    // Connect().
    CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Skipping Connect on Floss";
    OnConnected(std::nullopt);
    return;
  }

  // We must follow `Pair` with `Connect`. Not all Fast Pair devices initiate
  // a connection following pairing. For device that do initiate connecting
  // following pairing, this may result in `OnConnected` to return a failure,
  // however the connection is successful.
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": attempting connection to device following pair";
  bt_device->Connect(/*pairing_delegate=*/this,
                     base::BindOnce(&FastPairPairerImpl::OnConnected,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void FastPairPairerImpl::OnConnected(
    std::optional<device::BluetoothDevice::ConnectErrorCode> error) {
  // Terminal state for `Pair` flow, so we stop the timer here for this path.
  // We don't need to check which flow we are in here, since we can only
  // reach this point with `Pair`.
  if (!StopCreateBondTimer(__func__)) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__;
  RecordPairDeviceResult(/*success=*/!error.has_value());

  if (error) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to start pairing procedure by pairing to "
           "device due to error: "
        << error.value();
    RecordPairDeviceErrorReason(error.value());
    std::move(pair_failed_callback_)
        .Run(device_, PairFailure::kFailedToConnectAfterPairing);
    // |this| may be destroyed after this line.
    return;
  }

  RecordProtocolPairingStep(FastPairProtocolPairingSteps::kDeviceConnected,
                            *device_);

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": starting account key write for `Pair` flow";
  adapter_->RemovePairingDelegate(this);

  std::move(paired_callback_).Run(device_);
  AttemptSendAccountKey();
}

void FastPairPairerImpl::OnCreateBondTimeout() {
  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Timeout while attempting to create bond with device.";
  std::move(pair_failed_callback_)
      .Run(device_, PairFailure::kCreateBondTimeout);
}

bool FastPairPairerImpl::StopCreateBondTimer(const std::string& callback_name) {
  if (create_bond_timeout_timer_.IsRunning()) {
    RecordCreateBondTime(base::TimeTicks::Now() - create_bond_start_time_);
    create_bond_timeout_timer_.Stop();
    return true;
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": " << callback_name
      << " called after an attempt to create a bond with device"
         "with classic address "
      << device_->classic_address().value() << " has timed out.";
  return false;
}

}  // namespace quick_pair
}  // namespace ash
