// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NearbyConfirmationManagerTest : public testing::Test {
 public:
  NearbyConfirmationManagerTest() = default;
  ~NearbyConfirmationManagerTest() override = default;

  MockNearbySharingService& sharing_service() { return sharing_service_; }

  const ShareTarget& share_target() const { return share_target_; }

  NearbyConfirmationManager& manager() { return manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockNearbySharingService sharing_service_;
  ShareTarget share_target_;
  NearbyConfirmationManager manager_{&sharing_service_, share_target_};
};

}  // namespace

TEST_F(NearbyConfirmationManagerTest, Accept_Success) {
  EXPECT_CALL(sharing_service(), Accept(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kOk);
          }));
  base::MockCallback<NearbyConfirmationManager::AcceptCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true)));

  manager().Accept(callback.Get());
}

TEST_F(NearbyConfirmationManagerTest, Accept_Error) {
  EXPECT_CALL(sharing_service(), Accept(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kError);
          }));
  base::MockCallback<NearbyConfirmationManager::AcceptCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false)));

  manager().Accept(callback.Get());
}

TEST_F(NearbyConfirmationManagerTest, Reject_Success) {
  EXPECT_CALL(sharing_service(), Reject(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kOk);
          }));
  base::MockCallback<NearbyConfirmationManager::RejectCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true)));

  manager().Reject(callback.Get());
}

TEST_F(NearbyConfirmationManagerTest, Reject_Error) {
  EXPECT_CALL(sharing_service(), Reject(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kError);
          }));
  base::MockCallback<NearbyConfirmationManager::RejectCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false)));

  manager().Reject(callback.Get());
}

TEST_F(NearbyConfirmationManagerTest, Cancel_Success) {
  EXPECT_CALL(sharing_service(), Cancel(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kOk);
          }));
  base::MockCallback<NearbyConfirmationManager::CancelCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true)));

  manager().Cancel(callback.Get());
}

TEST_F(NearbyConfirmationManagerTest, Cancel_Error) {
  EXPECT_CALL(sharing_service(), Cancel(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const ShareTarget& target,
              NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target().id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kError);
          }));
  base::MockCallback<NearbyConfirmationManager::CancelCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false)));

  manager().Cancel(callback.Get());
}
