// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge/fake_concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

constexpr int64_t kGiB = 1024 * 1024 * 1024;

class FreeSpaceProviderMock
    : public BorealisDiskManagerImpl::FreeSpaceProvider {
 public:
  FreeSpaceProviderMock() = default;
  ~FreeSpaceProviderMock() override = default;
  MOCK_METHOD(void, Get, (base::OnceCallback<void(int64_t)>), ());
};

using DiskInfoCallbackFactory = StrictCallbackFactory<void(
    Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>)>;

using RequestDeltaCallbackFactory =
    StrictCallbackFactory<void(Expected<uint64_t, std::string>)>;

class BorealisDiskDispatcherMock : public BorealisDiskManagerDispatcher {
 public:
  BorealisDiskDispatcherMock() = default;
  ~BorealisDiskDispatcherMock() = default;

  MOCK_METHOD(
      void,
      GetDiskInfo,
      (const std::string&,
       const std::string&,
       base::OnceCallback<void(
           Expected<BorealisDiskManager::GetDiskInfoResponse, std::string>)>),
      ());
  MOCK_METHOD(void,
              RequestSpace,
              (const std::string&,
               const std::string&,
               uint64_t,
               base::OnceCallback<void(Expected<uint64_t, std::string>)>),
              ());
  MOCK_METHOD(void,
              ReleaseSpace,
              (const std::string&,
               const std::string&,
               uint64_t,
               base::OnceCallback<void(Expected<uint64_t, std::string>)>),
              ());
  MOCK_METHOD(void,
              SetDiskManagerDelegate,
              (BorealisDiskManager * disk_manager),
              ());
  MOCK_METHOD(void,
              RemoveDiskManagerDelegate,
              (BorealisDiskManager * disk_manager),
              ());
};

class BorealisDiskManagerTest : public testing::Test {
 public:
  BorealisDiskManagerTest() = default;
  ~BorealisDiskManagerTest() override = default;
  BorealisDiskManagerTest(const BorealisDiskManagerTest&) = delete;
  BorealisDiskManagerTest& operator=(const BorealisDiskManagerTest&) = delete;

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::CiceroneClient::InitializeFake();
    chromeos::ConciergeClient::InitializeFake();
    chromeos::SeneschalClient::InitializeFake();
    fake_concierge_client_ = chromeos::FakeConciergeClient::Get();
    CreateProfile();
    mock_dispatcher_ =
        std::make_unique<testing::NiceMock<BorealisDiskDispatcherMock>>();
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(profile_.get());
    borealis_features_ = std::make_unique<BorealisFeatures>(profile_.get());

    service_fake_ = BorealisServiceFake::UseFakeForTesting(profile_.get());
    service_fake_->SetDiskManagerDispatcherForTesting(mock_dispatcher_.get());
    service_fake_->SetWindowManagerForTesting(borealis_window_manager_.get());
    service_fake_->SetFeaturesForTesting(borealis_features_.get());

    context_ = BorealisContext::CreateBorealisContextForTesting(profile_.get());
    context_->set_vm_name("vm_name1");
    disk_manager_ = std::make_unique<BorealisDiskManagerImpl>(context_.get());
    auto free_space_provider = std::make_unique<FreeSpaceProviderMock>();
    free_space_provider_ = free_space_provider.get();
    disk_manager_->SetFreeSpaceProviderForTesting(
        std::move(free_space_provider));
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    disk_manager_.reset();
    context_.reset();
    profile_.reset();
    run_loop_.reset();
    chromeos::SeneschalClient::Shutdown();
    chromeos::ConciergeClient::Shutdown();
    chromeos::CiceroneClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }

  vm_tools::concierge::ListVmDisksResponse BuildListVmDisksResponse(
      bool success,
      const std::string& vm_name,
      vm_tools::concierge::DiskImageType image_type,
      int64_t min_size,
      int64_t size,
      int64_t available_space) {
    vm_tools::concierge::ListVmDisksResponse response;
    auto* image = response.add_images();
    response.set_success(success);
    image->set_name(vm_name);
    image->set_image_type(image_type);
    image->set_min_size(min_size);
    image->set_size(size);
    image->set_available_space(available_space);
    return response;
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisContext> context_;
  std::unique_ptr<BorealisDiskManagerImpl> disk_manager_;
  FreeSpaceProviderMock* free_space_provider_;
  BorealisServiceFake* service_fake_;
  std::unique_ptr<testing::NiceMock<BorealisDiskDispatcherMock>>
      mock_dispatcher_;
  std::unique_ptr<BorealisFeatures> borealis_features_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  std::unique_ptr<base::RunLoop> run_loop_;
  content::BrowserTaskEnvironment task_environment_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeConciergeClient* fake_concierge_client_;
};

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnFreeSpaceProviderError) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(testing::Invoke([](base::OnceCallback<void(int64_t)> callback) {
        std::move(callback).Run(-1);
      }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnNoResponseFromConcierge) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            // Concierge will return an empty ListVmDisksResponse.
            fake_concierge_client_->set_list_vm_disks_response(
                absl::optional<vm_tools::concierge::ListVmDisksResponse>());
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       GetDiskInfoFailsOnUnsuccessfulResponseFromConcierge) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/false, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                    /*available_space=*/1 * kGiB));
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnVmMismatch) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true,
                    /*vm_name=*/"UNMATCHED_VM", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                    /*available_space=*/1 * kGiB));
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoSucceedsAndReturnsResponse) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            // 3GB of disk space less 2GB of buffer is 1GB of available space.
            EXPECT_EQ(response_or_error.Value().available_bytes, 1 * kGiB);
            // 2GB of expandable space less 1GB of headroom is 1GB of expandable
            // space.
            EXPECT_EQ(response_or_error.Value().expandable_bytes, 1 * kGiB);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnConcurrentAttempt) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory first_callback_factory;
  DiskInfoCallbackFactory second_callback_factory;
  EXPECT_CALL(first_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  EXPECT_CALL(second_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->GetDiskInfo(first_callback_factory.BindOnce());
  disk_manager_->GetDiskInfo(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoSubsequentAttemptSucceeds) {
  vm_tools::concierge::ListVmDisksResponse response = BuildListVmDisksResponse(
      /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
      vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
      /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
      /*available_space=*/3 * kGiB);

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this, response = response](
                              base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory first_callback_factory;
  EXPECT_CALL(first_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  disk_manager_->GetDiskInfo(first_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this, response = response](
                              base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  disk_manager_->GetDiskInfo(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsIfRequestExceedsInt64) {
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->ReleaseSpace(uint64_t(std::numeric_limits<int64_t>::max()) + 1,
                              callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfBuildDiskInfoFails) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true,
                    /*vm_name=*/"UNMATCHED_VM", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfDiskTypeNotRaw) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_AUTO,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfRequestTooHigh) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  // 6GB > 4GB of expandable space.
  disk_manager_->RequestSpace(6 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       RequestDeltaFailsIfRequestWouldNotLeaveEnoughSpace) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  // Release space is requesting a negative delta. 2GB > 1GB of unused available
  // space.
  disk_manager_->ReleaseSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfRequestIsBelowMinimum) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/7 * kGiB,
                    /*available_space=*/10 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  // Release space is requesting a negative delta. 7GB-2GB < 6GB min_size.
  disk_manager_->ReleaseSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnNoResizeDiskResponse) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnFailedResizeDiskResponse) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnDelayedConciergeFailure) {
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  vm_tools::concierge::DiskImageStatusResponse in_progress;
  vm_tools::concierge::DiskImageStatusResponse failed;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  in_progress.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  failed.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals{in_progress,
                                                                    failed};
  fake_concierge_client_->set_resize_disk_image_response(disk_response);
  fake_concierge_client_->set_disk_image_status_signals(signals);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnFailureToGetUpdate) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(testing::Invoke([](base::OnceCallback<void(int64_t)> callback) {
        std::move(callback).Run(-1 * kGiB);
      }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceFailsIfResizeTooSmall) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                    /*available_space=*/4 * kGiB));
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsIfDiskExpanded) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/4 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                    /*available_space=*/5 * kGiB));
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceSuccessful) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                    /*available_space=*/5 * kGiB));
            std::move(callback).Run(3 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 2 * kGiB);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceSuccessful) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/4 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/19 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(6 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaConcurrentAttemptFails) {
  // We don't use a sequence here because the ordering of expectations is not
  // guaranteed, because of that we need to declare the free space provider
  // expectations in reverse order and retire the first expecation when
  // fulfilled.
  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                    /*available_space=*/5 * kGiB));
            std::move(callback).Run(3 * kGiB);
          }));

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }))
      .RetiresOnSaturation();

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_TRUE(response_or_error);
          }));
  RequestDeltaCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_FALSE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  disk_manager_->RequestSpace(2 * kGiB, second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaSubsequentAttemptSucceeds) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                    /*available_space=*/3 * kGiB));
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                    /*available_space=*/5 * kGiB));
            std::move(callback).Run(3 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_TRUE(response_or_error);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                    /*available_space=*/5 * kGiB));
            std::move(callback).Run(3 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse second_disk_response;
  second_disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(second_disk_response);

  EXPECT_CALL(*free_space_provider_, Get(testing::_))
      .WillOnce(
          testing::Invoke([this](base::OnceCallback<void(int64_t)> callback) {
            fake_concierge_client_->set_list_vm_disks_response(
                BuildListVmDisksResponse(
                    /*success=*/true, /*vm_name=*/"vm_name1", /*image_type=*/
                    vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW,
                    /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                    /*available_space=*/4 * kGiB));
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, std::string> response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

}  // namespace
}  // namespace borealis
