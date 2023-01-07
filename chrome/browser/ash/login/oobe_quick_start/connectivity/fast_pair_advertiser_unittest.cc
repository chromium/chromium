// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
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
const uint8_t kFastPairModelId[] = {0x41, 0xc0, 0xd9};

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
    for (auto& observer : observers_)
      observer.AdvertisementReleased(this);
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

  void TestExpectedMetrics(bool should_succeed) {
    if (should_succeed) {
      expected_success_count_++;
      histograms_.ExpectBucketCount(
          "OOBE.QuickStart.FastPair.AdvertisingStart.Result", true,
          expected_success_count_);
      return;
    }

    expected_failure_count_++;
    histograms_.ExpectBucketCount(
        "OOBE.QuickStart.FastPair.AdvertisingStart.Result", false,
        expected_failure_count_);
    histograms_.ExpectBucketCount(
        "OOBE.QuickStart.FastPair.AdvertisingStart.ErrorCode",
        device::BluetoothAdvertisement::ErrorCode::
            INVALID_ADVERTISEMENT_ERROR_CODE,
        expected_failure_count_);
  }

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

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_ = base::WrapUnique(args);
  }

  void StartAdvertising() {
    fast_pair_advertiser_->StartAdvertising(
        base::BindOnce(&FastPairAdvertiserTest::OnStartAdvertising,
                       base::Unretained(this)),
        base::BindOnce(&FastPairAdvertiserTest::OnStartAdvertisingError,
                       base::Unretained(this)),
        RandomSessionId());
    auto service_uuid_list =
        std::make_unique<device::BluetoothAdvertisement::UUIDList>();
    service_uuid_list->push_back(kFastPairServiceUuid);
    EXPECT_EQ(*service_uuid_list, register_args_->service_uuids);

    auto expected_payload = std::vector<uint8_t>(std::begin(kFastPairModelId),
                                                 std::end(kFastPairModelId));
    EXPECT_EQ(expected_payload,
              register_args_->service_data[kFastPairServiceUuid]);
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
      const RandomSessionId& random_id) {
    return fast_pair_advertiser_->GenerateManufacturerMetadata(random_id);
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;
  std::unique_ptr<FastPairAdvertiser> fast_pair_advertiser_;
  std::unique_ptr<RegisterAdvertisementArgs> register_args_;
  base::HistogramTester histograms_;
  bool called_on_start_advertising_ = false;
  bool called_on_start_advertising_error_ = false;
  bool called_on_stop_advertising_ = false;
  base::HistogramBase::Count expected_success_count_ = 0;
  base::HistogramBase::Count expected_failure_count_ = 0;
};

TEST_F(FastPairAdvertiserTest, TestStartAdvertising_Success) {
  StartAdvertising();
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_TRUE(fake_advertisement->HasObserver(fast_pair_advertiser_.get()));
  TestExpectedMetrics(/*should_succeed=*/true);
}

TEST_F(FastPairAdvertiserTest, TestStartAdvertising_Error) {
  StartAdvertising();
  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(called_on_start_advertising());
  EXPECT_TRUE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  TestExpectedMetrics(/*should_succeed=*/false);
}

// Regression test for crbug.com/1109581.
TEST_F(FastPairAdvertiserTest, TestStartAdvertising_DeleteInErrorCallback) {
  fast_pair_advertiser_->StartAdvertising(
      base::DoNothing(),
      base::BindLambdaForTesting([&]() { fast_pair_advertiser_.reset(); }),
      RandomSessionId());

  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(fast_pair_advertiser_);
  TestExpectedMetrics(/*should_succeed=*/false);
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
  RandomSessionId random_id;
  base::span<const uint8_t, RandomSessionId::kLength> random_id_bytes =
      random_id.AsBytes();
  std::vector<uint8_t> manufacturer_metadata =
      GetManufacturerMetadata(random_id);

  EXPECT_EQ(random_id_bytes.size(), manufacturer_metadata.size());
  for (size_t i = 0; i < random_id_bytes.size(); i++) {
    EXPECT_EQ(random_id_bytes[i], manufacturer_metadata[i]);
  }
}

}  // namespace ash::quick_start
