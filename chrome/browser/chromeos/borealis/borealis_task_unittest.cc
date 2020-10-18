// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_task.h"

#include <memory>

#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::StrNe;

namespace borealis {

namespace {

class CallbackForTesting {
 public:
  BorealisTask::CompletionStatusCallback GetCallback() {
    return base::BindOnce(&CallbackForTesting::Callback,
                          base::Unretained(this));
  }

  MOCK_METHOD(void,
              Callback,
              (BorealisContextManager::Status, std::string),
              ());
};

class BorealisTasksTest : public testing::Test {
 public:
  BorealisTasksTest() = default;
  ~BorealisTasksTest() override = default;

  // Disallow copy and assign.
  BorealisTasksTest(const BorealisTasksTest&) = delete;
  BorealisTasksTest& operator=(const BorealisTasksTest&) = delete;

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
    CreateProfile();
    context_ = BorealisContext::CreateBorealisContextForTesting();
    context_->set_profile(profile_.get());

    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
  }

  void TearDown() override {
    profile_.reset();

    chromeos::DlcserviceClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  std::unique_ptr<TestingProfile> profile_;
  BorealisContext* context_;
  content::BrowserTaskEnvironment task_environment_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeConciergeClient* fake_concierge_client_;
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }
};

TEST_F(BorealisTasksTest, MountDlcSucceedsAndCallbackRanWithResults) {
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  fake_dlcservice_client_->set_install_root_path("test/path");
  EXPECT_EQ(context_->root_path(), "");

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(BorealisContextManager::kSuccess, _));

  MountDlc task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(context_->root_path(), "test/path");
}

TEST_F(BorealisTasksTest, CreateDiskSucceedsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  base::FilePath path = base::FilePath("test/path");
  response.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  response.set_disk_path(path.AsUTF8Unsafe());
  fake_concierge_client_->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(BorealisContextManager::kSuccess, _));

  CreateDiskImage task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_EQ(context_->disk_path(), path);
}

TEST_F(BorealisTasksTest,
       CreateDiskImageAlreadyExistsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  base::FilePath path = base::FilePath("test/path");
  response.set_status(vm_tools::concierge::DISK_STATUS_EXISTS);
  response.set_disk_path(path.AsUTF8Unsafe());
  fake_concierge_client_->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(BorealisContextManager::kSuccess, _));

  CreateDiskImage task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_EQ(context_->disk_path(), path);
}

TEST_F(BorealisTasksTest, StartBorealisVmSucceedsAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_STARTING);
  fake_concierge_client_->set_start_vm_response(std::move(response));

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(BorealisContextManager::kSuccess, _));

  StartBorealisVm task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
}

TEST_F(BorealisTasksTest,
       StartBorealisVmVmAlreadyRunningAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  fake_concierge_client_->set_start_vm_response(std::move(response));

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(BorealisContextManager::kSuccess, _));

  StartBorealisVm task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
}

class BorealisTasksTestDlc : public BorealisTasksTest,
                             public testing::WithParamInterface<std::string> {};

TEST_P(BorealisTasksTestDlc, MountDlcFailsAndCallbackRanWithResults) {
  fake_dlcservice_client_->set_install_error(GetParam());
  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback,
              Callback(BorealisContextManager::kMountFailed, StrNe("")));

  MountDlc task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(BorealisTasksTestDlcErrors,
                         BorealisTasksTestDlc,
                         testing::Values(dlcservice::kErrorInternal,
                                         dlcservice::kErrorInvalidDlc,
                                         dlcservice::kErrorBusy,
                                         dlcservice::kErrorNeedReboot,
                                         dlcservice::kErrorAllocation,
                                         "unknown"));

class BorealisTasksTestDiskImage
    : public BorealisTasksTest,
      public testing::WithParamInterface<vm_tools::concierge::DiskImageStatus> {
};

TEST_P(BorealisTasksTestDiskImage, CreateDiskFailsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  response.set_status(GetParam());
  fake_concierge_client_->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback,
              Callback(BorealisContextManager::kDiskImageFailed, StrNe("")));

  CreateDiskImage task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_EQ(context_->disk_path(), base::FilePath());
}

INSTANTIATE_TEST_SUITE_P(
    BorealisTasksTestCreateDiskImageErrors,
    BorealisTasksTestDiskImage,
    testing::Values(vm_tools::concierge::DISK_STATUS_UNKNOWN,
                    vm_tools::concierge::DISK_STATUS_FAILED,
                    vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST,
                    vm_tools::concierge::DISK_STATUS_DESTROYED,
                    vm_tools::concierge::DISK_STATUS_IN_PROGRESS,
                    vm_tools::concierge::DISK_STATUS_RESIZED));

class BorealisTasksTestsStartBorealisVm
    : public BorealisTasksTest,
      public testing::WithParamInterface<vm_tools::concierge::VmStatus> {};

TEST_P(BorealisTasksTestsStartBorealisVm,
       StartBorealisVmErrorsAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(GetParam());
  fake_concierge_client_->set_start_vm_response(std::move(response));

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback,
              Callback(BorealisContextManager::kStartVmFailed, StrNe("")));

  StartBorealisVm task;
  task.Run(context_, callback.GetCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
}

INSTANTIATE_TEST_SUITE_P(
    BorealisTasksTestStartBorealisVmErrors,
    BorealisTasksTestsStartBorealisVm,
    testing::Values(vm_tools::concierge::VM_STATUS_UNKNOWN,
                    vm_tools::concierge::VM_STATUS_FAILURE));
}  // namespace
}  // namespace borealis
