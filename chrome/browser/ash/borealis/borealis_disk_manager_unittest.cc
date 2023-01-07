// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"

#include <memory>
#include <queue>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

using ::testing::_;
using ::testing::Not;

constexpr uint64_t kGiB = 1024 * 1024 * 1024;

using BorealisGetDiskSpaceInfoCallback =
    base::OnceCallback<void(absl::optional<int64_t>)>;

class FreeSpaceProviderMock
    : public BorealisDiskManagerImpl::FreeSpaceProvider {
 public:
  MOCK_METHOD(void, Get, (BorealisGetDiskSpaceInfoCallback), ());
};

using DiskInfoCallbackFactory = StrictCallbackFactory<void(
    Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
             Described<BorealisGetDiskInfoResult>>)>;

using RequestDeltaCallbackFactory = StrictCallbackFactory<void(
    Expected<uint64_t, Described<BorealisResizeDiskResult>>)>;

using SyncDiskCallbackFactory =
    NiceCallbackFactory<void(Expected<BorealisSyncDiskSizeResult,
                                      Described<BorealisSyncDiskSizeResult>>)>;

class BorealisDiskDispatcherMock : public BorealisDiskManagerDispatcher {
 public:
  MOCK_METHOD(void,
              GetDiskInfo,
              (const std::string&,
               const std::string&,
               base::OnceCallback<
                   void(Expected<BorealisDiskManager::GetDiskInfoResponse,
                                 Described<BorealisGetDiskInfoResult>>)>),
              ());
  MOCK_METHOD(void,
              RequestSpace,
              (const std::string&,
               const std::string&,
               uint64_t,
               base::OnceCallback<void(
                   Expected<uint64_t, Described<BorealisResizeDiskResult>>)>),
              ());
  MOCK_METHOD(void,
              ReleaseSpace,
              (const std::string&,
               const std::string&,
               uint64_t,
               base::OnceCallback<void(
                   Expected<uint64_t, Described<BorealisResizeDiskResult>>)>),
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

class BorealisDiskManagerTest : public testing::Test,
                                protected guest_os::FakeVmServicesHelper {
 public:
  BorealisDiskManagerTest() = default;
  ~BorealisDiskManagerTest() override = default;
  BorealisDiskManagerTest(const BorealisDiskManagerTest&) = delete;
  BorealisDiskManagerTest& operator=(const BorealisDiskManagerTest&) = delete;

 protected:
  void SetUp() override {
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
    features_.InitAndEnableFeature(ash::features::kBorealisDiskManagement);
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
  }

  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }

  vm_tools::concierge::ListVmDisksResponse BuildValidListVmDisksResponse(
      uint64_t min_size,
      uint64_t size,
      uint64_t available_space) {
    vm_tools::concierge::ListVmDisksResponse response;
    auto* image = response.add_images();
    response.set_success(true);
    image->set_name("vm_name1");
    image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
    image->set_user_chosen_size(true);
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
  base::test::ScopedFeatureList features_;
  std::unique_ptr<base::RunLoop> run_loop_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnFreeSpaceProviderError) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(testing::Invoke([](BorealisGetDiskSpaceInfoCallback callback) {
        std::move(callback).Run(-1);
      }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kFailedGettingExpandableSpace);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnNoResponseFromConcierge) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            // Concierge will return an empty ListVmDisksResponse.
            FakeConciergeClient()->set_list_vm_disks_response(
                absl::optional<vm_tools::concierge::ListVmDisksResponse>());
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kConciergeFailed);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       GetDiskInfoFailsOnUnsuccessfulResponseFromConcierge) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                /*available_space=*/1 * kGiB);
            response.set_success(false);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kConciergeFailed);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnVmMismatch) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                /*available_space=*/1 * kGiB);
            response.mutable_images()->at(0).set_name("UNMATCHED_VM");
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kConciergeFailed);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoSucceedsAndReturnsResponse) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
            // 3GB of disk space less 2GB of buffer is 1GB of available space.
            EXPECT_EQ(response_or_error.Value().available_bytes, 1 * kGiB);
            // 2GB of expandable space less 1GB of headroom is 1GB of expandable
            // space.
            EXPECT_EQ(response_or_error.Value().expandable_bytes, 1 * kGiB);
            EXPECT_EQ(response_or_error.Value().disk_size, 20 * kGiB);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoReservesExpandableSpaceForBuffer) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(3 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
            // Buffer is undersized so 0 space is available
            EXPECT_EQ(response_or_error.Value().available_bytes, 0u);
            // 3GB of expandable space less 1GB of headroom and less 1GB (needed
            // to regenerate the buffer) leaves 1GB free.
            EXPECT_EQ(response_or_error.Value().expandable_bytes, 1 * kGiB);
            EXPECT_EQ(response_or_error.Value().disk_size, 20 * kGiB);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       GetDiskInfoReturns0AvailableSpaceForSparseDisks) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            response.mutable_images()->at(0).set_user_chosen_size(false);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
            // 0GB of disk space, we always return 0 for sparse disks.
            EXPECT_EQ(response_or_error.Value().available_bytes, 0 * kGiB);
            // 2GB of expandable space less 1GB of headroom is 1GB of expandable
            // space.
            EXPECT_EQ(response_or_error.Value().expandable_bytes, 1 * kGiB);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsOnConcurrentAttempt) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  DiskInfoCallbackFactory first_callback_factory;
  DiskInfoCallbackFactory second_callback_factory;
  EXPECT_CALL(first_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
          }));
  EXPECT_CALL(second_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kAlreadyInProgress);
          }));
  disk_manager_->GetDiskInfo(first_callback_factory.BindOnce());
  disk_manager_->GetDiskInfo(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoSubsequentAttemptSucceeds) {
  auto response =
      BuildValidListVmDisksResponse(/*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                                    /*available_space=*/3 * kGiB);

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(testing::Invoke([this, response = response](
                                    BorealisGetDiskSpaceInfoCallback callback) {
        FakeConciergeClient()->set_list_vm_disks_response(response);
        std::move(callback).Run(2 * kGiB);
      }));

  DiskInfoCallbackFactory first_callback_factory;
  EXPECT_CALL(first_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
          }));
  disk_manager_->GetDiskInfo(first_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(testing::Invoke([this, response = response](
                                    BorealisGetDiskSpaceInfoCallback callback) {
        FakeConciergeClient()->set_list_vm_disks_response(response);
        std::move(callback).Run(2 * kGiB);
      }));

  DiskInfoCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_TRUE(response_or_error);
          }));
  disk_manager_->GetDiskInfo(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceFailsIf0SpaceRequested) {
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));
  disk_manager_->RequestSpace(0, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsIf0SpaceReleased) {
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));
  disk_manager_->ReleaseSpace(0, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsIfRequestExceedsInt64) {
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kOverflowError);
          }));
  disk_manager_->ReleaseSpace(uint64_t(std::numeric_limits<int64_t>::max()) + 1,
                              callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfBuildDiskInfoFails) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            response.mutable_images()->at(0).set_name("UNMATCHED_VM");
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kFailedToGetDiskInfo);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfDiskTypeNotRaw) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            response.mutable_images()->at(0).set_image_type(
                vm_tools::concierge::DiskImageType::DISK_IMAGE_AUTO);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kInvalidDiskType);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfRequestTooHigh) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kNotEnoughExpandableSpace);
          }));
  // 6GB > 4GB of expandable space.
  disk_manager_->RequestSpace(6 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       RequestDeltaFailsIfRequestWouldNotLeaveEnoughSpace) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kWouldNotLeaveEnoughSpace);
          }));
  // Release space is requesting a negative delta. 2GB > 1GB of unused available
  // space.
  disk_manager_->ReleaseSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsIfDiskIsBelowMinimum) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/8 * kGiB, /*size=*/7 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kViolatesMinimumSize);
          }));
  // Release space is requesting a negative delta. 7GB-2GB < 8GB min_size, and
  // min size is already smaller than the disk, so the disk cannot be shrunk
  // at all.
  disk_manager_->ReleaseSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaShrinksAsSmallAsPossible) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/7 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/6 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(6 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));

  // Release space is requesting a negative delta. 7GB-2GB < 6GB min_size, but
  // as the disk size is greater than the min size, the disk can still be
  // partially shrunk (whilst maintaining enough available space and not going
  // below the minimum disk size).
  disk_manager_->ReleaseSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnNoResizeDiskResponse) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kConciergeFailed);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnFailedResizeDiskResponse) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kConciergeFailed);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnDelayedConciergeFailure) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
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
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);
  FakeConciergeClient()->set_disk_image_status_signals(signals);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kConciergeFailed);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaFailsOnFailureToGetUpdate) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(testing::Invoke([](BorealisGetDiskSpaceInfoCallback callback) {
        std::move(callback).Run(-1 * kGiB);
      }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kFailedGettingUpdate);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceFailsIfResizeTooSmall) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(testing::Invoke(
          [this](base::OnceCallback<void(absl::optional<int64_t>)> callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                /*available_space=*/5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kFailedToFulfillRequest);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsIfDiskExpanded) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                /*available_space=*/5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kFailedToFulfillRequest);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceSuccessful) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(3 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
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

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/19 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(6 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceConvertsSparseDiskToFixed) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/100 * kGiB);
            response.mutable_images()->at(0).set_user_chosen_size(false);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(100 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            // Note: available_space goes from 100 GB to 3 GB, when requesting
            // for more space. In a sparse disk, the available space on the
            // disk is more like the expandable space on the disk, so it makes
            // sense that when we convert to a fixed disk, it will decrease so
            // that it reflects the actual space available on the disk.
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/23 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(97 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceConvertsSparseDiskToFixed) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/100 * kGiB);
            response.mutable_images()->at(0).set_user_chosen_size(false);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(100 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            // Note: available_space goes from 100 GB to 2 GB, when releasing
            // space. In a sparse disk, the available space on the disk is more
            // like the expandable space on the disk, so it makes sense that
            // when we convert to a fixed disk, it will decrease so that it
            // reflects the actual space available on the disk.
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                /*available_space=*/2 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(98 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 0u);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceSuccessfullyRegeneratesBuffer) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/23 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            // Though the disk size and total available space has increased
            // by 3GB, the space available for the client has only increased
            // by 2GB.
            EXPECT_EQ(response_or_error.Value(), 2 * kGiB);
          }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaConcurrentAttemptFails) {
  // We don't use a sequence here because the ordering of expectations is not
  // guaranteed, because of that we need to declare the free space provider
  // expectations in reverse order and retire the first expecation when
  // fulfilled.
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                /*available_space=*/5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(3 * kGiB);
          }));

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }))
      .RetiresOnSaturation();

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  RequestDeltaCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) { EXPECT_FALSE(response_or_error); }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  disk_manager_->RequestSpace(2 * kGiB, second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestDeltaSubsequentAttemptSucceeds) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                /*available_space=*/5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(3 * kGiB);
          }));

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) { EXPECT_TRUE(response_or_error); }));
  disk_manager_->RequestSpace(2 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/22 * kGiB,
                /*available_space=*/5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(3 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse second_disk_response;
  second_disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(second_disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/21 * kGiB,
                /*available_space=*/4 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(4 * kGiB);
          }));

  RequestDeltaCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_TRUE(response_or_error);
            EXPECT_EQ(response_or_error.Value(), 1 * kGiB);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeFailsIfGetDiskInfoFails) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            response.mutable_images()->at(0).set_name("UNMATCHED_VM");
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_FALSE(result);
            EXPECT_EQ(result.Error().error(),
                      BorealisSyncDiskSizeResult::kFailedToGetDiskInfo);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeSucceedsIfDiskNotFixedSize) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/3 * kGiB);
            response.mutable_images()->at(0).set_user_chosen_size(false);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kDiskNotFixed);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeSucceedsIfDiskCantExpand) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(0 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kNotEnoughSpaceToExpand);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeSucceedsIfDiskDoesntNeedToExpand) {
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/20 * kGiB,
                /*available_space=*/2 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(10 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kNoActionNeeded);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeFailsIfResizeAttemptFails) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/6 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_FALSE(result);
            EXPECT_EQ(result.Error().error(),
                      BorealisSyncDiskSizeResult::kResizeFailed);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest,
       SyncDiskSizeSucceedsIfDiskBelowMinSizeDuringShrink) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/5 * kGiB,
                /*available_space=*/3 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(5 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kDiskSizeSmallerThanMin);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizePartialResizeSucceeds) {
  // This is considered "partial" as it should log a warning. There is not
  // enough space on the device to fully rebuild the buffer to 2GB, but it will
  // succeed in rebuilding it to 1.5GB.

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/7 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1.5 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/7.5 * kGiB,
                /*available_space=*/1.5 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kResizedPartially);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeCompleteResizeSucceeds) {
  // This is a complete success as the buffer is fully resized to 2GB.

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  EXPECT_CALL(*free_space_provider_, Get(_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/7 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }));

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                /*available_space=*/2 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1 * kGiB);
          }));

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kResizedSuccessfully);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeConcurrentAttemptFails) {
  // We don't use a sequence here because the ordering of expectations is not
  // guaranteed, because of that we need to declare the free space provider
  // expectations in reverse order and retire the first expecation when
  // fulfilled.
  EXPECT_CALL(*free_space_provider_, Get(_))
      .WillOnce(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/8 * kGiB,
                /*available_space=*/2 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(1 * kGiB);
          }));

  EXPECT_CALL(*free_space_provider_, Get(_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([this](BorealisGetDiskSpaceInfoCallback callback) {
            auto response = BuildValidListVmDisksResponse(
                /*min_size=*/6 * kGiB, /*size=*/7 * kGiB,
                /*available_space=*/1 * kGiB);
            FakeConciergeClient()->set_list_vm_disks_response(response);
            std::move(callback).Run(2 * kGiB);
          }))
      .RetiresOnSaturation();

  vm_tools::concierge::ResizeDiskImageResponse disk_response;
  disk_response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  FakeConciergeClient()->set_resize_disk_image_response(disk_response);

  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
          }));

  SyncDiskCallbackFactory second_callback_factory;
  EXPECT_CALL(second_callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_FALSE(result);
            EXPECT_EQ(result.Error().error(),
                      BorealisSyncDiskSizeResult::kAlreadyInProgress);
          }));

  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  disk_manager_->SyncDiskSize(second_callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, GetDiskInfoFailsWhenFeatureDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(ash::features::kBorealisDiskManagement);
  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                      Described<BorealisGetDiskInfoResult>> response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisGetDiskInfoResult::kInvalidRequest);
          }));
  disk_manager_->GetDiskInfo(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, RequestSpaceFailsWhenFeatureDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(ash::features::kBorealisDiskManagement);
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));
  disk_manager_->RequestSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, ReleaseSpaceFailsWhenFeatureDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(ash::features::kBorealisDiskManagement);
  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error);
            EXPECT_EQ(response_or_error.Error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));
  disk_manager_->ReleaseSpace(1 * kGiB, callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

TEST_F(BorealisDiskManagerTest, SyncDiskSizeSucceedsWhenFeatureDisabled) {
  features_.Reset();
  features_.InitAndDisableFeature(ash::features::kBorealisDiskManagement);
  SyncDiskCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(_))
      .WillOnce(testing::Invoke(
          [](Expected<BorealisSyncDiskSizeResult,
                      Described<BorealisSyncDiskSizeResult>> result) {
            EXPECT_TRUE(result);
            EXPECT_EQ(result.Value(),
                      BorealisSyncDiskSizeResult::kNoActionNeeded);
          }));
  disk_manager_->SyncDiskSize(callback_factory.BindOnce());
  run_loop()->RunUntilIdle();
}

}  // namespace
}  // namespace borealis
