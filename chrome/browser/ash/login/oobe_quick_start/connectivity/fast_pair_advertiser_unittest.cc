// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chromeos/constants/devicetype.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

using ::testing::_;
using testing::NiceMock;
using testing::Return;

constexpr const char kFastPairServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
constexpr uint8_t kFastPairModelIdChromebook[] = {0x30, 0x68, 0x46};
constexpr uint8_t kFastPairModelIdChromebase[] = {0xe9, 0x31, 0x6c};
constexpr uint8_t kFastPairModelIdChromebox[] = {0xda, 0xde, 0x43};

struct ModelIdTestCase {
  chromeos::DeviceType device_type;
  std::vector<uint8_t> model_id;
};

const ModelIdTestCase kModelIdsTestCases[] = {
    {chromeos::DeviceType::kChromebook,
     std::vector<uint8_t>(std::begin(kFastPairModelIdChromebook),
                          std::end(kFastPairModelIdChromebook))},
    {chromeos::DeviceType::kChromebox,
     std::vector<uint8_t>(std::begin(kFastPairModelIdChromebox),
                          std::end(kFastPairModelIdChromebox))},
    {chromeos::DeviceType::kChromebit,
     std::vector<uint8_t>(std::begin(kFastPairModelIdChromebook),
                          std::end(kFastPairModelIdChromebook))},
    {chromeos::DeviceType::kChromebase,
     std::vector<uint8_t>(std::begin(kFastPairModelIdChromebase),
                          std::end(kFastPairModelIdChromebase))},
    {chromeos::DeviceType::kUnknown,
     std::vector<uint8_t>(std::begin(kFastPairModelIdChromebook),
                          std::end(kFastPairModelIdChromebook))},
};

// Sets the simulated device form factor allowing us to verify that the correct
// Fast Pair model ID is used for each one.
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

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdvertisement::ServiceData& service_data,
      device::BluetoothAdapter::CreateAdvertisementCallback callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback)
      : service_uuids(service_uuids),
        service_data(service_data),
        callback(std::move(callback)),
        error_callback(std::move(error_callback)) {}

  device::BluetoothAdvertisement::UUIDList service_uuids;
  device::BluetoothAdvertisement::ServiceData service_data;
  device::BluetoothAdapter::CreateAdvertisementCallback callback;
  device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(RegisterAdvertisementArgs*));

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      device::BluetoothAdapter::CreateAdvertisementCallback callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback)
      override {
    RegisterAdvertisementWithArgsStruct(new RegisterAdvertisementArgs(
        *advertisement_data->service_uuids(),
        *advertisement_data->service_data(), std::move(callback),
        std::move(error_callback)));
  }

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override = default;
};

class FakeBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  // device::BluetoothAdvertisement:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override {
    // Both of these callbacks destroy the BluetoothAdvertisement object, so
    // update the |called_unregister_success_callback_| and
    // |called_unregister_error_callback_| bools immediately before actually
    // invoking their respective callbacks in order to avoid the use of heap
    // allocated memory after it has been freed.
    if (should_unregister_succeed_) {
      called_unregister_success_callback_ = true;
      std::move(success_callback).Run();
      return;
    }

    called_unregister_error_callback_ = true;
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::ErrorCode::
                 INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  bool HasObserver(Observer* observer) {
    return observers_.HasObserver(observer);
  }

  void ReleaseAdvertisement() {
    for (auto& observer : observers_) {
      observer.AdvertisementReleased(this);
    }
  }

  bool called_unregister_success_callback() {
    return called_unregister_success_callback_;
  }

  bool called_unregister_error_callback() {
    return called_unregister_error_callback_;
  }

  void set_should_unregister_succeed(bool should_unregister_succeed) {
    should_unregister_succeed_ = should_unregister_succeed;
  }

 protected:
  ~FakeBluetoothAdvertisement() override = default;

  bool should_unregister_succeed_ = true;
  bool called_unregister_success_callback_ = false;
  bool called_unregister_error_callback_ = false;
};

}  // namespace

class FastPairAdvertiserTest : public testing::Test {
 public:
  FastPairAdvertiserTest(const FastPairAdvertiserTest&) = delete;
  FastPairAdvertiserTest& operator=(const FastPairAdvertiserTest&) = delete;

 protected:
  FastPairAdvertiserTest() = default;

  void SetUp() override {
    mock_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(
            this, &FastPairAdvertiserTest::OnAdapterRegisterAdvertisement));

    fast_pair_advertiser_ = std::make_unique<FastPairAdvertiser>(mock_adapter_);
  }

  void TestExpectedStartMetrics(
      bool should_succeed,
      std::optional<device::BluetoothAdvertisement::ErrorCode> error_code) {
    if (should_succeed) {
      expected_start_success_count_++;
      histograms_.ExpectBucketCount(
          "QuickStart.FastPairAdvertisementStarted.Succeeded", true,
          expected_start_success_count_);
      return;
    }

    expected_start_failure_count_++;
    expected_start_error_bucket_count_++;
    histograms_.ExpectBucketCount(
        "QuickStart.FastPairAdvertisementStarted.Succeeded", false,
        expected_start_failure_count_);
    histograms_.ExpectBucketCount(
        "QuickStart.FastPairAdvertisementStarted.ErrorCode", error_code.value(),
        expected_start_error_bucket_count_);
  }

  void TestExpectedStopMetrics(
      bool should_succeed,
      std::optional<device::BluetoothAdvertisement::ErrorCode> error_code) {
    if (should_succeed) {
      expected_end_success_count_++;
      histograms_.ExpectBucketCount(
          "QuickStart.FastPairAdvertisementEnded.Succeeded", true,
          expected_end_success_count_);
      histograms_.ExpectTotalCount(
          "QuickStart.FastPairAdvertisementEnded.Duration",
          expected_end_success_count_);
      return;
    }

    expected_end_failure_count_++;
    histograms_.ExpectBucketCount(
        "QuickStart.FastPairAdvertisementEnded.Succeeded", false,
        expected_end_failure_count_);
    histograms_.ExpectBucketCount(
        "QuickStart.FastPairAdvertisementEnded.ErrorCode", error_code.value(),
        expected_end_failure_count_);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_ = base::WrapUnique(args);
  }

  void StartAdvertising() {
    fast_pair_advertiser_->StartAdvertising(
        base::BindOnce(&FastPairAdvertiserTest::OnStartAdvertising,
                       base::Unretained(this)),
        base::BindOnce(&FastPairAdvertiserTest::OnStartAdvertisingError,
                       base::Unretained(this)),
        AdvertisingId());
    auto service_uuid_list =
        std::make_unique<device::BluetoothAdvertisement::UUIDList>();
    service_uuid_list->push_back(kFastPairServiceUuid);
    EXPECT_EQ(*service_uuid_list, register_args_->service_uuids);
  }

  void StopAdvertising() {
    fast_pair_advertiser_->StopAdvertising(base::BindOnce(
        &FastPairAdvertiserTest::OnStopAdvertising, base::Unretained(this)));
  }

  void OnStartAdvertising() { called_on_start_advertising_ = true; }

  void OnStartAdvertisingError() { called_on_start_advertising_error_ = true; }

  void OnStopAdvertising() { called_on_stop_advertising_ = true; }

  bool called_on_start_advertising() { return called_on_start_advertising_; }
  bool called_on_start_advertising_error() {
    return called_on_start_advertising_error_;
  }
  bool called_on_stop_advertising() { return called_on_stop_advertising_; }

  std::vector<uint8_t> GetManufacturerMetadata(
      const AdvertisingId& advertising_id) {
    return fast_pair_advertiser_->GenerateManufacturerMetadata(advertising_id);
  }

  void ResetExpectedErrorBucketCount() {
    expected_start_error_bucket_count_ = 0;
  }

  void SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode error_code) {
    StartAdvertising();
    std::move(register_args_->error_callback).Run(error_code);

    EXPECT_FALSE(called_on_start_advertising());
    EXPECT_TRUE(called_on_start_advertising_error());
    TestExpectedStartMetrics(/*should_succeed=*/false, error_code);
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;
  std::unique_ptr<FastPairAdvertiser> fast_pair_advertiser_;
  std::unique_ptr<RegisterAdvertisementArgs> register_args_;
  base::HistogramTester histograms_;
  bool called_on_start_advertising_ = false;
  bool called_on_start_advertising_error_ = false;
  bool called_on_stop_advertising_ = false;
  base::HistogramBase::Count expected_start_success_count_ = 0;
  base::HistogramBase::Count expected_start_failure_count_ = 0;
  base::HistogramBase::Count expected_start_error_bucket_count_ = 0;
  base::HistogramBase::Count expected_end_success_count_ = 0;
  base::HistogramBase::Count expected_end_failure_count_ = 0;
};

class FastPairAdvertiserModelIdsTest
    : public FastPairAdvertiserTest,
      public testing::WithParamInterface<ModelIdTestCase> {
 public:
  void SetUp() override {
    SetDeviceType(GetParam().device_type);
    FastPairAdvertiserTest::SetUp();
  }
};

TEST_F(FastPairAdvertiserTest, TestStartAdvertising_Success) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_TRUE(fake_advertisement->HasObserver(fast_pair_advertiser_.get()));
  TestExpectedStartMetrics(/*should_succeed=*/true,
                           /*error_code=*/std::nullopt);
}

TEST_F(FastPairAdvertiserTest, TestStartAdvertising_Error) {
  StartAdvertising();
  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(called_on_start_advertising());
  EXPECT_TRUE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  TestExpectedStartMetrics(/*should_succeed=*/false,
                           /*error_code=*/device::BluetoothAdvertisement::
                               ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE);
}

TEST_F(FastPairAdvertiserTest, TestStopAdvertising_Success) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  fake_advertisement->set_should_unregister_succeed(true);
  StopAdvertising();

  EXPECT_TRUE(fake_advertisement->called_unregister_success_callback());
  EXPECT_FALSE(fake_advertisement->called_unregister_error_callback());
  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_TRUE(called_on_stop_advertising());
  TestExpectedStopMetrics(/*should_succeed=*/true, /*error_code=*/std::nullopt);
}

TEST_F(FastPairAdvertiserTest, TestStopAdvertising_Error) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  fake_advertisement->set_should_unregister_succeed(false);
  StopAdvertising();

  EXPECT_FALSE(fake_advertisement->called_unregister_success_callback());
  EXPECT_TRUE(fake_advertisement->called_unregister_error_callback());
  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_TRUE(called_on_stop_advertising());
  TestExpectedStopMetrics(/*should_succeed=*/false,
                          /*error_code=*/device::BluetoothAdvertisement::
                              ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE);
}

TEST_F(FastPairAdvertiserTest, TestAdvertisementReleased) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(fake_advertisement->HasObserver(fast_pair_advertiser_.get()));

  fake_advertisement->ReleaseAdvertisement();

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_FALSE(fake_advertisement->HasObserver(fast_pair_advertiser_.get()));
}

TEST_F(FastPairAdvertiserTest, TestGenerateManufacturerMetadata) {
  AdvertisingId advertising_id;
  base::span<const uint8_t, AdvertisingId::kLength> advertising_id_bytes =
      advertising_id.AsBytes();
  std::vector<uint8_t> manufacturer_metadata =
      GetManufacturerMetadata(advertising_id);

  EXPECT_EQ(advertising_id_bytes.size(), manufacturer_metadata.size());
  for (size_t i = 0; i < advertising_id_bytes.size(); i++) {
    EXPECT_EQ(advertising_id_bytes[i], manufacturer_metadata[i]);
  }
}

TEST_F(FastPairAdvertiserTest, TestFastPairAdvertisingRegisteringErrors) {
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::ERROR_UNSUPPORTED_PLATFORM);
  // Reset bucket count because we are checking for a different advertising
  // error bucket.
  ResetExpectedErrorBucketCount();
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::
          ERROR_ADVERTISEMENT_ALREADY_EXISTS);

  ResetExpectedErrorBucketCount();
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::
          ERROR_ADVERTISEMENT_INVALID_LENGTH);

  ResetExpectedErrorBucketCount();
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::ERROR_STARTING_ADVERTISEMENT);

  ResetExpectedErrorBucketCount();
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::ERROR_ADAPTER_POWERED_OFF);

  ResetExpectedErrorBucketCount();
  SetAdvertisementErrorsForMetricsTesting(
      device::BluetoothAdvertisement::ErrorCode::
          ERROR_INVALID_ADVERTISEMENT_INTERVAL);
}

// Regression tests for crashes when accessing member variables after
// destruction, e.g. crbug.com/1109581.
TEST_F(FastPairAdvertiserTest, TestStartAdvertising_DeleteInErrorCallback) {
  fast_pair_advertiser_->StartAdvertising(
      base::DoNothing(),
      base::BindLambdaForTesting([&]() { fast_pair_advertiser_.reset(); }),
      AdvertisingId());

  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(fast_pair_advertiser_);
  TestExpectedStartMetrics(/*should_succeed=*/false,
                           /*error_code=*/device::BluetoothAdvertisement::
                               ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE);
}

TEST_F(FastPairAdvertiserTest, TestStopAdvertisingSuccess_DeleteInCallback) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  fake_advertisement->set_should_unregister_succeed(true);
  fast_pair_advertiser_->StopAdvertising(
      base::BindLambdaForTesting([&]() { fast_pair_advertiser_.reset(); }));

  EXPECT_TRUE(fake_advertisement->called_unregister_success_callback());
  EXPECT_FALSE(fake_advertisement->called_unregister_error_callback());
  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  TestExpectedStopMetrics(/*should_succeed=*/true,
                          /*error_code=*/std::nullopt);
}

TEST_F(FastPairAdvertiserTest, TestStopAdvertisingError_DeleteInCallback) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  fake_advertisement->set_should_unregister_succeed(false);
  fast_pair_advertiser_->StopAdvertising(
      base::BindLambdaForTesting([&]() { fast_pair_advertiser_.reset(); }));

  EXPECT_FALSE(fake_advertisement->called_unregister_success_callback());
  EXPECT_TRUE(fake_advertisement->called_unregister_error_callback());
  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  TestExpectedStopMetrics(/*should_succeed=*/false,
                          /*error_code=*/device::BluetoothAdvertisement::
                              ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE);
}

TEST_P(FastPairAdvertiserModelIdsTest, ModelIds) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);
  EXPECT_EQ(GetParam().model_id,
            register_args_->service_data[kFastPairServiceUuid]);
}

INSTANTIATE_TEST_SUITE_P(FastPairAdvertiserModelIdsTest,
                         FastPairAdvertiserModelIdsTest,
                         testing::ValuesIn(kModelIdsTestCases));

}  // namespace ash::quick_start
