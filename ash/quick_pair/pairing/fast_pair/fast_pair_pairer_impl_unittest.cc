// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

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
const char kPasskeyCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Time";
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
const char kConfirmPasskeyAskTime[] =
    "Bluetooth.ChromeOS.FastPair.RequestPasskey.Latency";
const char kConfirmPasskeyConfirmTime[] =
    "Bluetooth.ChromeOS.FastPair.ConfirmPasskey.Latency";
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
constexpr char kInitializePairingProcessSubsequent[] =
    "FastPair.SubsequentPairing.Initialization";
constexpr char kInitializePairingProcessRetroactive[] =
    "FastPair.RetroactivePairing.Initialization";
constexpr char kInitializePairingProcessFailureReasonInitial[] =
    "FastPair.InitialPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonSubsequent[] =
    "FastPair.SubsequentPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonRetroactive[] =
    "FastPair.RetroactivePairing.Initialization.FailureReason";

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    // There are a few situations where we want GetDevice to return nullptr. For
    // example, if we want the Pairer to "pair by address" then GetDevice should
    // return nullptr when called on the mac address.
    if (get_device_returns_nullptr_ &&
        address == kBluetoothCanonicalizedAddress) {
      get_device_returns_nullptr_ = false;
      return nullptr;
    }

    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }

    return nullptr;
  }

  void AddPairingDelegate(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      PairingDelegatePriority priority) override {
    pairing_delegate_ = pairing_delegate;
  }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service) {
    device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
  }

  void NotifyConfirmPasskey(uint32_t passkey, device::BluetoothDevice* device) {
    pairing_delegate_->ConfirmPasskey(device, passkey);
  }

  void DevicePairedChanged(device::BluetoothDevice* device,
                           bool new_paired_status) {
    for (auto& observer : GetObservers())
      observer.DevicePairedChanged(this, device, new_paired_status);
  }

  void ConnectDevice(
      const std::string& address,
      const absl::optional<device::BluetoothDevice::AddressType>& address_type,
      base::OnceCallback<void(device::BluetoothDevice*)> callback,
      base::OnceCallback<void(const std::string&)> error_callback) override {
    if (connect_device_failure_) {
      std::move(error_callback).Run("");
      return;
    }

    std::move(callback).Run(GetDevice(address));
  }

  void SetConnectFailure() { connect_device_failure_ = true; }

  void SetGetDeviceNullptr() { get_device_returns_nullptr_ = true; }

 protected:
  ~FakeBluetoothAdapter() override = default;
  bool connect_device_failure_ = false;
  bool get_device_returns_nullptr_ = false;
  device::BluetoothDevice::PairingDelegate* pairing_delegate_ = nullptr;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  explicit FakeBluetoothDevice(FakeBluetoothAdapter* adapter)
      : testing::NiceMock<device::MockBluetoothDevice>(
            adapter,
            0,
            kDeviceName,
            kBluetoothCanonicalizedAddress,
            /*paired=*/true,
            /*connected*/ false),
        fake_adapter_(adapter) {}

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

  void Pair(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceCallback<void(absl::optional<ConnectErrorCode> error_code)>
          callback) override {
    if (pair_failure_) {
      std::move(callback).Run(ConnectErrorCode::ERROR_FAILED);
      return;
    }

    std::move(callback).Run(absl::nullopt);
  }

  void SetPairFailure() { pair_failure_ = true; }

  void ConfirmPairing() override { is_device_paired_ = true; }

  bool IsDevicePaired() { return is_device_paired_; }

 protected:
  FakeBluetoothAdapter* fake_adapter_;
  bool pair_failure_ = false;
  bool is_device_paired_ = false;
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
      base::OnceCallback<void(absl::optional<ash::quick_pair::PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<ash::quick_pair::FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  ash::quick_pair::FakeFastPairGattServiceClient*
      fake_fast_pair_gatt_service_client_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

// For convenience.
using ::testing::Return;

class FastPairPairerImplTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);
    FastPairHandshakeLookup::SetCreateFunctionForTesting(base::BindRepeating(
        &FastPairPairerImplTest::CreateHandshake, base::Unretained(this)));

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

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
    pairer_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void CreateMockDevice(DeviceFastPairVersion version, Protocol protocol) {
    device_ = base::MakeRefCounted<Device>(
        kMetadataId, kBluetoothCanonicalizedAddress, protocol);
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

  void EraseHandshake() {
    FastPairHandshakeLookup::GetInstance()->Erase(device_);
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    // This is the only place where data_encryptor_unique_ is used. We assume
    // that CreateHandshake is only called once.
    auto fake_fast_pair_handshake = std::make_unique<FakeFastPairHandshake>(
        adapter_, device, std::move(callback),
        std::move(data_encryptor_unique_), std::move(gatt_service_client_));

    fake_fast_pair_handshake_ = fake_fast_pair_handshake.get();

    return fake_fast_pair_handshake;
  }

  void SetReuseHandshake() {
    FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                   base::DoNothing());
    fake_fast_pair_handshake_->set_completed_successfully(
        /*completed_successfully=*/true);
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
    data_encryptor_->passkey(absl::nullopt);
  }

  void SetDecryptPasskeyForSuccess() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void RunWritePasskeyCallback(
      std::vector<uint8_t> data,
      absl::optional<PairFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWritePasskeyCallback(data, failure);
  }

  void RunWriteAccountKeyCallback(
      absl::optional<AccountKeyFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWriteAccountKeyCallback(failure);
  }

  void PairFailedCallback(scoped_refptr<Device> device, PairFailure failure) {
    failure_ = failure;
  }

  void NotifyConfirmPasskey() {
    adapter_->NotifyConfirmPasskey(kValidPasskey, fake_bluetooth_device_ptr_);
  }

  absl::optional<PairFailure> GetPairFailure() { return failure_; }

  void SetPairFailure() { fake_bluetooth_device_ptr_->SetPairFailure(); }

  void SetConnectFailure() { adapter_->SetConnectFailure(); }

  void SetGetDeviceNullptr() { adapter_->SetGetDeviceNullptr(); }

  bool IsDevicePaired() { return fake_bluetooth_device_ptr_->IsDevicePaired(); }

  bool IsAccountKeySavedToFootprints() {
    return fast_pair_repository_.HasKeyForDevice(
        fake_bluetooth_device_ptr_->GetAddress());
  }

  void SetPublicKey() { data_encryptor_->public_key(kPublicKey); }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void DeviceUnpaired() {
    adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, false);
  }

  void DevicePaired() {
    adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  }

  void ExpectStepMetrics(std::string metric,
                         std::vector<FastPairProtocolPairingSteps> steps) {
    histogram_tester().ExpectTotalCount(metric, steps.size());
    for (FastPairProtocolPairingSteps step : steps) {
      histogram_tester().ExpectBucketCount(metric, step, 1);
    }
  }

 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairerImpl>(
        adapter_, device_, handshake_complete_callback_.Get(),
        paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  void CreatePairerAsFactory() {
    pairer_ = FastPairPairerImpl::Factory::Create(
        adapter_, device_, handshake_complete_callback_.Get(),
        paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  void CreateDevice(DeviceFastPairVersion version) {
    CreateMockDevice(version,
                     /*protocol=*/Protocol::kFastPairInitial);
    CreatePairer();
    if (version == DeviceFastPairVersion::kHigherThanV1) {
      SetPublicKey();
      // When pairing starts, if the classic address can't be resolved to
      // a device then we pair via address.
      SetGetDeviceNullptr();
      fake_fast_pair_handshake_->InvokeCallback();
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(GetPairFailure(), absl::nullopt);
      EXPECT_CALL(paired_callback_, Run);
      SetDecryptPasskeyForSuccess();
      NotifyConfirmPasskey();
      base::RunLoop().RunUntilIdle();
    }
  }

  void PerformAndCheckSuccessfulPairingCallbacks() {
    RunWritePasskeyCallback(kResponseBytes);
    base::RunLoop().RunUntilIdle();
    EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
    EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
    adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
    RunWriteAccountKeyCallback();
  }

  bool set_handshake_completed_successfully_ = false;
  absl::optional<PairFailure> failure_ = absl::nullopt;
  std::unique_ptr<FakeBluetoothDevice> fake_bluetooth_device_;
  FakeBluetoothDevice* fake_bluetooth_device_ptr_ = nullptr;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      handshake_complete_callback_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      paired_callback_;
  base::MockCallback<
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>>
      account_key_failure_callback_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      pairing_procedure_complete_;
  FakeFastPairRepository fast_pair_repository_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  std::unique_ptr<FakeFastPairDataEncryptor> data_encryptor_unique_;
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;
  FakeFastPairHandshake* fake_fast_pair_handshake_ = nullptr;
  std::unique_ptr<FastPairPairer> pairer_;
  base::WeakPtrFactory<FastPairPairerImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairPairerImplTest, NoPairingIfHandshakeFailed_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateGattConnection);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateGattConnection);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonInitial,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(FastPairPairerImplTest, NoPairingIfHandshakeFailed_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateGattConnection);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateGattConnection);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonSubsequent,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(FastPairPairerImplTest, NoPairingIfHandshakeFailed_Retroactive) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateGattConnection);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateGattConnection);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonRetroactive,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Retroactive) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

// PairByDevice refers to the fact that we aren't pairing by address, unlike
// most other tests in this file.
TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Initial_CancelsPairing) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  CreatePairer();

  // Mock that the device was paired unsuccessfully.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsPaired()).WillOnce(Return(false));
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();

  // Check to make sure that, when pairing fails, we call CancelPairing.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, CancelPairing()).Times(1);
}

TEST_F(FastPairPairerImplTest, PairByDeviceFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  SetPairFailure();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Initial_AlreadyClassicPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  EXPECT_CALL(paired_callback_, Run);
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);

  // For an already classic paired device, we skip right to Account Key writing.
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kAlreadyPaired,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Initial_AlreadyFastPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  // Mock that the device is already fast paired (and saved to Footprints).
  fast_pair_repository_.SaveMacAddressToAccount(kBluetoothCanonicalizedAddress);
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  // For an already fast paired device, we skip the Account Key writing.
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kInitialSuccessFunnelMetric,
          FastPairInitialSuccessFunnelEvent::kDeviceAlreadyAssociatedToAccount),
      1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kAlreadyPaired,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Subsequent_AlreadyClassicPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  // Mock that the device is already paired.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  EXPECT_CALL(paired_callback_, Run);
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kAlreadyPaired,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest,
       PairByDeviceSuccess_Subsequent_AlreadyFastPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  // Mock that the device is already fast paired (and saved to Footprints).
  fast_pair_repository_.SaveMacAddressToAccount(kBluetoothCanonicalizedAddress);
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsBonded()).WillOnce(Return(true));

  // For an already fast paired device, we skip the Account Key writing.
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kAlreadyPaired,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest, PairByDeviceSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful});
}

TEST_F(FastPairPairerImplTest, ConnectFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetConnectFailure();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, ConnectFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  SetConnectFailure();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted});
}

TEST_F(FastPairPairerImplTest, ConnectSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful});
}

TEST_F(FastPairPairerImplTest, ConnectSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();

  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Initial_SeekersPasskey) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kSeekersPasskey);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingRequest) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingRequest);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingResponse) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyNoPasskey) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForNoPasskey();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyDecryptFailure);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated});
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated});
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();

  // This time, this helper function is used to make the device lost during
  // Passkey exchange.
  SetGetDeviceNullptr();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated,
                     FastPairProtocolPairingSteps::kPasskeyConfirmed});
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();

  // This time, this helper function is used to make the device lost during
  // Passkey exchange.
  SetGetDeviceNullptr();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated,
                     FastPairProtocolPairingSteps::kPasskeyConfirmed});
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
  ExpectStepMetrics(kProtocolPairingStepInitial,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated,
                     FastPairProtocolPairingSteps::kPasskeyConfirmed,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest, BleDeviceLostMidPair) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();

  // Simulate BLE device being lost in the middle of pairing flow.
  EraseHandshake();

  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(PairFailure::kBleDeviceLostMidPair, GetPairFailure());
  EXPECT_FALSE(IsDevicePaired());
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial_FactoryCreate) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairerAsFactory();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
  ExpectStepMetrics(kProtocolPairingStepSubsequent,
                    {FastPairProtocolPairingSteps::kPairingStarted,
                     FastPairProtocolPairingSteps::kBondSuccessful,
                     FastPairProtocolPairingSteps::kPasskeyNegotiated,
                     FastPairProtocolPairingSteps::kRecievedPasskeyResponse,
                     FastPairProtocolPairingSteps::kPasskeyValidated,
                     FastPairProtocolPairingSteps::kPasskeyConfirmed,
                     FastPairProtocolPairingSteps::kPairingComplete});
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_GuestLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kGuestModeDetected),
            1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_KioskAppLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_KIOSK_APP);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
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
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
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
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Subsequent_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  // Ensure that the account key is not written to the peripheral until the
  // peripheral is successfully paired.
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // With Subsequent pairing, we expect to save the account key to the
  // Saved Device registry, but not upload the key to Footprints.
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_StrictFlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Initial_GattErrorFailed) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorFailed);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorUnknown) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorUnknown);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInProgress) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattInProgress);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInvalidLength) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorInvalidLength);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPermitted) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotPermitted);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotAuthorized) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotAuthorized);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotPaired);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotSupported) {
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorNotSupported);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Initial_NoCancelPairing) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());

  // Mock that the device was paired successfully.
  EXPECT_CALL(*fake_bluetooth_device_ptr_, IsPaired()).WillOnce(Return(true));
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback(AccountKeyFailure::kGattErrorFailed);

  // Check to make sure that, after bonding a device, we don't cancel pairing
  // (since this causes a paired device to disconnect).
  EXPECT_CALL(*fake_bluetooth_device_ptr_, CancelPairing()).Times(0);
}

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DevicePaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

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

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DeviceUnpaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedIn_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();

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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateDevice(DeviceFastPairVersion::kHigherThanV1);

  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
  base::RunLoop().RunUntilIdle();

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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
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
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // Start opted out.
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/false, 0);
  base::RunLoop().RunUntilIdle();

  // Pair the device via Initial Pairing protocol.
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  RunWriteAccountKeyCallback();

  // Expect that the user is now opted in.
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_.GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusInitialResult,
      /*success=*/false, 0);
}

TEST_F(FastPairPairerImplTest, UpdateOptInStatus_RetroactivePairing) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);

  // Start opted out
  fast_pair_repository_.SetOptInStatus(
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
  base::RunLoop().RunUntilIdle();

  // Retroactive pair
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();

  // Expect that the user is now opted in
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_.GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusRetroactiveResult,
      /*success=*/false, 0);
}

TEST_F(FastPairPairerImplTest, UpdateOptInStatus_SubsequentPairing) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  // Start opted out
  fast_pair_repository_.SetOptInStatus(
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
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_EQ(DeviceFastPairVersion::kHigherThanV1, device_->version().value());
  adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);

  // Expect that the user is opted in now
  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN,
            fast_pair_repository_.GetOptInStatus());
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(
      kSavedDeviceUpdateOptInStatusSubsequentResult,
      /*success=*/false, 0);
}

// There are two pairing flows in which PairFailure::kCreateBondTimeout occurs.
// In this test's scenario, |adapter_| knows of |device_|, so the
// FastPairPairerImpl object in |fake_fast_pair_handshake_| will attempt and
// fail to pair with it directly using FastPairPairerImpl::Pair.
TEST_F(FastPairPairerImplTest, CreateBondTimeout_AdapterHasDeviceAddress) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateBondTimeout);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

// There are two pairing flows in which PairFailure::kCreateBondTimeout occurs.
// In this test's scenario, |adapter_| doesn't know of |device_|, so the
// FastPairPairerImpl object in |fake_fast_pair_handshake_| will attempt and
// fail to connect with it using FastPairPairerImpl::ConnectDevice.
TEST_F(FastPairPairerImplTest,
       CreateBondTimeout_AdapterDoesNotHaveDeviceAddress) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateBondTimeout);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateBondTimeout);
}

TEST_F(FastPairPairerImplTest, RetroactiveNotLoggedToInitial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // When pairing starts, if the classic address can't be resolved to
  // a device then we pair via address.
  SetGetDeviceNullptr();
  CreatePairer();
  fake_fast_pair_handshake_->InvokeCallback();
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

TEST_F(FastPairPairerImplTest, HandshakeReused_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  // Simulate Handshake already created before attempt
  SetReuseHandshake();
  SetGetDeviceNullptr();
  CreatePairer();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

TEST_F(FastPairPairerImplTest, HandshakeReused_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  // Simulate Handshake already created before attempt
  SetReuseHandshake();
  SetGetDeviceNullptr();
  CreatePairer();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessSubsequent,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

TEST_F(FastPairPairerImplTest, HandshakeReused_Retroactive) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // Simulate Handshake already created before attempt
  SetReuseHandshake();
  SetGetDeviceNullptr();
  CreatePairer();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessRetroactive,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

}  // namespace quick_pair
}  // namespace ash
