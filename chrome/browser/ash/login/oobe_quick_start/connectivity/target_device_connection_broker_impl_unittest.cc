// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <array>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/nearby/fake_quick_start_connectivity_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connection.h"
#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/constants/devicetype.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

constexpr size_t kMaxEndpointInfoDisplayNameLength = 18;
// Default verification style 5 = "OUT_OF_BAND", which tells the phone to scan
// for the QR code.
constexpr uint8_t kEndpointInfoVerificationStyle = 5u;
constexpr uint8_t kEndpointInfoDeviceType = 8u;

constexpr size_t kEndpointInfoAdvertisingIdLength = 10;

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 32 random bytes to use as the secondary shared secret.
constexpr std::array<uint8_t, 32> kSecondarySharedSecret = {
    0x50, 0xfe, 0xd7, 0x14, 0x1c, 0x93, 0xf6, 0x92, 0xaf, 0x7b, 0x4d,
    0xab, 0xa0, 0xe3, 0xfc, 0xd3, 0x5a, 0x04, 0x01, 0x63, 0xf6, 0xf5,
    0xeb, 0x40, 0x7f, 0x4b, 0xac, 0xe4, 0xd1, 0xbf, 0x20, 0x19};

// random int with 64 bits to use as SessionId.
constexpr uint64_t kSessionId = 184467440;

// Arbitrary string to use as the endpoint id.
constexpr char kEndpointId[] = "endpoint_id";

// Arbitrary string to use as the connection's authentication token when
// deriving PIN.
constexpr char kAuthenticationToken[] = "auth_token";

// Expected PIN corresponding to |kAuthenticationToken|.
constexpr char kAuthenticationTokenPin[] = "6229";

constexpr base::TimeDelta kNearbyConnectionsAdvertisementAfterUpdateTimeout =
    base::Seconds(30);

// Perform base64 decoding with the kForgiving option to allow for missing
// padding.
std::vector<uint8_t> Base64DecodeForgiving(base::span<uint8_t> input) {
  std::string input_str(input.begin(), input.end());
  std::string output;
  base::Base64Decode(input_str, &output, base::Base64DecodePolicy::kForgiving);
  return std::vector<uint8_t>(output.begin(), output.end());
}

struct EndpointInfoTestCase {
  chromeos::DeviceType device_type;
  std::string expected_display_name;
};

const EndpointInfoTestCase kEndpointInfoTestCases[] = {
    {chromeos::DeviceType::kChromebook, "Chromebook"},
    {chromeos::DeviceType::kChromebox, "Chromebox"},
    {chromeos::DeviceType::kChromebit, "Chromebit"},
    {chromeos::DeviceType::kChromebase, "Chromebase"},
    {chromeos::DeviceType::kUnknown,
     "Chrome devic"},  // The "e" is truncated to fit within endpoint bytes.
};

using testing::NiceMock;

// Ensures that the device name retrieved for the EndpointInfo display name will
// include the specified device type, e.g. kChromebook will result in a device
// name like "Chromebook (958)".
void SetDeviceType(chromeos::DeviceType device_type) {
  switch (device_type) {
    case chromeos::DeviceType::kChromebook:
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          {"", "--form-factor=CHROMEBOOK"});
      break;
    case chromeos::DeviceType::kChromebox:
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          {"", "--form-factor=CHROMEBOX"});
      break;
    case chromeos::DeviceType::kChromebit:
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          {"", "--form-factor=CHROMEBIT"});
      break;
    case chromeos::DeviceType::kChromebase:
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          {"", "--form-factor=CHROMEBASE"});
      break;
    case chromeos::DeviceType::kUnknown:
      base::CommandLine::ForCurrentProcess()->InitFromArgv({"", ""});
      break;
  }
}

// Allows us to delay returning a Bluetooth adapter until after ReturnAdapter()
// is called. Used for testing how the connection broker behaves before the
// Bluetooth adapter is finished initializing
class DeferredBluetoothAdapterFactoryWrapper
    : public TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper {
 public:
  void ReturnAdapter() {
    if (!adapter_callback_) {
      return;
    }

    device::BluetoothAdapterFactory::Get()->GetAdapter(
        std::move(adapter_callback_));
  }

 private:
  void GetAdapterImpl(
      device::BluetoothAdapterFactory::AdapterCallback callback) override {
    adapter_callback_ = std::move(callback);
  }

  device::BluetoothAdapterFactory::AdapterCallback adapter_callback_;
};

class FakeFastPairAdvertiser : public FastPairAdvertiser {
 public:
  explicit FakeFastPairAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      bool should_succeed_on_start,
      base::OnceCallback<void()> on_stop_advertising_callback,
      base::OnceCallback<void()> on_destroy_callback)
      : FastPairAdvertiser(adapter),
        should_succeed_on_start_(should_succeed_on_start),
        on_stop_advertising_callback_(std::move(on_stop_advertising_callback)),
        on_destroy_callback_(std::move(on_destroy_callback)) {}

  ~FakeFastPairAdvertiser() override {
    StopAdvertising(base::DoNothing());
    std::move(on_destroy_callback_).Run();
  }

  void StartAdvertising(base::OnceCallback<void()> callback,
                        base::OnceCallback<void()> error_callback,
                        const AdvertisingId& advertising_id) override {
    ++start_advertising_call_count_;
    if (should_succeed_on_start_) {
      std::move(callback).Run();
    } else {
      std::move(error_callback).Run();
    }
  }

  void StopAdvertising(base::OnceCallback<void()> callback) override {
    if (!has_called_on_stop_advertising_callback_) {
      std::move(on_stop_advertising_callback_).Run();
      has_called_on_stop_advertising_callback_ = true;
    }

    std::move(callback).Run();
  }

  size_t start_advertising_call_count() {
    return start_advertising_call_count_;
  }

 private:
  bool should_succeed_on_start_;
  bool has_called_on_stop_advertising_callback_ = false;
  size_t start_advertising_call_count_ = 0u;
  base::OnceCallback<void()> on_stop_advertising_callback_;
  base::OnceCallback<void()> on_destroy_callback_;
};

class FakeFastPairAdvertiserFactory : public FastPairAdvertiser::Factory {
 public:
  explicit FakeFastPairAdvertiserFactory(bool should_succeed_on_start)
      : should_succeed_on_start_(should_succeed_on_start) {}

  std::unique_ptr<FastPairAdvertiser> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    auto fake_fast_pair_advertiser = std::make_unique<FakeFastPairAdvertiser>(
        adapter, should_succeed_on_start_,
        base::BindOnce(&FakeFastPairAdvertiserFactory::OnStopAdvertising,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &FakeFastPairAdvertiserFactory::OnFastPairAdvertiserDestroyed,
            weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_pair_advertiser_ = fake_fast_pair_advertiser.get();
    return std::move(fake_fast_pair_advertiser);
  }

  void OnFastPairAdvertiserDestroyed() {
    fast_pair_advertiser_destroyed_ = true;
    last_fake_fast_pair_advertiser_ = nullptr;
  }

  void OnStopAdvertising() { stop_advertising_called_ = true; }

  size_t StartAdvertisingCount() {
    return last_fake_fast_pair_advertiser_
               ? last_fake_fast_pair_advertiser_->start_advertising_call_count()
               : 0;
  }

  bool AdvertiserDestroyed() { return fast_pair_advertiser_destroyed_; }

  bool StopAdvertisingCalled() { return stop_advertising_called_; }

 private:
  raw_ptr<FakeFastPairAdvertiser> last_fake_fast_pair_advertiser_ = nullptr;
  bool should_succeed_on_start_ = false;
  bool stop_advertising_called_ = false;
  bool fast_pair_advertiser_destroyed_ = false;
  base::WeakPtrFactory<FakeFastPairAdvertiserFactory> weak_ptr_factory_{this};
};

class FakeConnectionLifecycleListener
    : public TargetDeviceConnectionBroker::ConnectionLifecycleListener {
 public:
  void OnPinVerificationRequested(const std::string& pin) override {
    pin_ = pin;
  }

  void OnConnectionAuthenticated(
      base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
          connection) override {
    connection_authenticated_ = true;
    authenticated_connection_ = connection;
  }

  void OnConnectionRejected() override { connection_rejected_ = true; }

  void OnConnectionClosed(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason) override {
    connection_closed_ = true;
    connection_closed_reason_ = reason;
  }

  std::optional<std::string> pin_;
  bool connection_authenticated_ = false;
  base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
      authenticated_connection_;
  bool connection_rejected_ = false;
  bool connection_closed_ = false;
  TargetDeviceConnectionBroker::ConnectionClosedReason
      connection_closed_reason_;
};

}  // namespace

class TargetDeviceConnectionBrokerImplTest : public testing::Test {
 public:
  TargetDeviceConnectionBrokerImplTest() = default;
  TargetDeviceConnectionBrokerImplTest(TargetDeviceConnectionBrokerImplTest&) =
      delete;
  TargetDeviceConnectionBrokerImplTest& operator=(
      TargetDeviceConnectionBrokerImplTest&) = delete;
  ~TargetDeviceConnectionBrokerImplTest() override = default;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this, &TargetDeviceConnectionBrokerImplTest::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered())
        .WillByDefault(Invoke(
            this, &TargetDeviceConnectionBrokerImplTest::IsBluetoothPowered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);
    TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper::
        set_bluetooth_adapter_factory_wrapper_for_testing(
            &bluetooth_adapter_factory_wrapper_);

    fake_quick_start_connectivity_service_ =
        std::make_unique<FakeQuickStartConnectivityService>();
    fake_nearby_connections_manager_ = fake_quick_start_connectivity_service_
                                           ->GetFakeNearbyConnectionsManager();

    CreateConnectionBroker();
    SetFakeFastPairAdvertiserFactory(/*should_succeed_on_start=*/true);
    fake_nearby_connections_manager_->SetAuthenticationToken(
        kEndpointId, kAuthenticationToken);
  }

  void CreateConnectionBroker(bool is_resume_after_update = false) {
    auto connection_factory = std::make_unique<FakeConnection::Factory>();
    connection_factory_ = connection_factory.get();

    if (is_resume_after_update) {
      session_context_ =
          SessionContext(kSessionId, advertising_id_, kSharedSecret,
                         kSecondarySharedSecret, is_resume_after_update);
    }

    connection_broker_ = std::make_unique<TargetDeviceConnectionBrokerImpl>(
        &session_context_, fake_quick_start_connectivity_service_.get(),
        std::move(connection_factory));
  }

  void FinishFetchingBluetoothAdapter() {
    base::RunLoop().RunUntilIdle();
    bluetooth_adapter_factory_wrapper_.ReturnAdapter();
  }

  bool IsBluetoothPowered() { return is_bluetooth_powered_; }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }

  void SetBluetoothIsPowered(bool powered) { is_bluetooth_powered_ = powered; }

  void SetBluetoothIsPresent(bool present) { is_bluetooth_present_ = present; }

  void SetFakeFastPairAdvertiserFactory(bool should_succeed_on_start) {
    fast_pair_advertiser_factory_ =
        std::make_unique<FakeFastPairAdvertiserFactory>(
            should_succeed_on_start);
    FastPairAdvertiser::Factory::SetFactoryForTesting(
        fast_pair_advertiser_factory_.get());
  }

  void StartAdvertisingResultCallback(bool success) {
    start_advertising_callback_called_ = true;
    start_advertising_callback_success_ = success;
  }

  void StopAdvertisingCallback() { stop_advertising_callback_called_ = true; }

  std::vector<uint8_t> GenerateEndpointInfo() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->GenerateEndpointInfo();
  }

  std::string DerivePin() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->DerivePin(kAuthenticationToken);
  }

  FakeConnection* connection() { return connection_factory_->instance_.get(); }

 protected:
  bool is_bluetooth_powered_ = true;
  bool is_bluetooth_present_ = true;
  bool start_advertising_callback_called_ = false;
  bool start_advertising_callback_success_ = false;
  bool stop_advertising_callback_called_ = false;
  AdvertisingId advertising_id_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<FakeQuickStartConnectivityService>
      fake_quick_start_connectivity_service_;
  SessionContext session_context_ = SessionContext(kSessionId,
                                                   advertising_id_,
                                                   kSharedSecret,
                                                   kSecondarySharedSecret);
  raw_ptr<FakeNearbyConnectionsManager> fake_nearby_connections_manager_;
  FakeNearbyConnection fake_nearby_connection_;
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;
  std::unique_ptr<FakeFastPairAdvertiserFactory> fast_pair_advertiser_factory_;
  DeferredBluetoothAdapterFactoryWrapper bluetooth_adapter_factory_wrapper_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeConnectionLifecycleListener connection_lifecycle_listener_;
  raw_ptr<FakeConnection::Factory> connection_factory_ = nullptr;
  base::HistogramTester histogram_tester_;
  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};

  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_ =
      std::make_unique<FakeQuickStartDecoder>();
  base::WeakPtrFactory<TargetDeviceConnectionBrokerImplTest> weak_ptr_factory_{
      this};
};

class TargetDeviceConnectionBrokerImplEndpointInfoTest
    : public TargetDeviceConnectionBrokerImplTest,
      public testing::WithParamInterface<EndpointInfoTestCase> {
 public:
  void SetUp() override {
    SetDeviceType(GetParam().device_type);
    TargetDeviceConnectionBrokerImplTest::SetUp();
  }
};

TEST_F(TargetDeviceConnectionBrokerImplTest, GetFeatureSupportStatus) {
  EXPECT_EQ(
      TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kUndetermined,
      connection_broker_->GetFeatureSupportStatus());

  FinishFetchingBluetoothAdapter();

  SetBluetoothIsPresent(false);
  EXPECT_EQ(
      TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kNotSupported,
      connection_broker_->GetFeatureSupportStatus());

  SetBluetoothIsPresent(true);
  EXPECT_EQ(TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kSupported,
            connection_broker_->GetFeatureSupportStatus());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, StartFastPairAdvertising) {
  FinishFetchingBluetoothAdapter();
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertising_BeforeBTAdapterInitialized) {
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  FinishFetchingBluetoothAdapter();

  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertisingError_BluetoothNotPresent) {
  FinishFetchingBluetoothAdapter();
  SetBluetoothIsPresent(false);
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertisingError_BluetoothNotPowered) {
  FinishFetchingBluetoothAdapter();
  SetBluetoothIsPowered(false);
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertisingError_Unsuccessful) {
  FinishFetchingBluetoothAdapter();
  SetFakeFastPairAdvertiserFactory(/*should_succeed_on_start=*/false);
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
  EXPECT_TRUE(fast_pair_advertiser_factory_->AdvertiserDestroyed());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StopFastPairAdvertising_NeverStarted) {
  FinishFetchingBluetoothAdapter();

  // If StartAdvertising is never called, StopAdvertising should not propagate
  // to the fast pair advertiser.
  connection_broker_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImplTest::StopAdvertisingCallback,
      weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(stop_advertising_callback_called_);
  EXPECT_FALSE(fast_pair_advertiser_factory_->StopAdvertisingCalled());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StopFastPairAdvertising_BeforeBTAdapterInitialized) {
  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));

  // If the Bluetooth adapter hasn't finished initializing, then
  // StartAdvertisings never completed, and StopAdvertising should not
  // propagate to the fast pair advertiser.
  connection_broker_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImplTest::StopAdvertisingCallback,
      weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(stop_advertising_callback_called_);
  EXPECT_FALSE(fast_pair_advertiser_factory_->StopAdvertisingCalled());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, StopFastPairAdvertising) {
  FinishFetchingBluetoothAdapter();

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
  EXPECT_FALSE(fast_pair_advertiser_factory_->StopAdvertisingCalled());

  connection_broker_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImplTest::StopAdvertisingCallback,
      weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(fast_pair_advertiser_factory_->StopAdvertisingCalled());
  EXPECT_TRUE(fast_pair_advertiser_factory_->AdvertiserDestroyed());
  EXPECT_TRUE(stop_advertising_callback_called_);
}

TEST_P(TargetDeviceConnectionBrokerImplEndpointInfoTest, GenerateEndpointInfo) {
  std::vector<uint8_t> endpoint_info = GenerateEndpointInfo();

  // Points to the field being parsed.
  size_t i = 0;

  ASSERT_GT(endpoint_info.size(), i);
  uint8_t version = endpoint_info[i];
  EXPECT_EQ(1u, version);
  i++;

  // Parse the display name. The field is variable-length, so we have to look
  // out for either a null byte or for the display name to reach the maximum
  // length.
  ASSERT_GT(endpoint_info.size(), i);
  size_t j = 0;
  std::vector<uint8_t> display_name_bytes;
  while (i + j < endpoint_info.size() && endpoint_info[i + j] != 0u &&
         j < kMaxEndpointInfoDisplayNameLength) {
    display_name_bytes.push_back(endpoint_info[i + j]);
    j++;
  }
  // Assert that we didn't break out of the while loop because we ran out of
  // bytes.
  ASSERT_LT(i + j, endpoint_info.size());
  if (j < kMaxEndpointInfoDisplayNameLength) {
    // Move past the null-terminator if the display name length is less than
    // the max.
    ASSERT_EQ(0u, endpoint_info[i + j]);
    j++;
  }
  std::string display_name =
      std::string(display_name_bytes.begin(), display_name_bytes.end());
  std::string expected_display_name = GetParam().expected_display_name + " (" +
                                      advertising_id_.GetDisplayCode() + ")";
  EXPECT_EQ(expected_display_name, display_name);
  i += j;

  // The remaining advertising info fields are base64-encoded. Decode them
  // before proceeding.
  std::vector<uint8_t> advertising_info = Base64DecodeForgiving(
      base::span<uint8_t>(endpoint_info.begin() + i, endpoint_info.end()));
  ASSERT_EQ(advertising_info.size(), 60u);
  i = 0;

  uint8_t verification_style = advertising_info[i];
  EXPECT_EQ(kEndpointInfoVerificationStyle, verification_style);
  i++;

  uint8_t device_type = advertising_info[i];
  EXPECT_EQ(kEndpointInfoDeviceType, device_type);
  i++;

  // Parse the AdvertisingId. The field is fixed-width, but contains a
  // string that may not occupy the full length, in which case there will be a
  // null terminator.
  std::string advertising_id = advertising_id_.ToString();
  for (size_t k = i; k < i + kEndpointInfoAdvertisingIdLength; k++) {
    if (advertising_info[k] == 0) {
      break;
    }
    EXPECT_EQ(advertising_id[k - i], advertising_info[k]);
  }
  i += kEndpointInfoAdvertisingIdLength;

  uint8_t is_quick_start = advertising_info[i];
  EXPECT_EQ(1u, is_quick_start);
  i++;

  uint8_t prefer_target_user_verification = advertising_info[i];
  EXPECT_EQ(0u, prefer_target_user_verification);
}

INSTANTIATE_TEST_SUITE_P(TargetDeviceConnectionBrokerImplTest,
                         TargetDeviceConnectionBrokerImplEndpointInfoTest,
                         testing::ValuesIn(kEndpointInfoTestCases));

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartNearbyConnectionsAdvertising) {
  FinishFetchingBluetoothAdapter();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(NearbyConnectionsManager::PowerLevel::kHighPower,
            fake_nearby_connections_manager_->advertising_power_level());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartNearbyConnectionsAdvertisingError) {
  FinishFetchingBluetoothAdapter();
  FakeNearbyConnectionsManager::ConnectionsCallback callback =
      fake_nearby_connections_manager_->GetStartAdvertisingCallback();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(start_advertising_callback_called_);

  std::move(callback).Run(NearbyConnectionsManager::ConnectionsStatus::kError);
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, DerivePin) {
  EXPECT_EQ(kAuthenticationTokenPin, DerivePin());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, Handshake_Success) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
  EXPECT_FALSE(connection_lifecycle_listener_.connection_authenticated_);

  connection()->HandleHandshakeResult(/*success=*/true);
  EXPECT_TRUE(connection_lifecycle_listener_.connection_authenticated_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, Handshake_Failed) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
  EXPECT_FALSE(connection_lifecycle_listener_.connection_authenticated_);

  connection()->HandleHandshakeResult(/*success=*/false);
  EXPECT_FALSE(connection_lifecycle_listener_.connection_authenticated_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       ConnectionIsAuthenticatedWithPinMethod) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ true,
                                       base::DoNothing());
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection_lifecycle_listener_.connection_authenticated_);
  EXPECT_NE(connection_lifecycle_listener_.authenticated_connection_, nullptr);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       ConnectionClosedEventIssuesCallback) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());

  connection()->Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError);

  ASSERT_TRUE(connection_lifecycle_listener_.connection_closed_);
  ASSERT_EQ(
      connection_lifecycle_listener_.connection_closed_reason_,
      TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartAdvertisingWhenResumeAfterUpdate) {
  CreateConnectionBroker(/*is_resume_after_update=*/true);
  FinishFetchingBluetoothAdapter();
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));

  // When the target device resumes the connection after an update, it should
  // begin Nearby Connections advertising without ever Fast Pair advertising.
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(NearbyConnectionsManager::PowerLevel::kHighPower,
            fake_nearby_connections_manager_->advertising_power_level());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       HandshakeInitiatedWhenResumeAfterUpdate_UseQRCodeAuthentication) {
  CreateConnectionBroker(/*is_resume_after_update=*/true);
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  // On the first attempt to resume the connection after an update, no QR code
  // or PIN should be generated on connection initiated.
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       HandshakeInitiatedWhenResumeAfterUpdate_UsePinAuthentication) {
  CreateConnectionBroker(/*is_resume_after_update=*/true);
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ true,
                                       base::DoNothing());
  ASSERT_FALSE(connection_lifecycle_listener_.pin_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  // On the first attempt to resume the connection after an update, no QR code
  // or PIN should be generated on connection initiated.
  EXPECT_FALSE(connection_lifecycle_listener_.pin_);
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       NearbyConnectionsAdvertisingTimeoutWhenResumeAfterUpdate_Pin) {
  CreateConnectionBroker(/*is_resume_after_update=*/true);
  FinishFetchingBluetoothAdapter();

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ true,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  // When Nearby Connections advertising is not successful because it times out,
  // advertising will begin like the initial connection flow.
  task_environment_.FastForwardBy(
      kNearbyConnectionsAdvertisementAfterUpdateTimeout);
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_FALSE(connection()->WasHandshakeInitiated());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       NearbyConnectionsAdvertisingTimeoutWhenResumeAfterUpdate_QrCode) {
  CreateConnectionBroker(/*is_resume_after_update=*/true);
  FinishFetchingBluetoothAdapter();

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  // When Nearby Connections advertising is not successful because it times out,
  // advertising will begin like the initial connection flow.
  task_environment_.FastForwardBy(
      kNearbyConnectionsAdvertisementAfterUpdateTimeout);
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_->GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
}

}  // namespace ash::quick_start
