// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeDeviceIdForTesting[] = "Fake Device ID";
const char kFakeDeviceNameForTesting[] = "Fake Device Name";
const char kFakeUpdateDescriptionForTesting[] =
    "This is a fake update for testing.";
const uint32_t kFakeUpdatePriorityForTesting = 1;
const char kFakeUpdateVersionForTesting[] = "1.0.0";
const char kFwupdServiceName[] = "org.freedesktop.fwupd";
const char kFwupdServicePath[] = "/";
const char kDescriptionKey[] = "Description";
const char kIdKey[] = "DeviceId";
const char kNameKey[] = "Name";
const char kPriorityKey[] = "Urgency";
const char kVersionKey[] = "Version";

void RunResponseCallback(dbus::ObjectProxy::ResponseOrErrorCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get(), nullptr);
}

}  // namespace

using chromeos::FwupdClient;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

class FirmwareUpdateManagerTest : public testing::Test {
 public:
  FirmwareUpdateManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::ash::features::kFirmwareUpdaterApp);

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = base::MakeRefCounted<dbus::MockBus>(options);

    dbus::ObjectPath fwupd_service_path(kFwupdServicePath);
    proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kFwupdServiceName, fwupd_service_path);

    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(kFwupdServiceName, fwupd_service_path))
        .WillRepeatedly(testing::Return(proxy_.get()));

    EXPECT_CALL(*proxy_, DoConnectToSignal(kFwupdServiceName, _, _, _))
        .WillRepeatedly(Return());

    dbus_client_ = FwupdClient::Create();
    dbus_client_->InitForTesting(bus_.get());
    firmware_update_manager_ = std::make_unique<FirmwareUpdateManager>();
  }
  FirmwareUpdateManagerTest(const FirmwareUpdateManagerTest&) = delete;
  FirmwareUpdateManagerTest& operator=(const FirmwareUpdateManagerTest&) =
      delete;
  ~FirmwareUpdateManagerTest() override = default;

  void OnMethodCalled(dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    ASSERT_FALSE(dbus_responses_.empty());
    auto response = std::move(dbus_responses_.front());
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RunResponseCallback, std::move(*callback),
                                  std::move(response)));
    dbus_responses_.pop_front();
  }

 protected:
  void InstallUpdate(base::ScopedFD fd, std::map<std::string, bool> options) {
    firmware_update_manager_->InstallUpdate(
        kFakeDeviceIdForTesting, std::move(fd), std::map<std::string, bool>());
  }

  std::unique_ptr<dbus::Response> CreateEmptyDeviceResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneDeviceResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(kFakeDeviceNameForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(kFakeDeviceIdForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateTwoDeviceResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceNameForTesting) +
                                      "1");
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceIdForTesting) +
                                      "1");
    device_array_writer.CloseContainer(&dict_writer);

    // Prepare the next device entry.
    response_array_writer.CloseContainer(&device_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kNameKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceNameForTesting) +
                                      "2");
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kIdKey);
    dict_writer.AppendVariantOfString(std::string(kFakeDeviceIdForTesting) +
                                      "2");
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateOneUpdateResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);
    dbus::MessageWriter dict_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kDescriptionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateDescriptionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kVersionKey);
    dict_writer.AppendVariantOfString(kFakeUpdateVersionForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    device_array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(kPriorityKey);
    dict_writer.AppendVariantOfUint32(kFakeUpdatePriorityForTesting);
    device_array_writer.CloseContainer(&dict_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateNoUpdateResponse() {
    auto response = dbus::Response::CreateEmpty();

    dbus::MessageWriter response_writer(response.get());
    dbus::MessageWriter response_array_writer(nullptr);
    dbus::MessageWriter device_array_writer(nullptr);

    // The response is an array of arrays of dictionaries. Each dictionary is
    // one device description.
    response_writer.OpenArray("a{sv}", &response_array_writer);
    response_array_writer.OpenArray("{sv}", &device_array_writer);

    response_array_writer.CloseContainer(&device_array_writer);
    response_writer.CloseContainer(&response_array_writer);

    return response;
  }

  std::unique_ptr<dbus::Response> CreateBoolResponse(bool success) {
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter response_writer(response.get());
    response_writer.AppendBool(success);
    return response;
  }

  int GetOnInstallResponseCallbackCallCountForTesting() {
    return firmware_update_manager_
        ->on_install_update_response_count_for_testing_;
  }

  // `FwupdClient` must be be before `FirmwareUpdateManager`.
  std::unique_ptr<FwupdClient> dbus_client_;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;

  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Fake responses.
  std::deque<std::unique_ptr<dbus::Response>> dbus_responses_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FirmwareUpdateManagerTest, CorrectMockInstance) {
  EXPECT_EQ(dbus_client_.get(), FwupdClient::Get());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesNoDevices) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateEmptyDeviceResponse());
  firmware_update_manager_->RequestAllUpdates();
  const std::vector<FirmwareUpdateManager::FirmwareUpdate>& updates =
      firmware_update_manager_->GetCachedUpdatesForTesting();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceNoUpdates) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  const std::vector<FirmwareUpdateManager::FirmwareUpdate>& updates =
      firmware_update_manager_->GetCachedUpdatesForTesting();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceOneUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  const std::vector<FirmwareUpdateManager::FirmwareUpdate>& updates =
      firmware_update_manager_->GetCachedUpdatesForTesting();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());

  EXPECT_EQ(kFakeDeviceIdForTesting, updates[0].device_id);
  EXPECT_EQ(kFakeDeviceNameForTesting, updates[0].device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0].version);
  EXPECT_EQ(kFakeUpdateDescriptionForTesting, updates[0].description);
  EXPECT_EQ(kFakeUpdatePriorityForTesting, updates[0].priority);
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesTwoDeviceOneWithUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateTwoDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  const std::vector<FirmwareUpdateManager::FirmwareUpdate>& updates =
      firmware_update_manager_->GetCachedUpdatesForTesting();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());

  // The second device was the one with the update.
  EXPECT_EQ(std::string(kFakeDeviceIdForTesting) + "2", updates[0].device_id);
  EXPECT_EQ(std::string(kFakeDeviceNameForTesting) + "2",
            updates[0].device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0].version);
  EXPECT_EQ(kFakeUpdateDescriptionForTesting, updates[0].description);
  EXPECT_EQ(kFakeUpdatePriorityForTesting, updates[0].priority);
}

// TODO(jimmyxgong): Rewrite this test with an observer.
TEST_F(FirmwareUpdateManagerTest, RequestUpdateList) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateBoolResponse(/**install_success=*/true));

  EXPECT_EQ(0, GetOnInstallResponseCallbackCallCountForTesting());
  InstallUpdate(base::ScopedFD(0), std::map<std::string, bool>());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetOnInstallResponseCallbackCallCountForTesting());
}

}  // namespace ash
