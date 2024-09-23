// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_receive_manager.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom-test-utils.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeReceiveObserver : public nearby_share::mojom::ReceiveObserver {
 public:
  void OnHighVisibilityChanged(bool in_high_visibility) override {
    in_high_visibility_ = in_high_visibility;
  }

  void OnTransferUpdate(
      const ShareTarget& share_target,
      nearby_share::mojom::TransferMetadataPtr metadata) override {
    last_share_target_ = share_target;
    last_metadata_ = *metadata;
  }

  void OnNearbyProcessStopped() override {
    on_nearby_process_stopped_called_ = true;
  }

  void OnStartAdvertisingFailure() override {
    on_start_advertising_failure_called_ = true;
  }

  std::optional<nearby_share::mojom::TransferMetadata> last_metadata_;
  std::optional<bool> in_high_visibility_;
  ShareTarget last_share_target_;
  bool on_nearby_process_stopped_called_ = false;
  bool on_start_advertising_failure_called_ = false;
  mojo::Receiver<nearby_share::mojom::ReceiveObserver> receiver_{this};
};

class NearbyReceiveManagerTest : public testing::Test {
 public:
  using ReceiveSurfaceState = NearbySharingService::ReceiveSurfaceState;
  using StatusCodes = NearbySharingService::StatusCodes;
  using RegisterReceiveSurfaceResult =
      nearby_share::mojom::RegisterReceiveSurfaceResult;

  NearbyReceiveManagerTest()
      : transfer_metadata_(TransferMetadata::Status::kAwaitingLocalConfirmation,
                           /*progress=*/0.f,
                           /*token=*/"1234",
                           /*is_original=*/true,
                           /*is_final_status=*/false) {
    share_target_.id = base::UnguessableToken::Create();
    receive_manager_.AddReceiveObserver(
        observer_.receiver_.BindNewPipeAndPassRemote());
  }

  ~NearbyReceiveManagerTest() override = default;

 protected:
  void FlushMojoMessages() { observer_.receiver_.FlushForTesting(); }

  void ExpectRegister(StatusCodes code = StatusCodes::kOk) {
    EXPECT_CALL(sharing_service_,
                RegisterReceiveSurface(testing::_, testing::_))
        .WillOnce([=](TransferUpdateCallback* transfer_callback,
                      ReceiveSurfaceState state) {
          EXPECT_EQ(ReceiveSurfaceState::kForeground, state);
          return code;
        });
  }

  void ExpectUnregister(StatusCodes code = StatusCodes::kOk) {
    EXPECT_CALL(sharing_service_, UnregisterReceiveSurface(testing::_))
        .WillOnce(
            [=](TransferUpdateCallback* transfer_callback) { return code; });
  }

  void ExpectIsInHighVisibility(bool in_high_visibility) {
    EXPECT_CALL(sharing_service_, IsInHighVisibility())
        .WillOnce(testing::Return(in_high_visibility));
  }

  void ExpectAccept(StatusCodes code = StatusCodes::kOk) {
    EXPECT_CALL(sharing_service_, Accept(testing::_, testing::_))
        .WillOnce([=](const ShareTarget& share_target,
                      NearbySharingService::StatusCodesCallback
                          status_codes_callback) {
          std::move(status_codes_callback).Run(code);
        });
  }

  void ExpectReject(StatusCodes code = StatusCodes::kOk) {
    EXPECT_CALL(sharing_service_, Reject(testing::_, testing::_))
        .WillOnce([=](const ShareTarget& share_target,
                      NearbySharingService::StatusCodesCallback
                          status_codes_callback) {
          std::move(status_codes_callback).Run(code);
        });
  }

  content::BrowserTaskEnvironment task_environment_;
  ShareTarget share_target_;
  TransferMetadata transfer_metadata_;
  MockNearbySharingService sharing_service_;
  FakeReceiveObserver observer_;
  NearbyReceiveManager receive_manager_{&sharing_service_};
  nearby_share::mojom::ReceiveManagerAsyncWaiter receive_manager_waiter_{
      &receive_manager_};
};

}  // namespace

TEST_F(NearbyReceiveManagerTest, Enter_Exit_Success) {
  ExpectRegister();
  RegisterReceiveSurfaceResult result = RegisterReceiveSurfaceResult::kFailure;
  receive_manager_waiter_.RegisterForegroundReceiveSurface(&result);
  EXPECT_EQ(RegisterReceiveSurfaceResult::kSuccess, result);

  ExpectUnregister();
  bool exited = false;
  receive_manager_waiter_.UnregisterForegroundReceiveSurface(&exited);
  EXPECT_TRUE(exited);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, Enter_Failed) {
  RegisterReceiveSurfaceResult result = RegisterReceiveSurfaceResult::kSuccess;
  ExpectRegister(StatusCodes::kError);
  receive_manager_waiter_.RegisterForegroundReceiveSurface(&result);
  EXPECT_EQ(RegisterReceiveSurfaceResult::kFailure, result);
  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, Multiple_Enter_Successful) {
  RegisterReceiveSurfaceResult result = RegisterReceiveSurfaceResult::kFailure;
  ExpectRegister(StatusCodes::kOk);
  receive_manager_waiter_.RegisterForegroundReceiveSurface(&result);
  FlushMojoMessages();
  EXPECT_EQ(RegisterReceiveSurfaceResult::kSuccess, result);

  result = RegisterReceiveSurfaceResult::kFailure;
  ExpectRegister(StatusCodes::kOk);
  receive_manager_waiter_.RegisterForegroundReceiveSurface(&result);
  FlushMojoMessages();
  EXPECT_EQ(RegisterReceiveSurfaceResult::kSuccess, result);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, Exit_Failed) {
  RegisterReceiveSurfaceResult result = RegisterReceiveSurfaceResult::kFailure;
  ExpectRegister();
  receive_manager_waiter_.RegisterForegroundReceiveSurface(&result);
  EXPECT_EQ(RegisterReceiveSurfaceResult::kSuccess, result);

  ExpectUnregister(StatusCodes::kError);
  bool exited = false;
  receive_manager_waiter_.UnregisterForegroundReceiveSurface(&exited);
  EXPECT_FALSE(exited);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, ShareTargetNotifies_Accept) {
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_);
  FlushMojoMessages();
  EXPECT_EQ(share_target_.id, observer_.last_share_target_.id);
  ASSERT_TRUE(observer_.last_metadata_.has_value());
  EXPECT_EQ("1234", observer_.last_metadata_->token);

  ExpectAccept();
  bool success = false;
  receive_manager_waiter_.Accept(share_target_.id, &success);
  EXPECT_TRUE(success);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, ShareTargetNotifies_Reject) {
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_);
  FlushMojoMessages();
  EXPECT_EQ(share_target_.id, observer_.last_share_target_.id);
  ASSERT_TRUE(observer_.last_metadata_.has_value());
  EXPECT_EQ("1234", observer_.last_metadata_->token);

  ExpectReject();
  bool success = false;
  receive_manager_waiter_.Reject(share_target_.id, &success);
  EXPECT_TRUE(success);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest,
       ShareTargetNotifies_SenderCancelsBeforeAccept) {
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_);
  FlushMojoMessages();
  EXPECT_EQ(share_target_.id, observer_.last_share_target_.id);
  ASSERT_TRUE(observer_.last_metadata_.has_value());
  EXPECT_EQ("1234", observer_.last_metadata_->token);

  // Simulate the sender canceling before we accept the share target and causing
  // the accept to fail before hitting the service.
  TransferMetadata transfer_metadata_final(TransferMetadata::Status::kCancelled,
                                           1.f, std::nullopt, true, true);
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_final);
  FlushMojoMessages();

  bool success = true;
  receive_manager_waiter_.Accept(share_target_.id, &success);
  EXPECT_FALSE(success);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest,
       ShareTargetNotifies_SenderCancelsBeforeReject) {
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_);
  FlushMojoMessages();
  EXPECT_EQ(share_target_.id, observer_.last_share_target_.id);
  ASSERT_TRUE(observer_.last_metadata_.has_value());
  EXPECT_EQ("1234", observer_.last_metadata_->token);

  // Simulate the sender canceling before we reject the share target and causing
  // the reject to fail before hitting the service.
  TransferMetadata transfer_metadata_final(TransferMetadata::Status::kCancelled,
                                           1.f, std::nullopt, true, true);
  receive_manager_.OnTransferUpdate(share_target_, transfer_metadata_final);
  FlushMojoMessages();

  bool success = true;
  receive_manager_waiter_.Reject(share_target_.id, &success);
  EXPECT_FALSE(success);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, IsInHighVisibility) {
  ExpectIsInHighVisibility(true);
  bool on = false;
  receive_manager_waiter_.IsInHighVisibility(&on);
  FlushMojoMessages();
  EXPECT_TRUE(on);

  ExpectIsInHighVisibility(false);
  on = true;
  receive_manager_waiter_.IsInHighVisibility(&on);
  FlushMojoMessages();
  EXPECT_FALSE(on);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, OnHighVisibilityChangedObserver) {
  receive_manager_.OnHighVisibilityChanged(false);
  FlushMojoMessages();
  ASSERT_TRUE(observer_.in_high_visibility_.has_value());
  EXPECT_FALSE(*observer_.in_high_visibility_);

  observer_.in_high_visibility_ = std::nullopt;

  receive_manager_.OnHighVisibilityChanged(true);
  FlushMojoMessages();
  ASSERT_TRUE(observer_.in_high_visibility_.has_value());
  EXPECT_TRUE(*observer_.in_high_visibility_);

  ExpectUnregister();
}

TEST_F(NearbyReceiveManagerTest, OnStartAdvertisingFailureObserver) {
  EXPECT_FALSE(observer_.on_start_advertising_failure_called_);
  receive_manager_.OnStartAdvertisingFailure();
  FlushMojoMessages();
  EXPECT_TRUE(observer_.on_start_advertising_failure_called_);

  ExpectUnregister();
}
