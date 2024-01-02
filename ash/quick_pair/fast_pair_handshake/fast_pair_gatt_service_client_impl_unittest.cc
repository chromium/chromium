// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"

#include <stddef.h>

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

using NotifySessionCallback = base::OnceCallback<void(
    std::unique_ptr<device::BluetoothGattNotifySession>)>;
using ErrorCallback =
    base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>;

const char kTotalGattConnectionTime[] =
    "Bluetooth.ChromeOS.FastPair.TotalGattConnectionTime";
const char kGattConnectionResult[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.Result";
const char kGattConnectionErrorMetric[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.ErrorReason";
const char kGattConnectionEffectiveSuccessRate[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.EffectiveSuccessRate";
const char kGattConnectionAttemptCount[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.AttemptCount";
const char kWriteKeyBasedCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.GattErrorReason";
const char kNotifyKeyBasedCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.NotifyTime";
const char kWritePasskeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.GattErrorReason";
const char kNotifyPasskeyCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.NotifyTime";
const char kWriteAccountKeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.GattErrorReason";
const char kWriteAccountKeyTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.TotalTime";
const char kFastPairGattConnectionStep[] = "FastPair.GattConnection";
const char kFastPairGattRetryFailureReason[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.RetryFailureReason";
const char kGattServiceDiscoveryTime[] =
    "FastPair.GattServiceDiscovery.Latency";
const char kPasskeyNotify[] = "FastPair.PasskeyNotify.Latency";
const char kKeyBasedNotify[] = "FastPair.KeyBasedNotify.Latency";
const char kPasskeyWriteRequest[] = "FastPair.PasskeyWriteRequest.Latency";
const char kKeyBasedWriteRequest[] = "FastPair.KeyBasedWriteRequest.Latency";
/*
 */

constexpr base::TimeDelta kConnectingTestTimeout = base::Seconds(15);
constexpr base::TimeDelta kSimulateStackFrameHangSeconds = base::Seconds(90);
constexpr base::TimeDelta kCoolOffPeriodBeforeGattConnectionAfterDisconnect =
    base::Seconds(2);
constexpr base::TimeDelta kDisconnectResponseTimeout = base::Seconds(5);

// 51 seconds is from 2 seconds for a cooloff period before GATT connection
// attempts * 3 GATT attempts + 15 seconds GATT connecting timeout * 3 GATT
// attempts.
constexpr base::TimeDelta kAllGattRetriesPeriod = base::Seconds(51);

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";
const char kTestServiceId[] = "service_id1";
const std::string kUUIDString1 = "keybased";
const std::string kUUIDString2 = "passkey";
const std::string kUUIDString3 = "accountkey";
const std::string kUUIDString4 = "additional data";
const std::string kUUIDString5 = "model id";
const device::BluetoothUUID kNonFastPairUuid("0xFE2B");

const device::BluetoothUUID kModelIDCharacteristicUuid1("1233");
const device::BluetoothUUID kModelIDCharacteristicUuid2(
    "FE2C1233-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kKeyBasedCharacteristicUuid1("1234");
const device::BluetoothUUID kKeyBasedCharacteristicUuid2(
    "FE2C1234-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kPasskeyCharacteristicUuid1("1235");
const device::BluetoothUUID kPasskeyCharacteristicUuid2(
    "FE2C1235-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAccountKeyCharacteristicUuid1("1236");
const device::BluetoothUUID kAccountKeyCharacteristicUuid2(
    "FE2C1236-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAdditionalDataCharacteristicUuid1("1237");
const device::BluetoothUUID kAdditionalDataCharacteristicUuid2(
    "FE2C1237-8366-4814-8EB0-01DE32100BEA");

const uint8_t kMessageType = 0x00;
const uint8_t kFlags = 0x00;
const std::string kProviderAddress = "abcde";
const std::string kSeekersAddress = "abcde";
const uint8_t kSeekerPasskey = 0x02;
const uint32_t kPasskey = 13;
const std::array<uint8_t, 16> kAccountKey = {0x04, 0x01, 0x01, 0x01, 0x01, 0x01,
                                             0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                             0x01, 0x01, 0x01, 0x01};

const std::array<uint8_t, 64> kPublicKey = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F,
    0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
    0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01,
    0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45,
    0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32,
    0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

const std::string kPersonalizedName = "Brando's Fake Device";

const device::BluetoothRemoteGattCharacteristic::Properties kProperties =
    device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
    device::BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
    device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const device::BluetoothRemoteGattCharacteristic::Permissions kPermissions =
    device::BluetoothRemoteGattCharacteristic::PERMISSION_READ_ENCRYPTED |
    device::BluetoothRemoteGattCharacteristic::PERMISSION_WRITE_ENCRYPTED;

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(ash::quick_pair::FakeBluetoothAdapter* adapter,
                      const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true),
        fake_adapter_(adapter) {}

  void CreateGattConnection(
      device::BluetoothDevice::GattConnectionCallback callback,
      std::optional<device::BluetoothUUID> service_uuid =
          std::nullopt) override {
    if (has_gatt_connection_hang_) {
      // Fast forward time to simulate this stack frame not finishing until
      // after the timer has fired.
      task_environment_->FastForwardBy(kSimulateStackFrameHangSeconds);
      return;
    }

    gatt_connection_ = std::make_unique<
        testing::NiceMock<device::MockBluetoothGattConnection>>(
        fake_adapter_.get(), kTestBleDeviceAddress);

    if (has_gatt_connection_error_) {
      std::move(callback).Run(std::move(gatt_connection_),
                              /*error_code=*/device::BluetoothDevice::
                                  ConnectErrorCode::ERROR_FAILED);
    } else {
      ON_CALL(*gatt_connection_.get(), IsConnected)
          .WillByDefault(testing::Return(true));
      std::move(callback).Run(std::move(gatt_connection_), std::nullopt);
    }
  }

  void Disconnect(base::OnceClosure callback,
                  base::OnceClosure error_callback) override {
    was_disconnect_called_ = true;
    if (has_disconnect_error_) {
      if (!no_disconnect_response_) {
        std::move(error_callback).Run();
      }
      return;
    }
    std::move(callback).Run();
  }

  void SetDisconnectError(bool has_disconnect_error) {
    has_disconnect_error_ = has_disconnect_error;
  }

  void SetNoDisconnectResponse(bool no_disconnect_response) {
    no_disconnect_response_ = no_disconnect_response;
  }

  bool WasDisconnectCalled() { return was_disconnect_called_; }

  void SetError(bool has_gatt_connection_error) {
    has_gatt_connection_error_ = has_gatt_connection_error;
  }

  void SetHang(bool has_gatt_connection_hang,
               base::test::TaskEnvironment* task_environment) {
    has_gatt_connection_hang_ = has_gatt_connection_hang;
    task_environment_ = task_environment;
  }

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  bool was_disconnect_called_ = false;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothGattConnection>>
      gatt_connection_;
  bool has_gatt_connection_error_ = false;
  bool has_gatt_connection_hang_ = false;
  bool has_disconnect_error_ = false;
  bool no_disconnect_response_ = false;
  raw_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
  raw_ptr<ash::quick_pair::FakeBluetoothAdapter> fake_adapter_ = nullptr;
};

class FakeBluetoothGattCharacteristic
    : public testing::NiceMock<device::MockBluetoothGattCharacteristic> {
 public:
  FakeBluetoothGattCharacteristic(device::MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const device::BluetoothUUID& uuid,
                                  Properties properties,
                                  Permissions permissions)
      : testing::NiceMock<device::MockBluetoothGattCharacteristic>(
            service,
            identifier,
            uuid,
            properties,
            permissions) {}

  // Move-only class
  FakeBluetoothGattCharacteristic(const FakeBluetoothGattCharacteristic&) =
      delete;
  FakeBluetoothGattCharacteristic operator=(
      const FakeBluetoothGattCharacteristic&) = delete;

  void StartNotifySession(NotifySessionCallback callback,
                          ErrorCallback error_callback) override {
    if (notify_session_error_) {
      std::move(error_callback)
          .Run(device::BluetoothGattService::GattErrorCode::kNotPermitted);
      return;
    }

    auto fake_notify_session = std::make_unique<
        testing::NiceMock<device::MockBluetoothGattNotifySession>>(
        GetWeakPtr());

    if (notify_timeout_) {
      task_environment_->FastForwardBy(kConnectingTestTimeout);
      return;
    }

    std::move(callback).Run(std::move(fake_notify_session));
  }

  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 WriteType write_type,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override {
    if (write_remote_error_) {
      std::move(error_callback)
          .Run(device::BluetoothGattService::GattErrorCode::kNotPermitted);
      return;
    }

    if (write_timeout_) {
      task_environment_->FastForwardBy(kConnectingTestTimeout);
      return;
    }

    std::move(callback).Run();
  }

  void SetWriteError(bool write_remote_error) {
    write_remote_error_ = write_remote_error;
  }

  void SetWriteTimeout(bool write_timeout,
                       base::test::TaskEnvironment* task_environment) {
    write_timeout_ = write_timeout;
    task_environment_ = task_environment;
  }

  void SetNotifySessionError(bool notify_session_error) {
    notify_session_error_ = notify_session_error;
  }

  void SetNotifySessionTimeout(bool timeout,
                               base::test::TaskEnvironment* task_environment) {
    notify_timeout_ = timeout;
    task_environment_ = task_environment;
  }

 private:
  bool notify_session_error_ = false;
  bool write_remote_error_ = false;
  bool notify_timeout_ = false;
  bool write_timeout_ = false;
  raw_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
};

std::unique_ptr<FakeBluetoothDevice> CreateTestBluetoothDevice(
    ash::quick_pair::FakeBluetoothAdapter* adapter,
    device::BluetoothUUID uuid) {
  auto mock_device = std::make_unique<FakeBluetoothDevice>(
      /*adapter=*/adapter, kTestBleDeviceAddress);
  mock_device->SetPaired(false);
  mock_device->AddUUID(uuid);
  mock_device->SetServiceDataForUUID(uuid, {1, 2, 3});
  return mock_device;
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairGattServiceClientTest : public testing::Test {
 public:
  void SetUp() override { fast_pair_data_encryptor_->public_key(kPublicKey); }

  void SuccessfulGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void FailedGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    unique_fake_bt_device_->SetError(true);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void HungGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    unique_fake_bt_device_->SetHang(true, &task_environment_);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void DisconectFailGattSetup() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    unique_fake_bt_device_->SetDisconnectError(true);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void NoDisconectResponseGattSetup() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    unique_fake_bt_device_->SetDisconnectError(true);
    unique_fake_bt_device_->SetNoDisconnectResponse(true);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void NonFastPairServiceDataSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    unique_fake_bt_device_ =
        CreateTestBluetoothDevice(adapter_.get(), kNonFastPairUuid);
    raw_fake_bt_device_ = unique_fake_bt_device_.get();
    adapter_->AddMockDevice(std::move(unique_fake_bt_device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void SetGattServiceCharacteristics() {
    if (!keybased_char_error_) {
      fake_key_based_characteristic_ =
          std::make_unique<FakeBluetoothGattCharacteristic>(
              gatt_service_.get(), kUUIDString1, kKeyBasedCharacteristicUuid1,
              kProperties, kPermissions);

      if (keybased_notify_session_error_)
        fake_key_based_characteristic_->SetNotifySessionError(true);

      if (keybased_notify_session_timeout_) {
        fake_key_based_characteristic_->SetNotifySessionTimeout(
            true, &task_environment_);
      }

      if (key_based_write_error_) {
        fake_key_based_characteristic_->SetWriteError(true);
      }

      if (key_based_write_timeout_) {
        fake_key_based_characteristic_->SetWriteTimeout(true,
                                                        &task_environment_);
      }

      temp_fake_key_based_characteristic_ =
          fake_key_based_characteristic_.get();
      gatt_service_->AddMockCharacteristic(
          std::move(fake_key_based_characteristic_));
    }

    if (!passkey_char_error_) {
      fake_passkey_characteristic_ =
          std::make_unique<FakeBluetoothGattCharacteristic>(
              gatt_service_.get(), kUUIDString2, kPasskeyCharacteristicUuid1,
              kProperties, kPermissions);

      if (passkey_notify_session_error_)
        fake_passkey_characteristic_->SetNotifySessionError(true);

      if (passkey_notify_session_timeout_) {
        fake_passkey_characteristic_->SetNotifySessionTimeout(
            true, &task_environment_);
      }

      if (passkey_write_error_) {
        fake_passkey_characteristic_->SetWriteError(true);
      }

      if (passkey_write_timeout_) {
        fake_passkey_characteristic_->SetWriteTimeout(true, &task_environment_);
      }

      temp_passkey_based_characteristic_ = fake_passkey_characteristic_.get();

      gatt_service_->AddMockCharacteristic(
          std::move(fake_passkey_characteristic_));
    }

    auto fake_account_key_characteristic =
        std::make_unique<FakeBluetoothGattCharacteristic>(
            gatt_service_.get(), kUUIDString3, kAccountKeyCharacteristicUuid1,
            kProperties, kPermissions);
    if (account_key_write_error_) {
      fake_account_key_characteristic->SetWriteError(true);
    }

    if (write_account_key_timeout_) {
      fake_account_key_characteristic->SetWriteTimeout(true,
                                                       &task_environment_);
    }

    gatt_service_->AddMockCharacteristic(
        std::move(fake_account_key_characteristic));

    if (!model_id_char_error_) {
      auto fake_model_id_characteristic =
          std::make_unique<FakeBluetoothGattCharacteristic>(
              gatt_service_.get(), kUUIDString5, kModelIDCharacteristicUuid2,
              kProperties, kPermissions);
      gatt_service_->AddMockCharacteristic(
          std::move(fake_model_id_characteristic));
    }

    auto fake_additional_data_characteristic =
        std::make_unique<FakeBluetoothGattCharacteristic>(
            gatt_service_.get(), kUUIDString4,
            kAdditionalDataCharacteristicUuid2, kProperties, kPermissions);
    additional_data_characteristic_ = fake_additional_data_characteristic.get();
    gatt_service_->AddMockCharacteristic(
        std::move(fake_additional_data_characteristic));
  }

  void NotifyGattDiscoveryCompleteForService(const device::BluetoothUUID uuid) {
    auto gatt_service =
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            CreateTestBluetoothDevice(adapter_.get(), uuid).get(),
            kTestServiceId, uuid,
            /*is_primary=*/true);
    gatt_service_ = std::move(gatt_service);
    ON_CALL(*(gatt_service_.get()), GetDevice)
        .WillByDefault(
            testing::Return(adapter_->GetDevice(kTestBleDeviceAddress)));
    SetGattServiceCharacteristics();
    adapter_->NotifyGattDiscoveryCompleteForService(gatt_service_.get());
  }

  bool ServiceIsSet() {
    if (!gatt_service_client_->gatt_service())
      return false;
    return gatt_service_client_->gatt_service() == gatt_service_.get();
  }

  void SetPasskeyCharacteristicError(bool passkey_char_error) {
    passkey_char_error_ = passkey_char_error;
  }

  void SetKeybasedCharacteristicError(bool keybased_char_error) {
    keybased_char_error_ = keybased_char_error;
  }

  void SetAccountKeyCharacteristicWriteError(bool account_key_write_error) {
    account_key_write_error_ = account_key_write_error;
  }

  void SetPasskeyNotifySessionError(bool passkey_notify_session_error) {
    passkey_notify_session_error_ = passkey_notify_session_error;
  }

  void SetKeybasedNotifySessionError(bool keybased_notify_session_error) {
    keybased_notify_session_error_ = keybased_notify_session_error;
  }

  void SetModelIdCharacteristicError(bool model_id_char_error) {
    model_id_char_error_ = model_id_char_error;
  }

  void InitializedTestCallback(std::optional<PairFailure> failure) {
    initalized_failure_ = failure;
  }

  std::optional<PairFailure> GetInitializedCallbackResult() {
    return initalized_failure_;
  }

  void ReadModelIdCallback(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value) {
    read_failure_ = error_code;
  }

  void WriteTestCallback(std::vector<uint8_t> response,
                         std::optional<PairFailure> failure) {
    write_failure_ = failure;
  }

  void AccountKeyCallback(
      std::optional<ash::quick_pair::AccountKeyFailure> failure) {
    account_key_error_ = failure;
    if (failure.has_value()) {
      gatt_service_client_.reset();
    }
  }

  std::optional<ash::quick_pair::AccountKeyFailure> GetAccountKeyCallback() {
    return account_key_error_;
  }

  std::optional<PairFailure> GetWriteCallbackResult() { return write_failure_; }

  std::optional<device::BluetoothGattService::GattErrorCode>
  GetReadCallbackResult() {
    return read_failure_;
  }

  void SetPasskeyNotifySessionTimeout(bool timeout) {
    passkey_notify_session_timeout_ = timeout;
  }

  void SetKeybasedNotifySessionTimeout(bool timeout) {
    keybased_notify_session_timeout_ = timeout;
  }

  void SetWriteAccountKeyTimeout(bool timeout) {
    write_account_key_timeout_ = timeout;
  }

  void FastForwardTimeByConnectingTimeout() {
    task_environment_.FastForwardBy(kConnectingTestTimeout);
  }

  void FastForwardTimeByGattDisconnectCoolOff() {
    task_environment_.FastForwardBy(
        kCoolOffPeriodBeforeGattConnectionAfterDisconnect);
  }

  void FastForwardTimeByGattDisconnectResponseTimeout() {
    task_environment_.FastForwardBy(kDisconnectResponseTimeout);
  }

  void FastForwardTimeByAllGattRetries() {
    task_environment_.FastForwardBy(kAllGattRetriesPeriod);
  }

  void ReadModelId() {
    gatt_service_client_->ReadModelIdAsync(base::BindRepeating(
        &::ash::quick_pair::FastPairGattServiceClientTest::ReadModelIdCallback,
        weak_ptr_factory_.GetWeakPtr()));
  }

  void WriteRequestToKeyBased() {
    gatt_service_client_->WriteRequestAsync(
        kMessageType, kFlags, kProviderAddress, kSeekersAddress,
        fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                WriteTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void WriteRequestToPasskey() {
    gatt_service_client_->WritePasskeyAsync(
        kSeekerPasskey, kPasskey, fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                WriteTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void WriteAccountKey() {
    gatt_service_client_->WriteAccountKey(
        kAccountKey, fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                AccountKeyCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void TriggerKeyBasedGattChanged() {
    adapter_->NotifyGattCharacteristicValueChanged(
        temp_fake_key_based_characteristic_);
  }

  void TriggerPasskeyGattChanged() {
    adapter_->NotifyGattCharacteristicValueChanged(
        temp_passkey_based_characteristic_);
  }

  void WritePersonalizedName(const std::string& name) {
    gatt_service_client_->WritePersonalizedName(
        name, kProviderAddress, fast_pair_data_encryptor_.get(),
        write_additional_data_callback_.Get());
  }

  std::optional<PairFailure> GetAdditionalDataWriteResult() {
    return additional_data_failure_;
  }

  void SetKeyBasedWriteError() { key_based_write_error_ = true; }

  void SetPasskeyWriteError() { passkey_write_error_ = true; }

  void SetWriteRequestTimeout() { key_based_write_timeout_ = true; }

  void SetWritePasskeyTimeout() { passkey_write_timeout_ = true; }

  void SetAdditionalDataWriteError(bool error) {
    additional_data_characteristic_->SetWriteError(error);
  }

  void SetAdditionalDataWriteTimeout(bool is_timeout) {
    additional_data_characteristic_->SetWriteTimeout(is_timeout,
                                                     &task_environment_);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::MockCallback<base::OnceCallback<void(std::optional<PairFailure>)>>
      write_additional_data_callback_;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  raw_ptr<FakeBluetoothDevice, DanglingUntriaged> raw_fake_bt_device_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;

 private:
  // We need temporary pointers to use for write/ready requests because we
  // move the unique pointers when we notify the session.
  raw_ptr<FakeBluetoothGattCharacteristic, DanglingUntriaged>
      temp_fake_key_based_characteristic_;
  raw_ptr<FakeBluetoothGattCharacteristic, DanglingUntriaged>
      temp_passkey_based_characteristic_;
  std::optional<ash::quick_pair::AccountKeyFailure> account_key_error_ =
      std::nullopt;
  bool passkey_char_error_ = false;
  bool keybased_char_error_ = false;
  bool account_key_write_error_ = false;
  bool passkey_notify_session_error_ = false;
  bool keybased_notify_session_error_ = false;
  bool passkey_notify_session_timeout_ = false;
  bool keybased_notify_session_timeout_ = false;
  bool key_based_write_error_ = false;
  bool key_based_write_timeout_ = false;
  bool passkey_write_error_ = false;
  bool passkey_write_timeout_ = false;
  bool write_account_key_timeout_ = false;
  bool model_id_char_error_ = false;

  std::optional<PairFailure> initalized_failure_;
  std::optional<PairFailure> write_failure_;
  std::optional<PairFailure> additional_data_failure_;

  std::optional<device::BluetoothGattService::GattErrorCode> read_failure_;

  std::unique_ptr<FakeBluetoothDevice> unique_fake_bt_device_;
  std::unique_ptr<FakeBluetoothGattCharacteristic>
      fake_key_based_characteristic_;
  raw_ptr<FakeBluetoothGattCharacteristic, DanglingUntriaged>
      additional_data_characteristic_;
  std::unique_ptr<FakeFastPairDataEncryptor> fast_pair_data_encryptor_ =
      std::make_unique<FakeFastPairDataEncryptor>();
  std::unique_ptr<FakeBluetoothGattCharacteristic> fake_passkey_characteristic_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
      gatt_service_;
  base::WeakPtrFactory<FastPairGattServiceClientTest> weak_ptr_factory_{this};
};

TEST_F(FastPairGattServiceClientTest, FailedGattConnection) {
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  FailedGattConnectionSetUp();
  FastForwardTimeByAllGattRetries();
  histogram_tester_.ExpectBucketCount(
      kFastPairGattRetryFailureReason,
      PairFailure::kBluetoothDeviceFailureCreatingGattConnection, 3);
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 1);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 3);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 3);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest,
       GattConnectionSuccess_HandshakeRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_TRUE(gatt_service_client_->IsConnected());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 1);
  histogram_tester().ExpectTotalCount(kGattServiceDiscoveryTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest,
       GattConnectionSuccess_HandshakeRefactorEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_TRUE(gatt_service_client_->IsConnected());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionEffectiveSuccessRate, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionAttemptCount, 1);
  histogram_tester().ExpectTotalCount(kGattServiceDiscoveryTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, IgnoreNonFastPairServices) {
  NonFastPairServiceDataSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, FailedKeyBasedCharacteristics) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SetKeybasedCharacteristicError(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 2);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, FailedPasskeyCharacteristics) {
  SetPasskeyCharacteristicError(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPasskeyCharacteristicDiscovery);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest,
       SuccessfulCharacteristicsStartNotify_HandshakeRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SetKeybasedCharacteristicError(false);
  SetPasskeyCharacteristicError(false);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToPasskey();
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 5);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest,
       SuccessfulCharacteristicsStartNotify_HandshakeRefactorEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SetKeybasedCharacteristicError(false);
  SetPasskeyCharacteristicError(false);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToPasskey();
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 5);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, StartNotifyPasskeyFailure) {
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  SetPasskeyNotifySessionError(true);
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToPasskey();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyCharacteristicNotifySession);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, StartNotifyKeybasedFailure) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  SetKeybasedNotifySessionError(true);
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToKeyBased();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicNotifySession);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, PasskeyStartNotifyTimeout) {
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SetPasskeyNotifySessionTimeout(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToPasskey();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyCharacteristicNotifySessionTimeout);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, KeyBasedStartNotifyTimeout) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SetKeybasedNotifySessionTimeout(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  WriteRequestToKeyBased();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest,
       WriteKeyBasedRequest_HandshakeRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedNotify, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedWriteRequest, 1);
}

TEST_F(FastPairGattServiceClientTest,
       WriteKeyBasedRequest_HandshakeRefactorEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  histogram_tester().ExpectTotalCount(kFastPairGattConnectionStep, 4);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedNotify, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedWriteRequest, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteKeyBasedRequestError) {
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  SetKeyBasedWriteError();
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteKeyBasedRequestTimeout) {
  SetWriteRequestTimeout();
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_TRUE(ServiceIsSet());
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingResponseTimeout);
}
TEST_F(FastPairGattServiceClientTest,
       WritePasskeyRequest_HandshakeRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerPasskeyGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyNotify, 1);
  histogram_tester().ExpectTotalCount(kPasskeyWriteRequest, 1);
}

TEST_F(FastPairGattServiceClientTest,
       WritePasskeyRequest_HandshakeRefactorEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerPasskeyGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyNotify, 1);
  histogram_tester().ExpectTotalCount(kPasskeyWriteRequest, 1);
}

TEST_F(FastPairGattServiceClientTest, WritePasskeyRequestError) {
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  SetPasskeyWriteError();
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 1);
}

TEST_F(FastPairGattServiceClientTest, WritePasskeyRequestTimeout) {
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  SetWritePasskeyTimeout();
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kPasskeyResponseTimeout);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
}

TEST_F(FastPairGattServiceClientTest,
       WriteAccountKey_HandshakeRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  WriteAccountKey();
  EXPECT_EQ(GetAccountKeyCallback(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 1);
}

TEST_F(FastPairGattServiceClientTest,
       WriteAccountKey_HandshakeRefactorEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kFastPairHandshakeLongTermRefactor);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  WriteAccountKey();
  EXPECT_EQ(GetAccountKeyCallback(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteAccountKeyFailure) {
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SetAccountKeyCharacteristicWriteError(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  WriteAccountKey();
  EXPECT_NE(GetAccountKeyCallback(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      1);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
}

TEST_F(FastPairGattServiceClientTest, WriteAccountKeyTimeout) {
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SetWriteAccountKeyTimeout(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  WriteAccountKey();
  EXPECT_NE(GetAccountKeyCallback(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
}

TEST_F(FastPairGattServiceClientTest, TimeoutOnNonFastPairServiceDiscovery) {
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();

  NotifyGattDiscoveryCompleteForService(kNonFastPairUuid);

  // Simulate all the GATT retries timing out following
  // `NotifyGattCompleteForService` on an invalid UUID because the timeout have
  // fired (not stopped because invalid UUID), which cause us to retry.
  FastForwardTimeByAllGattRetries();

  histogram_tester_.ExpectBucketCount(kFastPairGattRetryFailureReason,
                                      PairFailure::kGattServiceDiscoveryTimeout,
                                      3);
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
}

TEST_F(FastPairGattServiceClientTest, HungGattConnectionTimesOut) {
  HungGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  histogram_tester_.ExpectBucketCount(kFastPairGattRetryFailureReason,
                                      PairFailure::kGattServiceDiscoveryTimeout,
                                      3);
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, FailedToDisconnectGattResultsInError) {
  DisconectFailGattSetup();
  FastForwardTimeByGattDisconnectCoolOff();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kFailureToDisconnectGattBetweenRetries);
  EXPECT_FALSE(ServiceIsSet());
  EXPECT_TRUE(raw_fake_bt_device_->WasDisconnectCalled());
}

TEST_F(FastPairGattServiceClientTest,
       FailedToRecieveDisconnectResponseAfterGattFailure) {
  DisconectFailGattSetup();
  FastForwardTimeByGattDisconnectResponseTimeout();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kFailureToDisconnectGattBetweenRetries);
  EXPECT_FALSE(ServiceIsSet());
  EXPECT_TRUE(raw_fake_bt_device_->WasDisconnectCalled());
}

TEST_F(FastPairGattServiceClientTest, SuccessfulGattConnectionDisconnects) {
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_TRUE(gatt_service_client_->IsConnected());
  EXPECT_TRUE(raw_fake_bt_device_->WasDisconnectCalled());
}

TEST_F(FastPairGattServiceClientTest, PairingDeviceLostBetweenRetries) {
  FailedGattConnectionSetUp();
  adapter_->RemoveMockDevice(kTestBleDeviceAddress);
  FastForwardTimeByGattDisconnectCoolOff();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, PersonalizedNameWriteSuccess) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_CALL(write_additional_data_callback_, Run(testing::Eq(std::nullopt)))
      .Times(1);
  WritePersonalizedName(kPersonalizedName);
}

TEST_F(FastPairGattServiceClientTest,
       PersonalizedNameWrite_AdditionalDataCharacteristicWriteError) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  SetAdditionalDataWriteError(true);
  EXPECT_CALL(write_additional_data_callback_,
              Run(testing::Eq(PairFailure::kAdditionalDataCharacteristicWrite)))
      .Times(1);
  WritePersonalizedName(kPersonalizedName);
}

TEST_F(FastPairGattServiceClientTest,
       kPersonalizedNameWrite_AdditionalDataCharacteristicWriteTimeout) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  SetAdditionalDataWriteTimeout(true);
  EXPECT_CALL(
      write_additional_data_callback_,
      Run(testing::Eq(PairFailure::kAdditionalDataCharacteristicWriteTimeout)))
      .Times(1);
  WritePersonalizedName(kPersonalizedName);
}

TEST_F(FastPairGattServiceClientTest, WriteEmptyPersonalizedName) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  const std::string empty = "";
  EXPECT_CALL(write_additional_data_callback_, Run(testing::Eq(std::nullopt)))
      .Times(1);
  WritePersonalizedName(empty);
}

TEST_F(FastPairGattServiceClientTest, SuccessfulReadModelId) {
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  ReadModelId();
  EXPECT_EQ(GetReadCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedReadModelId) {
  SetModelIdCharacteristicError(true);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByGattDisconnectCoolOff();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  ReadModelId();
  EXPECT_EQ(GetReadCallbackResult(),
            device::BluetoothGattService::GattErrorCode::kNotSupported);
}

// Regression test for b/300596153
TEST_F(FastPairGattServiceClientTest,
       NoCrashWhenGattDiscoveryCompleteForServiceCalledTwice) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
  NotifyGattDiscoveryCompleteForService(
      ash::quick_pair::kFastPairBluetoothUuid);
}

}  // namespace quick_pair
}  // namespace ash
