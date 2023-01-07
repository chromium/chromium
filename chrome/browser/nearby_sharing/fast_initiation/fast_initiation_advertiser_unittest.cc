// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_advertiser.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using testing::NiceMock;
using testing::Return;

constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};

// Metadata bytes translate to 0b00000000 and 0b10111110, indicating "version
// 0", "type 0 (notify)", and "transmission power of 66".
const uint8_t kFastInitMetadataTypeNotify[] = {0x00, 0x42};

// Metadata bytes translate to 0b00000100 and 0b10111110, indicating "version
// 0", "type 1 (silent)", and "transmission power of 66".
const uint8_t kFastInitMetadataTypeSilent[] = {0x04, 0x42};

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
    std::move(success_callback).Run();
  }

  bool HasObserver(Observer* observer) {
    return observers_.HasObserver(observer);
  }

  void ReleaseAdvertisement() {
    for (auto& observer : observers_)
      observer.AdvertisementReleased(this);
  }

 protected:
  ~FakeBluetoothAdvertisement() override = default;
};

}  // namespace

class NearbySharingFastInitiationAdvertiserTest : public testing::Test {
 public:
  NearbySharingFastInitiationAdvertiserTest(
      const NearbySharingFastInitiationAdvertiserTest&) = delete;
  NearbySharingFastInitiationAdvertiserTest& operator=(
      const NearbySharingFastInitiationAdvertiserTest&) = delete;

 protected:
  NearbySharingFastInitiationAdvertiserTest() = default;

  void SetUp() override {
    mock_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(this, &NearbySharingFastInitiationAdvertiserTest::
                                        OnAdapterRegisterAdvertisement));

    fast_initiation_advertiser_ =
        std::make_unique<FastInitiationAdvertiser>(mock_adapter_);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_ = base::WrapUnique(args);
  }

  void StartAdvertising(FastInitiationAdvertiser::FastInitType type) {
    fast_initiation_advertiser_->StartAdvertising(
        type,
        base::BindOnce(
            &NearbySharingFastInitiationAdvertiserTest::OnStartAdvertising,
            base::Unretained(this)),
        base::BindOnce(
            &NearbySharingFastInitiationAdvertiserTest::OnStartAdvertisingError,
            base::Unretained(this)));
    auto service_uuid_list =
        std::make_unique<device::BluetoothAdvertisement::UUIDList>();
    service_uuid_list->push_back(kNearbySharingFastInitiationServiceUuid);
    EXPECT_EQ(*service_uuid_list, register_args_->service_uuids);

    auto expected_payload =
        std::vector<uint8_t>(std::begin(kNearbySharingFastPairId),
                             std::end(kNearbySharingFastPairId));
    if (type == FastInitiationAdvertiser::FastInitType::kNotify) {
      expected_payload.insert(std::end(expected_payload),
                              std::begin(kFastInitMetadataTypeNotify),
                              std::end(kFastInitMetadataTypeNotify));
    } else {
      expected_payload.insert(std::end(expected_payload),
                              std::begin(kFastInitMetadataTypeSilent),
                              std::end(kFastInitMetadataTypeSilent));
    }

    EXPECT_EQ(
        expected_payload,
        register_args_->service_data[kNearbySharingFastInitiationServiceUuid]);
  }

  void StopAdvertising() {
    fast_initiation_advertiser_->StopAdvertising(base::BindOnce(
        &NearbySharingFastInitiationAdvertiserTest::OnStopAdvertising,
        base::Unretained(this)));
  }

  void OnStartAdvertising() { called_on_start_advertising_ = true; }

  void OnStartAdvertisingError() { called_on_start_advertising_error_ = true; }

  void OnStopAdvertising() { called_on_stop_advertising_ = true; }

  bool called_on_start_advertising() { return called_on_start_advertising_; }
  bool called_on_start_advertising_error() {
    return called_on_start_advertising_error_;
  }
  bool called_on_stop_advertising() { return called_on_stop_advertising_; }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;
  std::unique_ptr<FastInitiationAdvertiser> fast_initiation_advertiser_;
  std::unique_ptr<RegisterAdvertisementArgs> register_args_;
  bool called_on_start_advertising_ = false;
  bool called_on_start_advertising_error_ = false;
  bool called_on_stop_advertising_ = false;
};

TEST_F(NearbySharingFastInitiationAdvertiserTest,
       TestStartAdvertising_Success_TypeNotify) {
  StartAdvertising(FastInitiationAdvertiser::FastInitType::kNotify);
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_TRUE(
      fake_advertisement->HasObserver(fast_initiation_advertiser_.get()));
}

TEST_F(NearbySharingFastInitiationAdvertiserTest,
       TestStartAdvertising_Success_TypeSilent) {
  StartAdvertising(FastInitiationAdvertiser::FastInitType::kSilent);
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_TRUE(
      fake_advertisement->HasObserver(fast_initiation_advertiser_.get()));
}

TEST_F(NearbySharingFastInitiationAdvertiserTest, TestStartAdvertising_Error) {
  StartAdvertising(FastInitiationAdvertiser::FastInitType::kNotify);
  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(called_on_start_advertising());
  EXPECT_TRUE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
}

// Regression test for crbug.com/1109581.
TEST_F(NearbySharingFastInitiationAdvertiserTest,
       TestStartAdvertising_DeleteInErrorCallback) {
  fast_initiation_advertiser_->StartAdvertising(
      FastInitiationAdvertiser::FastInitType::kNotify, base::DoNothing(),
      base::BindLambdaForTesting(
          [&]() { fast_initiation_advertiser_.reset(); }));

  std::move(register_args_->error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::
               INVALID_ADVERTISEMENT_ERROR_CODE);

  EXPECT_FALSE(fast_initiation_advertiser_);
}

TEST_F(NearbySharingFastInitiationAdvertiserTest, TestStopAdvertising) {
  StartAdvertising(FastInitiationAdvertiser::FastInitType::kNotify);
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  StopAdvertising();

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_TRUE(called_on_stop_advertising());
}

TEST_F(NearbySharingFastInitiationAdvertiserTest, TestAdvertisementReleased) {
  StartAdvertising(FastInitiationAdvertiser::FastInitType::kNotify);
  auto fake_advertisement = base::MakeRefCounted<FakeBluetoothAdvertisement>();
  std::move(register_args_->callback).Run(fake_advertisement);

  EXPECT_TRUE(
      fake_advertisement->HasObserver(fast_initiation_advertiser_.get()));

  fake_advertisement->ReleaseAdvertisement();

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
  EXPECT_FALSE(
      fake_advertisement->HasObserver(fast_initiation_advertiser_.get()));
}
