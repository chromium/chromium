// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_token_status.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_oauth2_token_response.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace {

bool HasUserPrefValue(const PrefService* pref_service,
                      const std::string& pref) {
  return pref_service->GetUserPrefValue(pref) != nullptr;
}

// Waits until local changes are committed or an auth error is encountered.
class TestForAuthError : public UpdatedProgressMarkerChecker {
 public:
  static bool HasAuthError(syncer::SyncService* service) {
    // Note that depending on the nature of the auth error, sync may become
    // paused (for persistent auth errors) or in some other cases transient
    // errors may be surfaced via GetSyncTokenStatusForDebugging() (e.g. 401
    // HTTP status codes returned by the Sync server when the access token has
    // expired).
    return service->GetTransportState() ==
               syncer::SyncService::TransportState::PAUSED ||
           service->GetSyncTokenStatusForDebugging()
                   .last_get_token_error.state() !=
               GoogleServiceAuthError::NONE;
  }

  explicit TestForAuthError(syncer::SyncServiceImpl* service)
      : UpdatedProgressMarkerChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for auth error";
    return HasAuthError(service()) ||
           UpdatedProgressMarkerChecker::IsExitConditionSatisfied(os);
  }
};

class SyncAuthTestBase : public SyncTest {
 public:
  explicit SyncAuthTestBase(SetupSyncMode setup_sync_mode)
      : SyncTest(SINGLE_CLIENT) {
    if (setup_sync_mode == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  SyncAuthTestBase(const SyncAuthTestBase&) = delete;
  SyncAuthTestBase& operator=(const SyncAuthTestBase&) = delete;

  ~SyncAuthTestBase() override = default;

  // Helper function that adds a reading list entry and waits for either an auth
  // error, or for the entry to be committed. Returns true if it detects an
  // auth error, false if the entry is committed successfully.
  bool AttemptToTriggerAuthError() {
    int index = GetNextEntryIndex();
    std::string title = base::StringPrintf("Entry %d", index);
    GURL url = GURL(base::StringPrintf("http://www.foo%d.com", index));

    ReadingListModel* model =
        ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
    model->AddOrReplaceEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

    // Run until the entry is committed or an auth error is encountered.
    TestForAuthError(GetSyncService(0)).Wait();
    return TestForAuthError::HasAuthError(GetSyncService(0));
  }

  void DisableTokenFetchRetries() {
    // If SyncServiceImpl observes a transient error like SERVICE_UNAVAILABLE
    // or CONNECTION_FAILED, this means the access token fetcher has given
    // up trying to reach Gaia. In practice, the access token fetching code
    // retries a fixed number of times, but the count is transparent to PSS.
    // Disable retries so that we instantly trigger the case where
    // SyncServiceImpl must pick up where the access token fetcher left off
    // (in terms of retries).
    signin::DisableAccessTokenFetchRetries(
        IdentityManagerFactory::GetForProfile(GetProfile(0)));
  }

 private:
  int GetNextEntryIndex() { return entry_index_++; }

  base::test::ScopedFeatureList scoped_feature_list_;

  int entry_index_ = 0;
};

class SyncAuthTest
    : public SyncAuthTestBase,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SyncAuthTest() : SyncAuthTestBase(GetSetupSyncMode()) {}

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         SyncAuthTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// Verify that sync works with a valid OAuth2 token.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  GetFakeServer()->ClearHttpError();
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(
      gaia::FakeOAuth2TokenResponse::Success("new_access_token"));
  ASSERT_FALSE(AttemptToTriggerAuthError());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered more than a fixed number of
// HTTP_INTERNAL_SERVER_ERROR (500) errors.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, RetryOnInternalServerError500) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInternalFailure));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

class SyncAuthTokenFetcherDependentTest
    : public SyncAuthTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, SyncTest::SetupSyncMode>> {
 public:
  SyncAuthTokenFetcherDependentTest() : SyncAuthTestBase(GetSetupSyncMode()) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    scoped_feature_list_.InitWithFeatureState(
        switches::kUseIssueTokenToFetchAccessTokens, IsIssueTokenEnabled());
#else
    CHECK(!IsIssueTokenEnabled());
#endif
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return std::get<1>(GetParam());
  }

  bool IsIssueTokenEnabled() const { return std::get<0>(GetParam()); }

 private:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// Verifies the behavior when the access token fetcher encounters an
// HTTP_FORBIDDEN (403) error. With IssueToken this should result in a PAUSED
// state with SERVICE_ERROR auth error, whereas with GetToken it should keep
// retrying.
IN_PROC_BROWSER_TEST_P(SyncAuthTokenFetcherDependentTest, HttpForbidden403) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kErrorUnexpectedFormat, net::HTTP_FORBIDDEN));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            IsIssueTokenEnabled()
                ? syncer::SyncService::TransportState::PAUSED
                : syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(GetSyncService(0)->GetAuthError().state(),
            IsIssueTokenEnabled() ? GoogleServiceAuthError::SERVICE_ERROR
                                  : GoogleServiceAuthError::NONE);
  EXPECT_EQ(GetSyncService(0)->IsRetryingAccessTokenFetchForTest(),
            !IsIssueTokenEnabled());
}

// Verifies the behavior when the access token fetcher receives a malformed
// token. With IssueToken this should result in a PAUSED state with
// UNEXPECTED_SERVICE_RESPONSE auth error, whereas with GetToken it should keep
// retrying.
IN_PROC_BROWSER_TEST_P(SyncAuthTokenFetcherDependentTest, MalformedToken) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kOkUnexpectedFormat));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            IsIssueTokenEnabled()
                ? syncer::SyncService::TransportState::PAUSED
                : syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(GetSyncService(0)->GetAuthError().state(),
            IsIssueTokenEnabled()
                ? GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE
                : GoogleServiceAuthError::NONE);
  EXPECT_EQ(GetSyncService(0)->IsRetryingAccessTokenFetchForTest(),
            !IsIssueTokenEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SyncAuthTokenFetcherDependentTest,
    testing::Combine(testing::Values(false
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                                     ,
                                     true
#endif
                                     ),
                     GetSyncTestModes()),
    [](const testing::TestParamInfo<std::tuple<bool, SyncTest::SetupSyncMode>>&
           info) {
      return (std::get<0>(info.param) ? "WithIssueToken_" : "WithGetToken_") +
             (SetupSyncModeAsString(std::get<1>(info.param)));
    });

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered a URLRequestStatus of FAILED.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, RetryOnRequestFailed) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(
      gaia::FakeOAuth2TokenResponse::NetError(net::ERR_FAILED));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered more than a fixed number of
// rate limit exceeded errors.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, RetryOnRateLimitExceeded) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kRateLimitExceeded));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl ends up with an INVALID_GAIA_CREDENTIALS auth
// error when an invalid_grant error is returned by the access token fetcher
// with an HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, InvalidGrant) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInvalidGrant));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetSyncService(0)->GetAuthError().state());
  EXPECT_FALSE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl does not retry after SERVICE_ERROR auth error
// when an invalid_client error is returned by the access token fetcher with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, InvalidClient) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInvalidClient));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            GetSyncService(0)->GetAuthError().state());
  EXPECT_FALSE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl retries after REQUEST_CANCELED auth error
// when the access token fetcher has encountered a URLRequestStatus of
// CANCELED.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, RetryRequestCanceled) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(
      gaia::FakeOAuth2TokenResponse::NetError(net::ERR_ABORTED));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl fails initial sync setup during backend
// initialization and ends up with an INVALID_GAIA_CREDENTIALS auth error when
// an invalid_grant error is returned by the access token fetcher with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, FailInitialSetupWithPersistentError) {
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInvalidGrant));
  ASSERT_FALSE(GetClient(0)->SetupSync());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetSyncService(0)->GetAuthError().state());
}

// Verify that SyncServiceImpl fails initial sync setup during backend
// initialization, but continues trying to fetch access tokens when
// the access token fetcher receives an HTTP_INTERNAL_SERVER_ERROR (500)
// response code.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, RetryInitialSetupWithTransientError) {
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInternalFailure));
  ASSERT_FALSE(GetClient(0)->SetupSync());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl fetches a new token when an old token expires.
IN_PROC_BROWSER_TEST_P(SyncAuthTest, TokenExpiry) {
  // Initial sync succeeds with a short lived OAuth2 Token.
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->ClearHttpError();
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::Success(
      "short_lived_access_token", base::Seconds(5)));
  ASSERT_TRUE(GetClient(0)->SetupSync());
  std::string old_token = GetSyncService(0)->GetAccessTokenForTest();

  // Wait until the token has expired.
  base::PlatformThread::Sleep(base::Seconds(5));

  // Trigger an auth error on the server so PSS requests OA2TS for a new token
  // during the next sync cycle.
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  SetOAuth2TokenResponse(gaia::FakeOAuth2TokenResponse::OAuth2Error(
      OAuth2Response::kInternalFailure));
  ASSERT_TRUE(AttemptToTriggerAuthError());
  ASSERT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());

  // Trigger an auth success state and set up a new valid OAuth2 token.
  GetFakeServer()->ClearHttpError();
  SetOAuth2TokenResponse(
      gaia::FakeOAuth2TokenResponse::Success("new_access_token"));

  // Verify that the next sync cycle is successful, and uses the new auth token.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  std::string new_token = GetSyncService(0)->GetAccessTokenForTest();
  EXPECT_NE(old_token, new_token);
}

class NoAuthErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit NoAuthErrorChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for auth error to be cleared";
    return service()->GetAuthError().state() == GoogleServiceAuthError::NONE;
  }
};

IN_PROC_BROWSER_TEST_P(SyncAuthTest, SyncPausedState) {
  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  const syncer::DataTypeSet active_types =
      GetSyncService(0)->GetActiveDataTypes();
  ASSERT_FALSE(active_types.empty());

  // Enter the "Sync paused"/"sign-in pending" state.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Sync should have shut itself down.
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_FALSE(GetSyncService(0)->IsEngineInitialized());

  // The active data types should now be empty.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().empty());

  // Clear the "Sync paused"/"sign-in pending" state again.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
  // SyncService will clear its auth error state only once it gets a valid
  // access token again, so wait for that to happen.
  NoAuthErrorChecker(GetSyncService(0)).Wait();
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Once the auth error is gone, wait for Sync to start up again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Now the active data types should be back.
  EXPECT_EQ(GetSyncService(0)->GetActiveDataTypes(), active_types);
}

IN_PROC_BROWSER_TEST_P(SyncAuthTest, ShouldTrackDeletionsInSyncPausedState) {
  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  // USS type.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
  // Pseudo-USS type.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  const std::u16string kTestTitle = u"Title";
  const GURL kTestURL("http://mail.google.com");

  PrefService* pref_service = GetProfile(0)->GetPrefs();

  // Create a bookmark...
  bookmarks::BookmarkModel* bookmark_model =
      bookmarks_helper::GetBookmarkModel(0);
  const bookmarks::BookmarkNode* bar =
      (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature)
          ? bookmark_model->bookmark_bar_node()
          : bookmark_model->account_bookmark_bar_node();
  ASSERT_FALSE(bookmarks_helper::HasNodeWithURL(0, kTestURL));
  const bookmarks::BookmarkNode* bookmark = bookmarks_helper::AddURL(
      0, bar, bar->children().size(), kTestTitle, kTestURL);

  // ...set a pref...
  ASSERT_FALSE(HasUserPrefValue(pref_service, prefs::kHomePageIsNewTabPage));
  pref_service->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  // ...and wait for both to be synced up.
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kTestTitle, kTestURL}}, /*cryptographer=*/nullptr)
                  .Wait());
  ASSERT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::PREFERENCES, prefs::kHomePageIsNewTabPage, "true")
                  .Wait());

  // Enter the "Sync paused"/"sign-in pending" state.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Sync should have shut itself down.
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_FALSE(GetSyncService(0)->IsEngineInitialized());

  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Delete the bookmark and the pref.
  // Note that AttemptToTriggerAuthError() also creates bookmarks, so the index
  // of our test bookmark might have changed.
  ASSERT_EQ(bar->children().back().get(), bookmark);
  bookmarks_helper::Remove(0, bar, bar->children().size() - 1);
  ASSERT_FALSE(bookmarks_helper::HasNodeWithURL(0, kTestURL));
  pref_service->ClearPref(prefs::kHomePageIsNewTabPage);

  // Clear the "Sync paused"/"sign-in pending" state again.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
  // SyncService will clear its auth error state only once it gets a valid
  // access token again, so wait for that to happen.
  NoAuthErrorChecker(GetSyncService(0)).Wait();
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());
  // Once the auth error is gone, wait for Sync to start up again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Resuming sync should *not* have re-created the deleted items.
  EXPECT_FALSE(bookmarks_helper::HasNodeWithURL(0, kTestURL));
  EXPECT_FALSE(HasUserPrefValue(pref_service, prefs::kHomePageIsNewTabPage));
}

}  // namespace
