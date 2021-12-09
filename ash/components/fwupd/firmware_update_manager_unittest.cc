// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeDeviceIdForTesting[] = "Fake_Device_ID";
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
const char kDownloadDir[] = "firmware-updates";
const char kCacheDir[] = "cache";
const char kCabExtension[] = ".cab";

void RunResponseCallback(dbus::ObjectProxy::ResponseOrErrorCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  std::move(callback).Run(response.get(), nullptr);
}

class FakeUpdateObserver : public ash::firmware_update::mojom::UpdateObserver {
 public:
  void OnUpdateListChanged(
      std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr>
          firmware_updates) override {
    updates_ = std::move(firmware_updates);
  }

  mojo::PendingRemote<ash::firmware_update::mojom::UpdateObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr>& updates()
      const {
    return updates_;
  }

 private:
  std::vector<ash::firmware_update::mojom::FirmwareUpdatePtr> updates_;
  mojo::Receiver<ash::firmware_update::mojom::UpdateObserver> receiver_{this};
};

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

    EXPECT_CALL(*proxy_, DoConnectToSignal(_, _, _, _))
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
  void StartInstall(const std::string& device_id, int release) {
    base::RunLoop loop;
    firmware_update_manager_->StartInstall(
        device_id, release,
        base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                       loop.QuitClosure()));
    loop.Run();
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

  void SetupObserver(FakeUpdateObserver* observer) {
    firmware_update_manager_->ObservePeripheralUpdates(
        observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  // `FwupdClient` must be be before `FirmwareUpdateManager`.
  std::unique_ptr<FwupdClient> dbus_client_;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;

  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Fake responses.
  std::deque<std::unique_ptr<dbus::Response>> dbus_responses_;

  base::test::TaskEnvironment task_environment_;
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
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceNoUpdates) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(updates.empty());
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesOneDeviceOneUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateOneDeviceResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());
  EXPECT_EQ(kFakeDeviceIdForTesting, updates[0]->device_id);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeDeviceNameForTesting)),
            updates[0]->device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0]->device_version);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeUpdateDescriptionForTesting)),
            updates[0]->device_description);
  EXPECT_EQ(ash::firmware_update::mojom::UpdatePriority(
                kFakeUpdatePriorityForTesting),
            updates[0]->priority);
}

TEST_F(FirmwareUpdateManagerTest, RequestAllUpdatesTwoDeviceOneWithUpdate) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(CreateTwoDeviceResponse());
  dbus_responses_.push_back(CreateNoUpdateResponse());
  dbus_responses_.push_back(CreateOneUpdateResponse());
  firmware_update_manager_->RequestAllUpdates();
  FakeUpdateObserver update_observer;
  SetupObserver(&update_observer);
  base::RunLoop().RunUntilIdle();

  const std::vector<firmware_update::mojom::FirmwareUpdatePtr>& updates =
      update_observer.updates();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, updates.size());

  // The second device was the one with the update.
  EXPECT_EQ(std::string(kFakeDeviceIdForTesting) + "2", updates[0]->device_id);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeDeviceNameForTesting) + "2"),
            updates[0]->device_name);
  EXPECT_EQ(kFakeUpdateVersionForTesting, updates[0]->device_version);
  EXPECT_EQ(base::UTF8ToUTF16(std::string(kFakeUpdateDescriptionForTesting)),
            updates[0]->device_description);
  EXPECT_EQ(ash::firmware_update::mojom::UpdatePriority(
                kFakeUpdatePriorityForTesting),
            updates[0]->priority);
}

TEST_F(FirmwareUpdateManagerTest, RequestInstall) {
  EXPECT_CALL(*proxy_, DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &FirmwareUpdateManagerTest::OnMethodCalled));

  dbus_responses_.push_back(dbus::Response::CreateEmpty());

  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath root_path =
      root_dir.Append(FILE_PATH_LITERAL(kDownloadDir))
          .Append(FILE_PATH_LITERAL(kCacheDir));

  const std::string test_filename =
      std::string(kFakeDeviceIdForTesting) + std::string(kCabExtension);
  base::FilePath full_path = root_path.Append(test_filename);
  // Create a temporary file to simulate a .cab available for install.
  base::WriteFile(full_path, "", 0);
  EXPECT_TRUE(base::PathExists(full_path));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetOnInstallResponseCallbackCallCountForTesting());
  StartInstall(std::string(kFakeDeviceIdForTesting), /*release=*/0);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetOnInstallResponseCallbackCallCountForTesting());
}

}  // namespace ash
