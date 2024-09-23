// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_IMPL_H_

#include <optional>

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace ash {
namespace quick_pair {

class Device;
enum class AccountKeyFailure;
enum class PairFailure;
class FastPairDataEncryptor;
class FastPairHandshake;

class FastPairPairerImpl : public FastPairPairer,
                           public device::BluetoothDevice::PairingDelegate,
                           public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairPairer> Create(
        scoped_refptr<device::BluetoothAdapter> adapter,
        scoped_refptr<Device> device,
        base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
        base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
            pair_failed_callback,
        base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
            account_key_failure_callback,
        base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
        base::OnceCallback<void(scoped_refptr<Device>)>
            pairing_procedure_complete);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();

    virtual std::unique_ptr<FastPairPairer> CreateInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        scoped_refptr<Device> device,
        base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
        base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
            pair_failed_callback,
        base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
            account_key_failure_callback,
        base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
        base::OnceCallback<void(scoped_refptr<Device>)>
            pairing_procedure_complete) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairPairerImpl(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<Device> device,
      base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
      base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
      base::OnceCallback<void(scoped_refptr<Device>)>
          pairing_procedure_complete);
  FastPairPairerImpl(const FastPairPairerImpl&) = delete;
  FastPairPairerImpl& operator=(const FastPairPairerImpl&) = delete;
  FastPairPairerImpl(FastPairPairerImpl&&) = delete;
  FastPairPairerImpl& operator=(FastPairPairerImpl&&) = delete;
  ~FastPairPairerImpl() override;

 private:
  // There are two flows a device can go through for V2 pairing:
  // `device::BluetoothAdapter::ConnectDevice` and
  // `device::BluetoothDevice::Pair`. The flows for each are as follows:
  //
  // ConnectDevice : `ConnectDevice` -> `OnConnectDevice -> `ConfirmPasskey` ->
  // `WritePasskeyAsync` -> `OnPasskeyResponse` -> `DevicePairedChanged`
  //
  // Pair: `Pair` -> `ConfirmPasskey` -> `WritePasskeyAsync` ->
  // `OnPasskeyResponse` -> `DevicePairedChanged` -> `OnPairConnected` ->
  // `Connect` -> `OnConnected`
  //
  // We need to capture which flow we are using in order to correctly stop
  // the bonding timer when the flow has ended, since each has a different
  // end.
  enum class FastPairPairingFlow {
    kConnectDevice,
    kPair,
  };

  // device::BluetoothDevice::PairingDelegate
  void RequestPinCode(device::BluetoothDevice* device) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // device::BluetoothAdapter::Observer
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // Helper to safely stop |create_bond_timeout_timer_|.
  // If the timer can be stopped because it is running, this function returns
  // true. If the timer cannot be stopped, this function returns false,
  // informing the caller that the timer has expired and the caller should not
  // proceed with bond creation.
  bool StopCreateBondTimer(const std::string& callback_name);

  // device::BluetoothDevice::Pair callback
  void OnPairConnected(
      std::optional<device::BluetoothDevice::ConnectErrorCode> error);

  // device::BluetoothDevice::Connect callback
  void OnConnected(
      std::optional<device::BluetoothDevice::ConnectErrorCode> error);

  // device::BluetoothAdapter::ConnectDevice callbacks
  void OnConnectDevice(device::BluetoothDevice* device);
  void OnConnectError(const std::string& error_message);

  // Callback for timeout on creating a bond with |device_| in
  // StartPairing.
  void OnCreateBondTimeout();

  //  FastPairHandshakeLookup::Create callback
  void OnHandshakeComplete(scoped_refptr<Device> device,
                           std::optional<PairFailure> failure);

  // FastPairGattServiceClient::WritePasskey callback
  void OnPasskeyResponse(std::vector<uint8_t> response_bytes,
                         std::optional<PairFailure> failure);

  // FastPairDataEncryptor::ParseDecryptedPasskey callback
  void OnParseDecryptedPasskey(base::TimeTicks decrypt_start_time,
                               const std::optional<DecryptedPasskey>& passkey);

  // FastPairRepository::IsDeviceSavedToAccount callback
  void OnIsDeviceSavedToAccount(bool is_device_saved_to_account);

  // FastPairRepository::CheckOptInStatus callback
  void OnCheckOptInStatus(nearby::fastpair::OptInStatus status);

  // FastPairRepository::UpdateOptInStatus callback
  void OnUpdateOptInStatus(bool success);

  // Creates a 16-byte array of random bytes with a first byte of 0x04 to
  // signal Fast Pair account key, and then writes to the device.
  void AttemptSendAccountKey();

  // FastPairDataEncryptor::WriteAccountKey callback
  void OnWriteAccountKey(std::array<uint8_t, 16> account_key,
                         std::optional<AccountKeyFailure> error);

  void StartPairing();

  void WriteAccountKey();

  FastPairPairingFlow pairing_flow_;
  uint32_t expected_passkey_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  std::string pairing_device_address_;
  base::OnceCallback<void(scoped_refptr<Device>)> paired_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
      pair_failed_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(std::u16string, uint32_t)> display_passkey_;
  base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete_;
  raw_ptr<FastPairHandshake, DanglingUntriaged> fast_pair_handshake_ = nullptr;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};

  // A timer to time the bonding with |device_| in StartPairing and invoke a
  // timeout if necessary.
  base::OneShotTimer create_bond_timeout_timer_;
  base::TimeTicks create_bond_start_time_;

  base::WeakPtrFactory<FastPairPairerImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_IMPL_H_
