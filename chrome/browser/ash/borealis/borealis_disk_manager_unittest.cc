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
  ~FreeSpaceProviderMock() = default;
  MOCK_METHOD(void, Get, (base::OnceCallback<void(int64_t)>), ());
};

class CallbackFactory
    : public testing::StrictMock<testing::MockFunction<void(
          Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                   std::string>)>> {
 public:
  base::OnceCallback<
      void(Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>)>
  BindOnce() {
    return base::BindOnce(&CallbackFactory::Call, base::Unretained(this));
  }
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

  CallbackFactory callback_factory;
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

  CallbackFactory callback_factory;
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

  CallbackFactory callback_factory;
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

  CallbackFactory callback_factory;
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

  CallbackFactory callback_factory;
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

  CallbackFactory first_callback_factory;
  CallbackFactory second_callback_factory;
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

  CallbackFactory first_callback_factory;
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

  CallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse, std::string>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  disk_manager_->GetDiskInfo(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

}  // namespace
}  // namespace borealis
