// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_token_status.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace {

constexpr char kShortLivedOAuth2Token[] = R"(
    {
      "refresh_token": "short_lived_refresh_token",
      "access_token": "short_lived_access_token",
      "expires_in": 5,  // 5 seconds.
      "token_type": "Bearer"
    })";

constexpr char kValidOAuth2Token[] = R"({
                                   "refresh_token": "new_refresh_token",
                                   "access_token": "new_access_token",
                                   "expires_in": 3600,  // 1 hour.
                                   "token_type": "Bearer"
                                 })";

constexpr char kInvalidGrantOAuth2Token[] = R"({
                                           "error": "invalid_grant"
                                        })";

constexpr char kInvalidClientOAuth2Token[] = R"({
                                           "error": "invalid_client"
                                         })";

constexpr char kEmptyOAuth2Token[] = "";

constexpr char kMalformedOAuth2Token[] = R"({ "foo": )";

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

class SyncTransportActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncTransportActiveChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync transport to become active";
    return service()->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE;
  }
};

class SyncAuthTest : public SyncTest {
 public:
  SyncAuthTest() : SyncTest(SINGLE_CLIENT) {}

  SyncAuthTest(const SyncAuthTest&) = delete;
  SyncAuthTest& operator=(const SyncAuthTest&) = delete;

  ~SyncAuthTest() override = default;

  // Helper function that adds a bookmark and waits for either an auth error, or
  // for the bookmark to be committed.  Returns true if it detects an auth
  // error, false if the bookmark is committed successfully.
  bool AttemptToTriggerAuthError() {
    int bookmark_index = GetNextBookmarkIndex();
    std::string title = base::StringPrintf("Bookmark %d", bookmark_index);
    GURL url = GURL(base::StringPrintf("http://www.foo%d.com", bookmark_index));
    EXPECT_NE(nullptr, bookmarks_helper::AddURL(0, title, url));

    // Run until the bookmark is committed or an auth error is encountered.
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
  int GetNextBookmarkIndex() { return bookmark_index_++; }

  int bookmark_index_ = 0;
};

class SyncAuthTestOAuthTokens : public SyncAuthTest {
 public:
  SyncAuthTestOAuthTokens() {
    // This test suite intercepts OAuth token requests, and IP Protection also
    // requests OAuth tokens. Those requests race with the requests from the
    // sync service, resulting in intermittent failures. Disabling IP Protection
    // prevents these races.
    feature_list_.InitAndDisableFeature(
        net::features::kEnableIpProtectionProxy);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that sync works with a valid OAuth2 token.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  GetFakeServer()->ClearHttpError();
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kValidOAuth2Token, net::HTTP_OK, net::OK);
  ASSERT_FALSE(AttemptToTriggerAuthError());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered more than a fixed number of
// HTTP_INTERNAL_SERVER_ERROR (500) errors.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnInternalServerError500) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kValidOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::OK);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered more than a fixed number of
// HTTP_FORBIDDEN (403) errors.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnHttpForbidden403) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_FORBIDDEN, net::OK);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher has encountered a URLRequestStatus of FAILED.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnRequestFailed) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::ERR_FAILED);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl continues trying to fetch access tokens
// when the access token fetcher receives a malformed token.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnMalformedToken) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kMalformedOAuth2Token, net::HTTP_OK, net::OK);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl ends up with an INVALID_GAIA_CREDENTIALS auth
// error when an invalid_grant error is returned by the access token fetcher
// with an HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, InvalidGrant) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kInvalidGrantOAuth2Token, net::HTTP_BAD_REQUEST,
                         net::OK);
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
IN_PROC_BROWSER_TEST_F(SyncAuthTest, InvalidClient) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kInvalidClientOAuth2Token, net::HTTP_BAD_REQUEST,
                         net::OK);
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
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryRequestCanceled) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(AttemptToTriggerAuthError());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::ERR_ABORTED);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl fails initial sync setup during backend
// initialization and ends up with an INVALID_GAIA_CREDENTIALS auth error when
// an invalid_grant error is returned by the access token fetcher with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, FailInitialSetupWithPersistentError) {
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kInvalidGrantOAuth2Token, net::HTTP_BAD_REQUEST,
                         net::OK);
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
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryInitialSetupWithTransientError) {
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::OK);
  ASSERT_FALSE(GetClient(0)->SetupSync());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  EXPECT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());
}

// Verify that SyncServiceImpl fetches a new token when an old token expires.
// TODO(crbug.com/40788468): Flaky on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TokenExpiry DISABLED_TokenExpiry
#else
#define MAYBE_TokenExpiry TokenExpiry
#endif
IN_PROC_BROWSER_TEST_F(SyncAuthTestOAuthTokens, MAYBE_TokenExpiry) {
  // Initial sync succeeds with a short lived OAuth2 Token.
  ASSERT_TRUE(SetupClients());
  GetFakeServer()->ClearHttpError();
  DisableTokenFetchRetries();
  SetOAuth2TokenResponse(kShortLivedOAuth2Token, net::HTTP_OK, net::OK);
  ASSERT_TRUE(GetClient(0)->SetupSync());
  std::string old_token = GetSyncService(0)->GetAccessTokenForTest();

  // Wait until the token has expired.
  base::PlatformThread::Sleep(base::Seconds(5));

  // Trigger an auth error on the server so PSS requests OA2TS for a new token
  // during the next sync cycle.
  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);
  SetOAuth2TokenResponse(kEmptyOAuth2Token, net::HTTP_INTERNAL_SERVER_ERROR,
                         net::OK);
  ASSERT_TRUE(AttemptToTriggerAuthError());
  ASSERT_TRUE(GetSyncService(0)->IsRetryingAccessTokenFetchForTest());

  // Trigger an auth success state and set up a new valid OAuth2 token.
  GetFakeServer()->ClearHttpError();
  SetOAuth2TokenResponse(kValidOAuth2Token, net::HTTP_OK, net::OK);

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

IN_PROC_BROWSER_TEST_F(SyncAuthTest, SyncPausedState) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  const syncer::DataTypeSet active_types =
      GetSyncService(0)->GetActiveDataTypes();
  ASSERT_FALSE(active_types.empty());

  // Enter the "Sync paused" state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Sync should have shut itself down.
  EXPECT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_FALSE(GetSyncService(0)->IsEngineInitialized());

  // The active data types should now be empty.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().empty());

  // Clear the "Sync paused" state again.
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  // SyncService will clear its auth error state only once it gets a valid
  // access token again, so wait for that to happen.
  NoAuthErrorChecker(GetSyncService(0)).Wait();
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Once the auth error is gone, wait for Sync to start up again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Now the active data types should be back.
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  EXPECT_EQ(GetSyncService(0)->GetActiveDataTypes(), active_types);
}

IN_PROC_BROWSER_TEST_F(SyncAuthTest, ShouldTrackDeletionsInSyncPausedState) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  // USS type.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
  // Pseudo-USS type.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  const GURL kTestURL("http://mail.google.com");

  PrefService* pref_service = GetProfile(0)->GetPrefs();

  // Create a bookmark...
  const bookmarks::BookmarkNode* bar = bookmarks_helper::GetBookmarkBarNode(0);
  ASSERT_FALSE(bookmarks_helper::HasNodeWithURL(0, kTestURL));
  const bookmarks::BookmarkNode* bookmark = bookmarks_helper::AddURL(
      0, bar, bar->children().size(), "Title", kTestURL);

  // ...set a pref...
  ASSERT_FALSE(HasUserPrefValue(pref_service, prefs::kHomePageIsNewTabPage));
  pref_service->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  // ...and wait for both to be synced up.
  UpdatedProgressMarkerChecker(GetSyncService(0)).Wait();

  // Enter the "Sync paused" state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
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

  // Clear the "Sync paused" state again.
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  // SyncService will clear its auth error state only once it gets a valid
  // access token again, so wait for that to happen.
  NoAuthErrorChecker(GetSyncService(0)).Wait();
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());
  // Once the auth error is gone, wait for Sync to start up again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Resuming sync could issue a reconfiguration, so wait until it finishes.
  SyncTransportActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Resuming sync should *not* have re-created the deleted items.
  EXPECT_FALSE(bookmarks_helper::HasNodeWithURL(0, kTestURL));
  EXPECT_FALSE(HasUserPrefValue(pref_service, prefs::kHomePageIsNewTabPage));
}

}  // namespace
