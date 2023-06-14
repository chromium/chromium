// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <array>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service_factory.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/constants/devicetype.h"
#include "components/prefs/pref_service.h"
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

constexpr size_t kEndpointInfoRandomSessionIdLength = 10;

// Base qr code url ("https://signin.google/qs/") represented in a 25 byte
// array.
constexpr std::array<uint8_t, 25> kBaseUrl = {
    0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x73,
    0x69, 0x67, 0x6e, 0x69, 0x6e, 0x2e, 0x67, 0x6f, 0x6f,
    0x67, 0x6c, 0x65, 0x2f, 0x71, 0x73, 0x2f};

// Qr code key param ("?key=") represented in a 5 byte array.
constexpr std::array<uint8_t, 5> kUrlKeyParam = {0x3f, 0x6b, 0x65, 0x79, 0x3d};

// 32 random bytes to use as the shared secret when generating QR Code.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// Base64 representation of kSharedSecret.
constexpr char kSharedSecretBase64[] =
    "VL1Az4p8L2rKFVnP8%2BsxCJBz79qH1CPAVdWDWwQoSfI%3D";

// Arbitrary string to use as the endpoint id.
constexpr char kEndpointId[] = "endpoint_id";

// Arbitrary string to use as the connection's authentication token when
// deriving PIN.
constexpr char kAuthenticationToken[] = "auth_token";

// Expected PIN corresponding to |kAuthenticationToken|.
constexpr char kAuthenticationTokenPin[] = "6229";

// The keys expected to be in the dict returned by PrepareForUpdate().
constexpr char kPrepareForUpdateRandomSessionIdKey[] = "random_session_id";
constexpr char kPrepareForUpdateSecondarySharedSecretKey[] =
    "secondary_shared_secret";

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
                        const RandomSessionId& random_session_id) override {
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
  raw_ptr<FakeFastPairAdvertiser, ExperimentalAsh>
      last_fake_fast_pair_advertiser_ = nullptr;
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

  void OnQRCodeVerificationRequested(
      const std::vector<uint8_t>& qr_code_data) override {
    qr_code_data_ = qr_code_data;
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

  absl::optional<std::string> pin_;
  absl::optional<std::vector<uint8_t>> qr_code_data_;
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
  TargetDeviceConnectionBrokerImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
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

    CreateConnectionBroker();
    SetFakeFastPairAdvertiserFactory(/*should_succeed_on_start=*/true);
    fake_nearby_connections_manager_.SetAuthenticationToken(
        kEndpointId, kAuthenticationToken);
  }

  void CreateConnectionBroker(bool is_resume_after_update = false) {
    auto connection_factory = std::make_unique<FakeConnection::Factory>();
    connection_factory_ = connection_factory.get();
    connection_broker_ = std::make_unique<TargetDeviceConnectionBrokerImpl>(
        fake_nearby_connections_manager_.GetWeakPtr(),
        std::move(connection_factory),
        mojo::SharedRemote<mojom::QuickStartDecoder>(
            fake_quick_start_decoder_->GetRemote()),
        is_resume_after_update);
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

  const RandomSessionId& GetRandomSessionId() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->random_session_id_;
  }

  const TargetDeviceConnectionBroker::SharedSecret GetSharedSecret() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->shared_secret_;
  }

  const TargetDeviceConnectionBroker::SharedSecret GetSecondarySharedSecret() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->secondary_shared_secret_;
  }

  std::string GetSecondarySharedSecretString() {
    TargetDeviceConnectionBroker::SharedSecret secondary_shared_secret =
        GetSecondarySharedSecret();
    std::string secondary_shared_secret_bytes(secondary_shared_secret.begin(),
                                              secondary_shared_secret.end());
    std::string secondary_shared_secret_base64;
    base::Base64Encode(secondary_shared_secret_bytes,
                       &secondary_shared_secret_base64);
    return secondary_shared_secret_base64;
  }

  const std::vector<uint8_t> GetQrCodeData() {
    const RandomSessionId& session_id = GetRandomSessionId();
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->GetQrCodeData(session_id, kSharedSecret);
  }

  std::string DerivePin() {
    return static_cast<TargetDeviceConnectionBrokerImpl*>(
               connection_broker_.get())
        ->DerivePin(kAuthenticationToken);
  }

  FakeConnection* connection() { return connection_factory_->instance_.get(); }

  PrefService* GetLocalState() { return local_state_.Get(); }

  void ResumeAfterUpdate() {
    // The connection broker expects these prefs to be set if resuming after an
    // update.
    GetLocalState()->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot,
                                true);
    base::Value::Dict info = connection_broker_->GetPrepareForUpdateInfo();
    GetLocalState()->SetDict(prefs::kResumeQuickStartAfterRebootInfo,
                             std::move(info));
    std::string expected_random_session_id = GetRandomSessionId().ToString();
    TargetDeviceConnectionBroker::SharedSecret expected_shared_secret =
        GetSecondarySharedSecret();

    CreateConnectionBroker(/*is_resume_after_update=*/true);
    ASSERT_EQ(expected_random_session_id, GetRandomSessionId().ToString());
    ASSERT_EQ(expected_shared_secret, GetSharedSecret());
  }

 protected:
  bool is_bluetooth_powered_ = true;
  bool is_bluetooth_present_ = true;
  bool start_advertising_callback_called_ = false;
  bool start_advertising_callback_success_ = false;
  bool stop_advertising_callback_called_ = false;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  FakeNearbyConnectionsManager fake_nearby_connections_manager_;
  FakeNearbyConnection fake_nearby_connection_;
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;
  std::unique_ptr<FakeFastPairAdvertiserFactory> fast_pair_advertiser_factory_;
  DeferredBluetoothAdapterFactoryWrapper bluetooth_adapter_factory_wrapper_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeConnectionLifecycleListener connection_lifecycle_listener_;
  ScopedTestingLocalState local_state_;
  raw_ptr<FakeConnection::Factory, ExperimentalAsh> connection_factory_ =
      nullptr;

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
                                      GetRandomSessionId().GetDisplayCode() +
                                      ")";
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

  // Parse the RandomSessionId. The field is fixed-width, but contains a
  // string that may not occupy the full length, in which case there will be a
  // null terminator.
  std::string session_id = GetRandomSessionId().ToString();
  for (size_t k = i; k < i + kEndpointInfoRandomSessionIdLength; k++) {
    if (advertising_info[k] == 0) {
      break;
    }
    EXPECT_EQ(session_id[k - i], advertising_info[k]);
  }
  i += kEndpointInfoRandomSessionIdLength;

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
  EXPECT_FALSE(fake_nearby_connections_manager_.IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());
  EXPECT_EQ(PowerLevel::kHighPower,
            fake_nearby_connections_manager_.advertising_power_level());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartNearbyConnectionsAdvertisingError) {
  FinishFetchingBluetoothAdapter();
  FakeNearbyConnectionsManager::ConnectionsCallback callback =
      fake_nearby_connections_manager_.GetStartAdvertisingCallback();
  EXPECT_FALSE(fake_nearby_connections_manager_.IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());
  EXPECT_FALSE(start_advertising_callback_called_);

  std::move(callback).Run(NearbyConnectionsManager::ConnectionsStatus::kError);
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, GetQRCodeData) {
  std::string random_session_id = GetRandomSessionId().ToString();
  std::string encoded_shared_secret(kSharedSecretBase64);

  std::vector<uint8_t> expected_data(std::begin(kBaseUrl), std::end(kBaseUrl));
  expected_data.insert(expected_data.end(), random_session_id.begin(),
                       random_session_id.end());
  expected_data.insert(expected_data.end(), std::begin(kUrlKeyParam),
                       std::end(kUrlKeyParam));
  expected_data.insert(expected_data.end(), encoded_shared_secret.begin(),
                       encoded_shared_secret.end());

  std::vector<uint8_t> actual_data = GetQrCodeData();
  EXPECT_EQ(expected_data, actual_data);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, DerivePin) {
  EXPECT_EQ(kAuthenticationTokenPin, DerivePin());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, Handshake_Success) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  EXPECT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  ASSERT_TRUE(connection_lifecycle_listener_.qr_code_data_);
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
  EXPECT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  ASSERT_TRUE(connection_lifecycle_listener_.qr_code_data_);
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
  EXPECT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection_lifecycle_listener_.connection_authenticated_);
  EXPECT_NE(connection_lifecycle_listener_.authenticated_connection_, nullptr);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, GetPrepareForUpdateInfo) {
  base::Value::Dict prepare_for_update_info =
      connection_broker_->GetPrepareForUpdateInfo();
  EXPECT_FALSE(prepare_for_update_info.empty());
  EXPECT_EQ(
      GetRandomSessionId().ToString(),
      *prepare_for_update_info.FindString(kPrepareForUpdateRandomSessionIdKey));
  EXPECT_EQ(GetSecondarySharedSecretString(),
            *prepare_for_update_info.FindString(
                kPrepareForUpdateSecondarySharedSecretKey));
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       ConnectionClosedEventIssuesCallback) {
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  EXPECT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  ASSERT_TRUE(connection_lifecycle_listener_.qr_code_data_);
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());

  connection()->Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason::kConnectionLost);

  ASSERT_TRUE(connection_lifecycle_listener_.connection_closed_);
  ASSERT_EQ(
      connection_lifecycle_listener_.connection_closed_reason_,
      TargetDeviceConnectionBroker::ConnectionClosedReason::kConnectionLost);
}

TEST_F(TargetDeviceConnectionBrokerImplTest, ConstructWhenResumeAfterUpdate) {
  ResumeAfterUpdate();

  // Prefs should be cleared after the |connection_broker_| construction.
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartAdvertisingWhenResumeAfterUpdate) {
  ResumeAfterUpdate();
  FinishFetchingBluetoothAdapter();
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_FALSE(fake_nearby_connections_manager_.IsAdvertising());

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));

  // When the target device resumes the connection after an update, it should
  // begin Nearby Connections advertising without ever Fast Pair advertising.
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());
  EXPECT_EQ(PowerLevel::kHighPower,
            fake_nearby_connections_manager_.advertising_power_level());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       HandshakeInitiatedWhenResumeAfterUpdate_UseQRCodeAuthentication) {
  ResumeAfterUpdate();
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ false,
                                       base::DoNothing());
  ASSERT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  // On the first attempt to resume the connection after an update, no QR code
  // or PIN should be generated on connection initiated.
  EXPECT_FALSE(connection_lifecycle_listener_.qr_code_data_);
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       HandshakeInitiatedWhenResumeAfterUpdate_UsePinAuthentication) {
  ResumeAfterUpdate();
  FinishFetchingBluetoothAdapter();
  connection_broker_->StartAdvertising(&connection_lifecycle_listener_,
                                       /* use_pin_authentication= */ true,
                                       base::DoNothing());
  ASSERT_FALSE(connection_lifecycle_listener_.pin_);
  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
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
  ResumeAfterUpdate();
  FinishFetchingBluetoothAdapter();

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ true,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  // When Nearby Connections advertising is not successful because it times out,
  // advertising will begin like the initial connection flow.
  task_environment_.FastForwardBy(
      kNearbyConnectionsAdvertisementAfterUpdateTimeout);
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());

  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
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
  ResumeAfterUpdate();
  FinishFetchingBluetoothAdapter();

  connection_broker_->StartAdvertising(
      &connection_lifecycle_listener_, /* use_pin_authentication= */ false,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  // When Nearby Connections advertising is not successful because it times out,
  // advertising will begin like the initial connection flow.
  task_environment_.FastForwardBy(
      kNearbyConnectionsAdvertisementAfterUpdateTimeout);
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(fake_nearby_connections_manager_.IsAdvertising());

  NearbyConnectionsManager::IncomingConnectionListener*
      incoming_connection_listener =
          fake_nearby_connections_manager_.GetAdvertisingListener();
  ASSERT_TRUE(incoming_connection_listener);
  incoming_connection_listener->OnIncomingConnectionInitiated(
      kEndpointId, std::vector<uint8_t>());
  incoming_connection_listener->OnIncomingConnectionAccepted(
      kEndpointId, std::vector<uint8_t>(), &fake_nearby_connection_);

  ASSERT_TRUE(connection());
  EXPECT_TRUE(connection()->WasHandshakeInitiated());
}

}  // namespace ash::quick_start
