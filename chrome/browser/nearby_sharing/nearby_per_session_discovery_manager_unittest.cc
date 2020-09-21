// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

MATCHER_P(MatchesTarget, target, "") {
  return arg.id == target.id;
}

const char kTextAttachmentBody[] = "Test text payload";

std::vector<std::unique_ptr<Attachment>> CreateAttachments() {
  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.push_back(std::make_unique<TextAttachment>(
      TextAttachment::Type::kText, kTextAttachmentBody));
  return attachments;
}

void ExpectTextAttachment(
    const std::string& text_body,
    const std::vector<std::unique_ptr<Attachment>>& attachments) {
  ASSERT_EQ(1u, attachments.size());
  ASSERT_TRUE(attachments[0]);
  ASSERT_EQ(Attachment::Family::kText, attachments[0]->family());
  auto* text_attachment = static_cast<TextAttachment*>(attachments[0].get());
  EXPECT_EQ(text_body, text_attachment->text_body());
}

class MockShareTargetListener
    : public nearby_share::mojom::ShareTargetListener {
 public:
  MockShareTargetListener() = default;
  ~MockShareTargetListener() override = default;

  mojo::PendingRemote<ShareTargetListener> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void reset() { receiver_.reset(); }

  // nearby_share::mojom::ShareTargetListener:
  MOCK_METHOD(void, OnShareTargetDiscovered, (const ShareTarget&), (override));
  MOCK_METHOD(void, OnShareTargetLost, (const ShareTarget&), (override));

 private:
  mojo::Receiver<ShareTargetListener> receiver_{this};
};

class MockTransferUpdateListener
    : public nearby_share::mojom::TransferUpdateListener {
 public:
  MockTransferUpdateListener() = default;
  ~MockTransferUpdateListener() override = default;

  void Bind(mojo::PendingReceiver<TransferUpdateListener> receiver) {
    return receiver_.Bind(std::move(receiver));
  }

  // nearby_share::mojom::TransferUpdateListener:
  MOCK_METHOD(void,
              OnTransferUpdate,
              (nearby_share::mojom::TransferStatus status,
               const base::Optional<std::string>&),
              (override));

 private:
  mojo::Receiver<TransferUpdateListener> receiver_{this};
};

class NearbyPerSessionDiscoveryManagerTest : public testing::Test {
 public:
  using MockSelectShareTargetCallback = base::MockCallback<
      NearbyPerSessionDiscoveryManager::SelectShareTargetCallback>;
  using MockStartDiscoveryCallback = base::MockCallback<
      NearbyPerSessionDiscoveryManager::StartDiscoveryCallback>;

  NearbyPerSessionDiscoveryManagerTest() = default;
  ~NearbyPerSessionDiscoveryManagerTest() override = default;

  MockNearbySharingService& sharing_service() { return sharing_service_; }

  NearbyPerSessionDiscoveryManager& manager() { return manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockNearbySharingService sharing_service_;
  NearbyPerSessionDiscoveryManager manager_{&sharing_service_,
                                            CreateAttachments()};
};

}  // namespace

TEST_F(NearbyPerSessionDiscoveryManagerTest, CreateDestroyWithoutRegistering) {
  EXPECT_CALL(sharing_service(), RegisterSendSurface(&manager(), &manager(), _))
      .Times(0);
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);
  {
    NearbyPerSessionDiscoveryManager manager(&sharing_service(),
                                             CreateAttachments());
    // Creating and destroying an instance should not register itself with the
    // NearbySharingService.
  }
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, StartDiscovery_Success) {
  MockStartDiscoveryCallback callback;
  EXPECT_CALL(callback, Run(/*success=*/true));

  EXPECT_CALL(
      sharing_service(),
      RegisterSendSurface(&manager(), &manager(),
                          NearbySharingService::SendSurfaceState::kForeground))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);  // Should not be called!

  MockShareTargetListener listener;
  manager().StartDiscovery(listener.Bind(), callback.Get());

  // Reset the listener here like the UI does when switching pages.
  listener.reset();
  // We have to run util idle to give the disconnect handler a chance to run.
  // We no longer have a disconnect handler, but we want to very that once the
  // mojo connection is torn down, that we don't unregister.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that UnregisterSendSurface was NOT called due to the disconnect.
  // This previously failed when disconnect handler was being registered.
  EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(&sharing_service()));

  // However, we do expect UnregisterSendSurface to be called from the
  // destructor.
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, StartDiscovery_Error) {
  MockStartDiscoveryCallback callback;
  EXPECT_CALL(callback, Run(/*success=*/false));

  EXPECT_CALL(
      sharing_service(),
      RegisterSendSurface(&manager(), &manager(),
                          NearbySharingService::SendSurfaceState::kForeground))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kError));
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);

  MockShareTargetListener listener;
  manager().StartDiscovery(listener.Bind(), callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnShareTargetDiscovered) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  ShareTarget share_target;

  base::RunLoop run_loop;
  EXPECT_CALL(listener, OnShareTargetDiscovered(MatchesTarget(share_target)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  manager().OnShareTargetDiscovered(share_target);

  run_loop.Run();

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnShareTargetLost) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  ShareTarget share_target;

  base::RunLoop run_loop;
  EXPECT_CALL(listener, OnShareTargetLost(MatchesTarget(share_target)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  manager().OnShareTargetLost(share_target);

  run_loop.Run();

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_Invalid) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  MockSelectShareTargetCallback callback;
  EXPECT_CALL(
      callback,
      Run(nearby_share::mojom::SelectShareTargetResult::kInvalidShareTarget,
          testing::IsFalse(), testing::IsFalse()));

  manager().SelectShareTarget({}, callback.Get());

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_SendSuccess) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            testing::IsTrue(), testing::IsTrue()));

  EXPECT_CALL(sharing_service(), SendAttachments(_, _))
      .WillOnce(testing::Invoke(
          [&share_target](
              const ShareTarget& target,
              std::vector<std::unique_ptr<Attachment>> attachments) {
            EXPECT_EQ(share_target.id, target.id);
            ExpectTextAttachment(kTextAttachmentBody, attachments);
            return NearbySharingService::StatusCodes::kOk;
          }));

  manager().SelectShareTarget(share_target.id, callback.Get());

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_SendError) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  // Expect an error if NearbySharingService::Send*() fails.
  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback,
              Run(nearby_share::mojom::SelectShareTargetResult::kError,
                  testing::IsFalse(), testing::IsFalse()));

  EXPECT_CALL(sharing_service(), SendAttachments(_, _))
      .WillOnce(testing::Invoke(
          [&share_target](
              const ShareTarget& target,
              std::vector<std::unique_ptr<Attachment>> attachments) {
            EXPECT_EQ(share_target.id, target.id);
            ExpectTextAttachment(kTextAttachmentBody, attachments);
            return NearbySharingService::StatusCodes::kError;
          }));

  manager().SelectShareTarget(share_target.id, callback.Get());

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnTransferUpdate_WaitRemote) {
  // Setup share target
  MockShareTargetListener listener;
  MockTransferUpdateListener transfer_listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_listener, OnTransferUpdate(_, _))
      .WillOnce(testing::Invoke(
          [&run_loop](nearby_share::mojom::TransferStatus status,
                      const base::Optional<std::string>& token) {
            EXPECT_EQ(
                nearby_share::mojom::TransferStatus::kAwaitingRemoteAcceptance,
                status);
            EXPECT_FALSE(token.has_value());
            run_loop.Quit();
          }));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  EXPECT_CALL(listener, OnShareTargetDiscovered(MatchesTarget(share_target)))
      .Times(1);
  manager().OnShareTargetDiscovered(share_target);

  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            testing::IsTrue(), testing::IsTrue()))
      .WillOnce(testing::Invoke(
          [&transfer_listener](
              nearby_share::mojom::SelectShareTargetResult result,
              mojo::PendingReceiver<nearby_share::mojom::TransferUpdateListener>
                  listener,
              mojo::PendingRemote<nearby_share::mojom::ConfirmationManager>
                  manager) { transfer_listener.Bind(std::move(listener)); }));

  EXPECT_CALL(sharing_service(), SendAttachments(_, _))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().SelectShareTarget(share_target.id, callback.Get());

  auto metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build();
  manager().OnTransferUpdate(share_target, metadata);

  run_loop.Run();
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnTransferUpdate_WaitLocal) {
  // Setup share target
  MockShareTargetListener listener;
  MockTransferUpdateListener transfer_listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  const std::string expected_token = "Test Token";

  base::RunLoop run_loop;
  EXPECT_CALL(transfer_listener, OnTransferUpdate(_, _))
      .WillOnce(testing::Invoke([&run_loop, &expected_token](
                                    nearby_share::mojom::TransferStatus status,
                                    const base::Optional<std::string>& token) {
        EXPECT_EQ(
            nearby_share::mojom::TransferStatus::kAwaitingLocalConfirmation,
            status);
        EXPECT_EQ(expected_token, token);
        run_loop.Quit();
      }));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  EXPECT_CALL(listener, OnShareTargetDiscovered(MatchesTarget(share_target)))
      .Times(1);
  manager().OnShareTargetDiscovered(share_target);

  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            testing::IsTrue(), testing::IsTrue()))
      .WillOnce(testing::Invoke(
          [&transfer_listener](
              nearby_share::mojom::SelectShareTargetResult result,
              mojo::PendingReceiver<nearby_share::mojom::TransferUpdateListener>
                  listener,
              mojo::PendingRemote<nearby_share::mojom::ConfirmationManager>
                  manager) { transfer_listener.Bind(std::move(listener)); }));

  EXPECT_CALL(sharing_service(), SendAttachments(_, _))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().SelectShareTarget(share_target.id, callback.Get());

  auto metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(expected_token)
          .build();
  manager().OnTransferUpdate(share_target, metadata);

  run_loop.Run();

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, TransferUpdateWithoutListener) {
  MockStartDiscoveryCallback callback;
  EXPECT_CALL(callback, Run(/*success=*/true));

  EXPECT_CALL(
      sharing_service(),
      RegisterSendSurface(&manager(), &manager(),
                          NearbySharingService::SendSurfaceState::kForeground))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);  // Should not be called!

  MockShareTargetListener listener;
  manager().StartDiscovery(listener.Bind(), callback.Get());

  // It is possible that during registration if a transfer is in progress
  // already we might get a transfer update before selecting the share target.
  // This used to cause a crash due to transfer_update_listener_ not being
  // bound.
  ShareTarget share_target;
  manager().OnTransferUpdate(
      share_target, TransferMetadataBuilder()
                        .set_status(TransferMetadata::Status::kComplete)
                        .build());

  // However, we do expect UnregisterSendSurface to be called from the
  // destructor.
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
}
