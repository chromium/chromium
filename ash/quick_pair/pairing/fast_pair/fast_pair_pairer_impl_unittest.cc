// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr base::TimeDelta kCreateBondTimeout = base::Seconds(15);

const std::vector<uint8_t> kResponseBytes = {0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
                                             0x32, 0x1D, 0xA0, 0xBA, 0xF0, 0xBB,
                                             0x95, 0x1F, 0xF7, 0xB6};
std::array<uint8_t, 12> kPasskeySaltBytes = {
    0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6, 0xBA, 0xF0, 0xBB, 0xB6, 0xBA, 0xF0};

const std::array<uint8_t, 64> kPublicKey = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F,
    0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
    0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01,
    0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45,
    0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32,
    0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

const uint8_t kValidPasskey = 13;
const uint8_t kInvalidPasskey = 9;

constexpr char kMetadataId[] = "test_metadata_id";
constexpr char kDeviceName[] = "test_device_name";
constexpr char kBluetoothCanonicalizedAddress[] = "0C:0E:4C:C8:05:08";
const std::string kUserEmail = "test@test.test";

const char kWritePasskeyCharacteristicResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.Result";
const char kWritePasskeyCharacteristicPairFailureMetric[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.PairFailure";
const char kPasskeyCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Result";
const char kWriteAccountKeyCharacteristicResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result";
const char kConnectDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.ConnectDevice.Result";
const char kPairDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.Result";
const char kPairDeviceErrorReason[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.ErrorReason";
const char kSavedDeviceUpdateOptInStatusInitialResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "InitialPairingProtocol";
const char kSavedDeviceUpdateOptInStatusRetroactiveResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "RetroactivePairingProtocol";
const char kSavedDeviceUpdateOptInStatusSubsequentResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "SubsequentPairingProtocol";
constexpr char kInitialSuccessFunnelMetric[] = "FastPair.InitialPairing";
constexpr char kProtocolPairingStepInitial[] =
    "FastPair.InitialPairing.Pairing";
constexpr char kProtocolPairingStepSubsequent[] =
    "FastPair.SubsequentPairing.Pairing";
constexpr char kInitializePairingProcessInitial[] =
    "FastPair.InitialPairing.Initialization";
const char kCreateBondTime[] = "FastPair.CreateBond.Latency";
const char kEngagementFlowInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  explicit FakeBluetoothDevice(ash::quick_pair::FakeBluetoothAdapter* adapter)
      : testing::NiceMock<device::MockBluetoothDevice>(
            adapter,
            0,
            kDeviceName,
            kBluetoothCanonicalizedAddress,
            /*paired=*/false,
            /*connected*/ false),
        fake_adapter_(adapter) {}

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

  void Pair(BluetoothDevice::PairingDelegate* pairing_delegate,
            base::OnceCallback<void(std::optional<ConnectErrorCode> error_code)>
                callback) override {
    if (pair_failure_) {
      std::move(callback).Run(ConnectErrorCode::ERROR_FAILED);
      return;
    }

    if (pair_timeout_) {
      return;
    }

    pair_callback_ = std::move(callback);
  }

  void TriggerPairCallback() {
    ASSERT_TRUE(pair_callback_);
    std::move(pair_callback_).Run(/*error_code=*/std::nullopt);
  }

  void Connect(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceCallback<void(std::optional<ConnectErrorCode> error_code)>
          callback) override {
    if (connect_failure_) {
      std::move(callback).Run(ConnectErrorCode::ERROR_FAILED);
      return;
    }

    if (connect_timeout_) {
      return;
    }

    std::move(callback).Run(std::nullopt);
  }

  void ConnectClassic(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceCallback<void(std::optional<ConnectErrorCode> error_code)>
          callback) override {
    is_device_classic_paired = true;
    if (floss::features::IsFlossEnabled()) {
      // On Floss, ConnectClassic is equivalent to Pair
      Pair(pairing_delegate, std::move(callback));
      return;
    }
    Connect(pairing_delegate, std::move(callback));
  }

  // This method is called in DevicePairedChanged to ensure we are setting the
  // classic address only if the device's address has the correct type (public).
  device::BluetoothDevice::AddressType GetAddressType() const override {
    return device::BluetoothDevice::AddressType::ADDR_TYPE_PUBLIC;
  }

  void SetPairFailure() { pair_failure_ = true; }

  void SetPairTimeout() { pair_timeout_ = true; }

  void SetConnectFailure() { connect_failure_ = true; }

  void SetConnectTimeout() { connect_timeout_ = true; }

  void ConfirmPairing() override { is_device_paired_ = true; }

  bool IsDevicePaired() { return is_device_paired_; }

  bool IsDeviceClassicPaired() { return is_device_classic_paired; }

 protected:
  base::OnceCallback<void(std::optional<ConnectErrorCode> error_code)>
      pair_callback_;
  raw_ptr<ash::quick_pair::FakeBluetoothAdapter> fake_adapter_;
  bool pair_failure_ = false;
  bool pair_timeout_ = false;
  bool connect_failure_ = false;
  bool connect_timeout_ = false;
  bool is_device_paired_ = false;
  bool is_device_classic_paired = false;
};

class FakeFastPairGattServiceClientImplFactory
    : public ash::quick_pair::FastPairGattServiceClientImpl::Factory {
 public:
  ~FakeFastPairGattServiceClientImplFactory() override = default;

  ash::quick_pair::FakeFastPairGattServiceClient*
  fake_fast_pair_gatt_service_client() {
    return fake_fast_pair_gatt_service_client_;
  }

 private:
  // FastPairGattServiceClientImpl::Factory:
  std::unique_ptr<ash::quick_pair::FastPairGattServiceClient> CreateInstance(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(std::optional<ash::quick_pair::PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<ash::quick_pair::FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  raw_ptr<ash::quick_pair::FakeFastPairGattServiceClient, DanglingUntriaged>
      fake_fast_pair_gatt_service_client_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

// For convenience.
using ::testing::Return;

class FastPairPairerImplTest : public AshTestBase {
 public:
  FastPairPairerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);
    FastPairHandshakeLookup::UseFakeInstance();

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    fast_pair_repository_ = std::make_unique<FakeFastPairRepository>();

    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        fake_bluetooth_device_ptr_, adapter_.get(), base::DoNothing());

    // We have to pass in a unique_ptr when we create a Handshake, however
    // we also want to be able to set fake responses on the encryptor. Thus
    // we maintain 2 pointers. We won't touch data_encryptor_unique_ aside
    // from CreateHandshake.
    data_encryptor_unique_ = std::make_unique<FakeFastPairDataEncryptor>();
    data_encryptor_ =
        static_cast<FakeFastPairDataEncryptor*>(data_encryptor_unique_.get());
  }

  void TearDown() override {
    fast_pair_repository_.reset();
    pairer_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void CreateMockDevice(DeviceFastPairVersion version, Protocol protocol) {
    device_ = base::MakeRefCounted<Device>(
        kMetadataId, kBluetoothCanonicalizedAddress, protocol);
    // TODO(b/268722180): classic address should be set in the handshake, which
    // is only for V2 devices. Should refactor that for unit tests.
    device_->set_classic_address(kBluetoothCanonicalizedAddress);

    device_->set_version(version);

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    fake_bluetooth_device_ =
        std::make_unique<FakeBluetoothDevice>(adapter_.get());
    fake_bluetooth_device_ptr_ = fake_bluetooth_device_.get();

    adapter_->AddMockDevice(std::move(fake_bluetooth_device_));
  }

  void AddConnectedHandshake() {
    FakeFastPairHandshakeLookup::GetFakeInstance()->CreateForTesting(
        adapter_, device_, base::DoNothing(), nullptr,
        std::move(data_encryptor_unique_));

    // Add fake GATT service client to the lookup class. In normal
    // flow this is usually done when handshake is created.
    FastPairGattServiceClientLookup::GetInstance()->InsertFakeForTesting(
        fake_bluetooth_device_ptr_, std::move(gatt_service_client_));
  }

  void EraseHandshake() {
    FastPairHandshakeLookup::GetInstance()->Erase(device_);
  }

  void SetHandshakeBleCallback() {
    auto* handshake = FastPairHandshakeLookup::GetInstance()->Get(device_);
    handshake->BleAddressRotated(
        base::BindOnce(&FastPairPairerImplTest::on_ble_rotation_test_callback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void SetDecryptPasskeyForIncorrectMessageType(FastPairMessageType type) {
    DecryptedPasskey passkey(type, kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForPasskeyMismatch() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kInvalidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForNoPasskey() {
    data_encryptor_->passkey(std::nullopt);
  }

  void SetDecryptPasskeyForSuccess() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void RunWritePasskeyCallback(
      std::vector<uint8_t> data,
      std::optional<PairFailure> failure = std::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWritePasskeyCallback(data, failure);
  }

  void RunWriteAccountKeyCallback(
      std::optional<AccountKeyFailure> failure = std::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWriteAccountKeyCallback(failure);
  }

  void PairFailedCallback(scoped_refptr<Device> device, PairFailure failure) {
    failure_ = failure;
  }

  void NotifyConfirmPasskey() {
    adapter_->NotifyConfirmPasskey(kValidPasskey, fake_bluetooth_device_ptr_);
  }

  std::optional<PairFailure> GetPairFailure() { return failure_; }

  void SetPairFailure() { fake_bluetooth_device_ptr_->SetPairFailure(); }

  void SetConnectFailureAfterPair() {
    fake_bluetooth_device_ptr_->SetConnectFailure();
  }

  // Causes FakeBluetoothDevice::Pair() to hang instead of triggering either a
  // success or a failure callback.
  void SetPairTimeout() { fake_bluetooth_device_ptr_->SetPairTimeout(); }

  // Causes FakeBluetoothDevice::Connect() to hang instead of triggering either
  // a success or a failure callback.
  void SetConnectTimeoutAfterPair() {
    fake_bluetooth_device_ptr_->SetConnectTimeout();
  }

  void SetConnectFailure() { adapter_->SetConnectFailure(); }

  // Causes FakeBluetoothAdapter::ConnectDevice() to hang instead of triggering
  // either a success or a failure callback.
  void SetConnectDeviceTimeout() { adapter_->SetConnectDeviceTimeout(); }

  void SetGetDeviceNullptr() { adapter_->SetGetDeviceNullptr(); }

  bool IsDevicePaired() { return fake_bluetooth_device_ptr_->IsDevicePaired(); }

  bool IsDeviceClassicPaired() {
    return fake_bluetooth_device_ptr_->IsDeviceClassicPaired();
  }

  bool IsAccountKeySavedToFootprints() {
    return fast_pair_repository_->HasKeyForDevice(
        fake_bluetooth_device_ptr_->GetAddress());
  }

  bool IsDisplayNameSavedToFootprints() {
    return fast_pair_repository_->HasNameForDevice(
        fake_bluetooth_device_ptr_->GetAddress());
  }

  void SetPublicKey() { data_encryptor_->public_key(kPublicKey); }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void DeviceUnpaired() {
    adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, false);
  }

  void DevicePaired() {
    adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  }

  void NotifyDisplayPasskey() {
    adapter_->NotifyDisplayPasskey(fake_bluetooth_device_ptr_, kValidPasskey);
  }

  template <typename T>
  void ExpectStepMetrics(std::string metric, std::vector<T> steps) {
    histogram_tester().ExpectTotalCount(metric, steps.size());
    for (T step : steps) {
      histogram_tester().ExpectBucketCount(metric, step, 1);
    }
  }

 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairerImpl>(
        adapter_, device_, paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), display_passkey_.Get(),
        pairing_procedure_complete_.Get());
  }

  void CreatePairerAsFactory() {
    pairer_ = FastPairPairerImpl::Factory::Create(
        adapter_, device_, paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), display_passkey_.Get(),
        pairing_procedure_complete_.Get());
  }

  void CreateDevice(DeviceFastPairVersion version) {
    CreateMockDevice(version,
                     /*protocol=*/Protocol::kFastPairInitial);

    // Adds a connected handshake that has completed successfully in
    // 'FastPairHandshakeLookup' for the mock device.
    SetGetDeviceNullptr();
    AddConnectedHandshake();
    CreatePairer();
    if (version == DeviceFastPairVersion::kHigherThanV1) {
      SetPublicKey();
      EXPECT_EQ(GetPairFailure(), std::nullopt);
      EXPECT_CALL(paired_callback_, Run);
      SetDecryptPasskeyForSuccess();
      NotifyConfirmPasskey();
    }
  }

  void PerformAndCheckSuccessfulPairingCallbacks() {
    RunWritePasskeyCallback(kResponseBytes);
    EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
    EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
    adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
    RunWriteAccountKeyCallback();
  }

  void on_ble_rotation_test_callback() {
    on_ble_rotation_callback_called_ = true;
  }

  bool set_handshake_completed_successfully_ = false;
  bool on_ble_rotation_callback_called_ = false;
  std::optional<PairFailure> failure_ = std::nullopt;
  std::unique_ptr<FakeBluetoothDevice> fake_bluetooth_device_;
  raw_ptr<FakeBluetoothDevice, DanglingUntriaged> fake_bluetooth_device_ptr_ =
      nullptr;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      handshake_complete_callback_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      paired_callback_;
  base::MockCallback<
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>>
      account_key_failure_callback_;
  base::MockCallback<base::OnceCallback<void(std::u16string, uint32_t)>>
      display_passkey_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      pairing_procedure_complete_;
  std::unique_ptr<FakeFastPairRepository> fast_pair_repository_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  std::unique_ptr<FakeFastPairDataEncryptor> data_encryptor_unique_;
  raw_ptr<FakeFastPairDataEncryptor, DanglingUntriaged> data_encryptor_ =
      nullptr;
  raw_ptr<FakeFastPairHandshake, DanglingUntriaged> fake_fast_pair_handshake_ =
      nullptr;
  std::unique_ptr<FastPairPairer> pairer_;
  base::WeakPtrFactory<FastPairPairerImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Retroactive) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_ConnectFailure_Initial) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetConnectFailureAfterPair();
  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), PairFailure::kFailedToConnectAfterPairing);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_ConnectFailure_Subsequent) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  SetConnectFailureAfterPair();
  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), PairFailure::kFailedToConnectAfterPairing);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Initial) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Initial_CancelsPairing) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  AddConnectedHandshake();
  CreatePairer();

  // Mock that the device was paired unsuccessfully.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsPaired()).WillOnce(Return(false));

  // Check to make sure that, when pairing fails, we call CancelPairing.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, CancelPairing()).Times(1);
}

TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Subsequent) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  SetPairFailure();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
  histogram_tester().ExpectTotalCount(kCreateBondTime, 1);
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Initial_Floss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {floss::features::kFlossEnabled},
      /*disabled_features=*/{});

  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDeviceClassicPaired());
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
  histogram_tester().ExpectTotalCount(kCreateBondTime, 1);
}

TEST_F(FastPairPairerImplTest, PairByBLEDeviceSuccess_Initial) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::kFastPairKeyboards, floss::features::kFlossEnabled},
      /*disabled_features=*/{});

  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // Mock the BLE device having an invalid classic address
  device_->set_classic_address("00:00:00:00:00:00");
  // Pairing flags to indicate the provider prefers LE bonding
  device_->set_key_based_pairing_flags(0x40);

  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_FALSE(IsDeviceClassicPaired());
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
  histogram_tester().ExpectTotalCount(kCreateBondTime, 1);
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Initial_AlreadyClassicPaired) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();

  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(paired_callback_, Run);
  CreatePairer();

  EXPECT_EQ(GetPairFailure(), std::nullopt);

  // For an already classic paired device, we skip right to Account Key writing.
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Initial_AlreadyClassicPaired_FailureToConnect) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetConnectFailureAfterPair();
  AddConnectedHandshake();

  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsConnected())
      .WillOnce(Return(false));

  EXPECT_CALL(paired_callback_, Run).Times(0);
  CreatePairer();

  // Since connecting fails, we should not complete the procedure.
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(0);

  EXPECT_EQ(GetPairFailure(), PairFailure::kFailedToConnectAfterPairing);

  // Verify that we already paired, but do not emit the connected event metric.
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete});
  ExpectStepMetrics<FastPairEngagementFlowEvent>(
      kEngagementFlowInitialMetric,
      {FastPairEngagementFlowEvent::kPairingSucceededAlreadyPaired});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Initial_AlreadyClassicPaired_Disconnected) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();

  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsConnected())
      .WillOnce(Return(false));
  EXPECT_CALL(paired_callback_, Run);
  CreatePairer();

  EXPECT_EQ(GetPairFailure(), std::nullopt);

  // For an already classic paired device, we skip right to Account Key writing.
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Initial_AlreadyFastPaired) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();

  // Mock that the device is already fast paired (and saved to Footprints).
  fast_pair_repository_->SaveMacAddressToAccount(
      kBluetoothCanonicalizedAddress);
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  // For an already fast paired device, we skip the Account Key writing.
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  CreatePairer();

  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kInitialSuccessFunnelMetric,
          FastPairInitialSuccessFunnelEvent::kDeviceAlreadyAssociatedToAccount),
      1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Subsequent_AlreadyClassicPaired) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  AddConnectedHandshake();

  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  EXPECT_CALL(paired_callback_, Run);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Subsequent_AlreadyFastPaired) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  AddConnectedHandshake();

  // Mock that the device is already fast paired (and saved to Footprints).
  fast_pair_repository_->SaveMacAddressToAccount(
      kBluetoothCanonicalizedAddress);
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  // For an already fast paired device, we skip the Account Key writing.
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kAlreadyPaired,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  AddConnectedHandshake();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, ConnectFailure_Initial) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetConnectFailure();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();

  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, ConnectFailure_Subsequent) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  SetConnectFailure();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, ConnectSuccess_Initial) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, ConnectSuccess_Subsequent) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Initial) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  NotifyConfirmPasskey();

  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Subsequent) {
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  NotifyConfirmPasskey();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Initial_SeekersPasskey) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kSeekersPasskey);
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingRequest) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingRequest);
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingResponse) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyNoPasskey) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForNoPasskey();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyDecryptFailure);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated});
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForSuccess();

  // This time, this helper function is used to make the device lost during
  // Passkey exchange.
  NotifyConfirmPasskey();
  SetGetDeviceNullptr();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated,
       FastPairProtocolPairingSteps::kPasskeyConfirmed});
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForSuccess();

  // This time, this helper function is used to make the device lost during
  // Passkey exchange.
  NotifyConfirmPasskey();
  SetGetDeviceNullptr();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kDeviceConnected,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated,
       FastPairProtocolPairingSteps::kPasskeyConfirmed});
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated,
       FastPairProtocolPairingSteps::kPasskeyConfirmed,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial_Floss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {floss::features::kFlossEnabled},
      /*disabled_features=*/{});

  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  // Floss calls Pair instead of finishing after ConnectDevice.
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepInitial,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated,
       FastPairProtocolPairingSteps::kPasskeyConfirmed,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, BleDeviceLostMidPair) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  SetDecryptPasskeyForSuccess();

  // Simulate BLE device being lost in the middle of pairing flow.
  EraseHandshake();

  NotifyConfirmPasskey();

  EXPECT_EQ(PairFailure::kBleDeviceLostMidPair, GetPairFailure());
  EXPECT_FALSE(IsDevicePaired());
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial_FactoryCreate) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairerAsFactory();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  ExpectStepMetrics<FastPairProtocolPairingSteps>(
      kProtocolPairingStepSubsequent,
      {FastPairProtocolPairingSteps::kPairingStarted,
       FastPairProtocolPairingSteps::kPairingComplete,
       FastPairProtocolPairingSteps::kPasskeyNegotiated,
       FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
       FastPairProtocolPairingSteps::kPasskeyValidated,
       FastPairProtocolPairingSteps::kPasskeyConfirmed,
       FastPairProtocolPairingSteps::kDeviceConnected});
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  EXPECT_TRUE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  EXPECT_TRUE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  EXPECT_TRUE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_GuestLoggedIn) {
  Login(user_manager::UserType::kGuest);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kGuestModeDetected),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_KioskAppLoggedIn) {
  Login(user_manager::UserType::kKioskApp);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_NotLoggedIn) {
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_Locked) {
  GetSessionControllerClient()->LockScreen();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());

  // With Subsequent pairing, the display name is not saved to Footprints.
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());

  // With Subsequent pairing, the display name is not saved to Footprints.
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());

  // With Subsequent pairing, the display name is not saved to Footprints.
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Initial_GattErrorFailed) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorFailed);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorUnknown) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorUnknown);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInProgress) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattInProgress);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInvalidLength) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorInvalidLength);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPermitted) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotPermitted);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotAuthorized) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotAuthorized);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPaired) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotPaired);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotSupported) {
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  Login(user_manager::UserType::kRegular);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotSupported);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  EXPECT_FALSE(IsDisplayNameSavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Initial_NoCancelPairing) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Mock that the device was paired successfully.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsPaired()).WillOnce(Return(true));
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorFailed);

  // Check to make sure that, after bonding a device, we don't cancel pairing
  // (since this causes a paired device to disconnect).
  EXPECT_CALL(*fake_bluetooth_device_ptr_, CancelPairing()).Times(0);
}

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DevicePaired) {
  Login(user_manager::UserType::kRegular);

  CreateDevice(DeviceFastPairVersion::kV1);
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kV1, device_->version().value());
  DevicePaired();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kPassedToPairingDialog),
            1);
}

TEST_F(FastPairPairerImplTest,
       FastPairVersionOne_SetsClassicAddressAfterPairing) {
  Login(user_manager::UserType::kRegular);
  CreateDevice(DeviceFastPairVersion::kV1);
  // V1 devices don't have classic addresses set during handshake.
  device_->set_classic_address(std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kV1, device_->version().value());
  DevicePaired();

  // After pairing, classic address should be set.
  EXPECT_TRUE(device_->classic_address());
  EXPECT_EQ(device_->classic_address(), kBluetoothCanonicalizedAddress);
}

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DeviceUnpaired) {
  Login(user_manager::UserType::kRegular);

  CreateDevice(DeviceFastPairVersion::kV1);
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
  EXPECT_CALL(paired_callback_, Run).Times(0);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(0);
  EXPECT_EQ(DeviceFastPairVersion::kV1, device_->version().value());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kV1DeviceDetected),
            1);
  DeviceUnpaired();
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedOut_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedIn_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedIn_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  PerformAndCheckSuccessfulPairingCallbacks();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedOut_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  PerformAndCheckSuccessfulPairingCallbacks();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedOut_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  PerformAndCheckSuccessfulPairingCallbacks();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_FlagEnabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);

  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_FlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  PerformAndCheckSuccessfulPairingCallbacks();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_StrictFlagDisabled) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            1);
}

TEST_F(FastPairPairerImplTest, UpdateOptInStatus_InitialPairing) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // Start opted out.
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/false, 0);

  // Pair the device via Initial Pairing protocol.
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();

  // Expect that the user is now opted in.
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_->GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/false, 0);
}

TEST_F(FastPairPairerImplTest, UpdateOptInStatus_RetroactivePairing) {
  Login(user_manager::UserType::kRegular);

  // Start opted out
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/false, 0);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // Retroactive pair
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();

  // Expect that the user is now opted in.
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_->GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/false, 0);
}

TEST_F(FastPairPairerImplTest, UpdateOptInStatus_SubsequentPairing) {
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPair},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // Start opted out
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/false, 0);

  // Subsequent pair
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), std::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->NotifyDevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // Expect that the user is opted in now.
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_->GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/false, 0);
}

// In this test's scenario, |adapter_| knows of |device_|, so the
// FastPairPairerImpl object in |fake_fast_pair_handshake_| will attempt and
// fail to pair with it directly using FastPairPairerImpl::Pair.
TEST_F(FastPairPairerImplTest, CreateBondTimeout_AdapterHasDeviceAddress) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();
  SetPairTimeout();
  CreatePairer();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

TEST_F(FastPairPairerImplTest,
       CreateBondTimeout_AdapterDoesNotHaveDeviceAddress) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // This call mocks the scenario in which |adapter_| does not know |device_|'s
  // address.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  SetConnectDeviceTimeout();
  CreatePairer();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_ConnectTimeout_Initial) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  AddConnectedHandshake();
  SetConnectTimeoutAfterPair();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_ConnectTimeout_Subsequent) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  AddConnectedHandshake();
  SetConnectTimeoutAfterPair();
  CreatePairer();
  fake_bluetooth_device_ptr_->TriggerPairCallback();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

TEST_F(FastPairPairerImplTest, RetroactiveNotLoggedToInitial) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPreparingToWriteAccountKey),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kAccountKeyWritten),
            0);
}

TEST_F(FastPairPairerImplTest, BleAddressRotatedCallsCallback) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairBleRotation};
  Login(user_manager::UserType::kRegular);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  SetHandshakeBleCallback();
  CreatePairer();
  EXPECT_TRUE(on_ble_rotation_callback_called_);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
}

// Because we have an overall bonding timer, we still test what happens when
// the `ConfirmPasskey` event times out, and expect the overall timer to
// fire.
TEST_F(FastPairPairerImplTest,
       CreateBondTimeout_ConfirmPasskey_AdapterHasDeviceAddress) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  AddConnectedHandshake();
  CreatePairer();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

TEST_F(FastPairPairerImplTest,
       CreateBondTimeout_ConfirmPasskey_AdapterDoesNotHaveDeviceAddress) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  task_environment()->FastForwardBy(kCreateBondTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Retroactive) {
  Login(user_manager::UserType::kRegular);
  fast_pair_repository_->SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  // The following code is what's in |CreateDevice()| except protocol is
  // Retroactive instead of Initial.
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairRetroactive);

  // Adds a connected handshake that has completed successfully in
  // 'FastPairHandshakeLookup' for the mock device.
  //
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();
  SetPublicKey();

  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Initiates recognition of Retroactive Pair scenario.
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotPaired);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
}

TEST_F(FastPairPairerImplTest, DisplayPasskey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairKeyboards,
                            floss::features::kFlossEnabled},
      /*disabled_features=*/{});

  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();

  EXPECT_CALL(display_passkey_, Run);
  NotifyDisplayPasskey();
}

TEST_F(FastPairPairerImplTest, DoNotDisplayPasskey) {
  Login(user_manager::UserType::kRegular);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address. 'SetGetDeviceNullptr' tells the adapter
  // to return null when queried for the device to mock this behavior.
  SetGetDeviceNullptr();
  AddConnectedHandshake();
  CreatePairer();

  EXPECT_CALL(display_passkey_, Run).Times(0);
  NotifyDisplayPasskey();
}

}  // namespace quick_pair
}  // namespace ash
