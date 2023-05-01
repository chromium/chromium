// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/pair_failure.h"

namespace ash {
namespace quick_pair {

std::ostream& operator<<(std::ostream& stream, PairFailure failure) {
  switch (failure) {
    case PairFailure::kCreateGattConnection:
      stream << "[Failed to create a GATT connection to the device]";
      break;
    case PairFailure::kGattServiceDiscovery:
      stream << "[Failed to find the expected GATT service]";
      break;
    case PairFailure::kGattServiceDiscoveryTimeout:
      stream << "[Timed out while starting discovery of GATT service]";
      break;
    case PairFailure::kDataEncryptorRetrieval:
      stream << "[Failed to retrieve the data encryptor]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicDiscovery:
      stream << "[Failed to find the Key-based pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicDiscovery:
      stream << "[Failed to find the Passkey GATT characteristic]";
      break;
    case PairFailure::kAccountKeyCharacteristicDiscovery:
      stream << "[Failed to find the Account Key GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicNotifySession:
      stream << "[Failed to start a notify session on the Key-based pairing "
                "GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicNotifySession:
      stream << "[Failed to start a notify session on the Passkey GATT "
                "characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout:
      stream << "[Timed out while starting a notify session on the Key-based "
                "pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicNotifySessionTimeout:
      stream << "[Timed out while starting a notify session on the Passkey "
                "GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicWrite:
      stream
          << "[Failed to write to the Key-based pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyPairingCharacteristicWrite:
      stream << "[Failed to write to the Passkey GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingResponseTimeout:
      stream << "[Timed out while waiting for the Key-based Pairing response]";
      break;
    case PairFailure::kPasskeyResponseTimeout:
      stream << "[Timed out while waiting for the Passkey response]";
      break;
    case PairFailure::kKeybasedPairingResponseDecryptFailure:
      stream << "[Failed to decrypt Key-based Pairing response]";
      break;
    case PairFailure::kIncorrectKeyBasedPairingResponseType:
      stream << "[Incorrect Key-based response message type]";
      break;
    case PairFailure::kPasskeyDecryptFailure:
      stream << "[Failed to decrypt Passkey response]";
      break;
    case PairFailure::kIncorrectPasskeyResponseType:
      stream << "[Incorrect Passkey response message type]";
      break;
    case PairFailure::kPasskeyMismatch:
      stream << "[Passkeys did not match]";
      break;
    case PairFailure::kPairingDeviceLost:
      stream << "[Potential pairing device lost during Passkey exchange]";
      break;
    case PairFailure::kPairingConnect:
      stream << "[Failed to bond to discovered device]";
      break;
    case PairFailure::kAddressConnect:
      stream << "[Failed to bond to device via public address]";
      break;
    case PairFailure::kBleDeviceLostMidPair:
      stream << "[[BLE device instance lost mid pair with classic device "
                "instance]]";
      break;
    case PairFailure::kCreateBondTimeout:
      stream << "[Timed out while attempting to create bond with device]";
      break;
    case PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts:
      stream
          << "[Potential pairing device lost between GATT connection attempts]";
      break;
    case PairFailure::kConfirmPasskeyTimeout:
      stream << "[Timed out while waiting for confirm passkey event from "
                "Bluetooth adapter]";
      break;
    case PairFailure::kFailureToDisconnectGattBetweenRetries:
      stream << "[Failed to disconnect from GATT before retrying a failed GATT "
                "connection]";
      break;
    case PairFailure::kBluetoothDeviceFailureCreatingGattConnection:
      stream << "[Bluetooth platform layer has failed to create a GATT "
                "connection. This is not a complete failure, and Fast Pair may "
                "retry.]";
      break;
    case PairFailure::kDisconnectResponseTimeout:
      stream << "[Timed out while waiting for a response after attempt to "
                "disconnect]";
      break;
    case PairFailure::kFailedToConnectAfterPairing:
      stream << "[Failed to connect to discovered device after pairing when "
                "the device is known to the adapter.]";
      break;
    case PairFailure::kAdditionalDataCharacteristicWrite:
      stream << "[Failed to write to Additional Data GATT characteristic.]";
      break;
    case PairFailure::kAdditionalDataCharacteristicDiscovery:
      stream << "[Failed to find the Additional Data characteristic.]";
      break;
    case PairFailure::kAdditionalDataCharacteristicWriteTimeout:
      stream << "[Timed out while writing to Additional Data characteristic.]";
      break;
  }

  return stream;
}

}  // namespace quick_pair
}  // namespace ash
