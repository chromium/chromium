// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"

namespace {

using sync_pb::SharingMessageSpecifics;

constexpr char kEmptyOAuth2Token[] = "";

constexpr char kInvalidGrantOAuth2Token[] = R"(
    {
      "error": "invalid_grant"
    })";

constexpr char kValidOAuth2Token[] = R"(
    {
      "refresh_token": "new_refresh_token",
      "access_token": "new_access_token",
      "expires_in": 3600,  // 1 hour.
      "token_type": "Bearer"
    })";

MATCHER_P(HasErrorCode, expected_error_code, "") {
  return arg.error_code() == expected_error_code;
}

class NextCycleIterationChecker : public SingleClientStatusChangeChecker {
 public:
  explicit NextCycleIterationChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {
    last_synced_time_ = SingleClientStatusChangeChecker::service()
                            ->GetLastSyncedTimeForDebugging();
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side for next sync cycle.";

    if (last_synced_time_ == service()->GetLastSyncedTimeForDebugging()) {
      return false;
    }

    return true;
  }

 private:
  base::Time last_synced_time_;
};

class DisabledSharingMessageChecker : public SingleClientStatusChangeChecker {
 public:
  explicit DisabledSharingMessageChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for disabled SHARING_MESSAGE data type";
    return !service()->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE);
  }
};

class RetryingAccessTokenFetchChecker : public SingleClientStatusChangeChecker {
 public:
  explicit RetryingAccessTokenFetchChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for auth error";
    return service()->IsRetryingAccessTokenFetchForTest();
  }
};

// Waits until all expected sharing messages are successfully committed. Sharing
// messages are equal if they have the same payload.
class SharingMessageEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  SharingMessageEqualityChecker(
      syncer::SyncServiceImpl* service,
      fake_server::FakeServer* fake_server,
      std::vector<SharingMessageSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        expected_specifics_(std::move(expected_specifics)) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side SHARING_MESSAGE to match expected.";
    std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByDataType(syncer::SHARING_MESSAGE);

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_GE(expected_specifics_.size(), entities.size());

    if (expected_specifics_.size() != entities.size()) {
      return false;
    }

    for (const SharingMessageSpecifics& specifics : expected_specifics_) {
      auto iter = base::ranges::find(
          entities, specifics.payload(), [](const sync_pb::SyncEntity& entity) {
            return entity.specifics().sharing_message().payload();
          });
      if (iter == entities.end()) {
        *os << "Server doesn't have expected sharing message with payload: "
            << specifics.payload();
        return false;
      }
      // Remove found entity to check for duplicate payloads.
      entities.erase(iter);
    }

    DCHECK(entities.empty());
    return true;
  }

 private:
  const raw_ptr<fake_server::FakeServer> fake_server_ = nullptr;
  const std::vector<SharingMessageSpecifics> expected_specifics_;
};

// Waits for the sharing message callback with expected error code to be called.
// The provided callback must be called only once.
class SharingMessageCallbackChecker : public SingleClientStatusChangeChecker {
 public:
  SharingMessageCallbackChecker(
      syncer::SyncServiceImpl* service,
      sync_pb::SharingMessageCommitError::ErrorCode expected_error_code)
      : SingleClientStatusChangeChecker(service),
        expected_error_code_(expected_error_code) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for callback";

    return last_error_code_ &&
           last_error_code_->error_code() == expected_error_code_;
  }

  SharingMessageBridge::CommitFinishedCallback GetCommitFinishedCallback() {
    return base::BindOnce(&SharingMessageCallbackChecker::OnCommitFinished,
                          weak_ptr_factory_.GetWeakPtr());
  }

 private:
  void OnCommitFinished(const sync_pb::SharingMessageCommitError& error_code) {
    EXPECT_FALSE(last_error_code_.has_value());
    last_error_code_ = error_code;
  }

  const sync_pb::SharingMessageCommitError::ErrorCode expected_error_code_;
  std::optional<sync_pb::SharingMessageCommitError> last_error_code_;

  base::WeakPtrFactory<SharingMessageCallbackChecker> weak_ptr_factory_{this};
};

class SingleClientSharingMessageSyncTest : public SyncTest {
 public:
  SingleClientSharingMessageSyncTest() : SyncTest(SINGLE_CLIENT) {}

  bool WaitForSharingMessage(
      std::vector<SharingMessageSpecifics> expected_specifics) {
    return SharingMessageEqualityChecker(GetSyncService(0), GetFakeServer(),
                                         std::move(expected_specifics))
        .Wait();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest, ShouldSubmit) {
  ASSERT_TRUE(SetupSync());
  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::NONE);

  ASSERT_EQ(0u, GetFakeServer()
                    ->GetSyncEntitiesByDataType(syncer::SHARING_MESSAGE)
                    .size());

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  EXPECT_TRUE(WaitForSharingMessage({specifics}));
  EXPECT_TRUE(callback_checker.Wait());
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldSubmitInTransportMode) {
  // We avoid calling SetupSync(), because we don't want to turn on full sync,
  // only sign in such that the standalone transport starts.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive())
      << "Full sync should be disabled";
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE));

  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::NONE);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  EXPECT_TRUE(WaitForSharingMessage({specifics}));
  EXPECT_TRUE(callback_checker.Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldPropagateCommitFailure) {
  ASSERT_TRUE(SetupSync());
  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR);

  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::TRANSIENT_ERROR);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  EXPECT_TRUE(callback_checker.Wait());
}

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientSharingMessageSyncTest,
                       ShouldCleanPendingMessagesUponSignout) {
  ASSERT_TRUE(SetupSync());
  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(GetClient(0)->SetupSync());

  EXPECT_TRUE(callback_checker.Wait());
  EXPECT_TRUE(GetFakeServer()
                  ->GetSyncEntitiesByDataType(syncer::SHARING_MESSAGE)
                  .empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(
    SingleClientSharingMessageSyncTest,
    ShouldTurnOffSharingMessageDataTypeOnPersistentAuthError) {
  ASSERT_TRUE(SetupSync());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  SetOAuth2TokenResponse(kInvalidGrantOAuth2Token, net::HTTP_BAD_REQUEST,
                         net::OK);

  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload("payload");
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  EXPECT_TRUE(DisabledSharingMessageChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(callback_checker.Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSharingMessageSyncTest,
    ShouldRetrySendingSharingMessageDataTypeOnTransientAuthError) {
  const std::string payload = "payload";

  ASSERT_TRUE(SetupSync());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::OK);

  SharingMessageCallbackChecker callback_checker(
      GetSyncService(0), sync_pb::SharingMessageCommitError::NONE);

  SharingMessageBridge* sharing_message_bridge =
      SharingMessageBridgeFactory::GetForBrowserContext(GetProfile(0));
  SharingMessageSpecifics specifics;
  specifics.set_payload(payload);
  sharing_message_bridge->SendSharingMessage(
      std::make_unique<SharingMessageSpecifics>(specifics),
      callback_checker.GetCommitFinishedCallback());

  ASSERT_TRUE(RetryingAccessTokenFetchChecker(GetSyncService(0)).Wait());
  SetOAuth2TokenResponse(kValidOAuth2Token, net::HTTP_OK, net::OK);
  GetFakeServer()->ClearHttpError();

  EXPECT_TRUE(WaitForSharingMessage({specifics}));
  EXPECT_TRUE(callback_checker.Wait());
}

}  // namespace
