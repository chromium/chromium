// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_management_service.h"

#include <cstddef>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::StringPiece;
using ::base::Time;
using ::kids_chrome_management::FamilyMember;
using ::kids_chrome_management::FamilyRole;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::network::TestURLLoaderFactory;
using ::signin::ConsentLevel;
using ::signin::IdentityTestEnvironment;

std::unique_ptr<KeyedService> MakeTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

std::unique_ptr<KeyedService> CreateMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::MockSyncService>();
}

std::unique_ptr<TestingProfile> MakeTestingProfile(
    network::TestURLLoaderFactory& test_url_loader_factory,
    bool is_supervised = true) {
  TestingProfile::Builder builder;
  builder.SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                            base::BindRepeating(&MakeTestSigninClient));
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateMockSyncService));
  if (is_supervised) {
    builder.SetIsSupervisedProfile();
  }
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(builder);
}

FamilyMember* AddFamilyMember(ListFamilyMembersResponse& response,
                              FamilyRole role,
                              StringPiece display_name,
                              StringPiece user_id,
                              StringPiece email) {
  FamilyMember* member = response.add_members();
  member->set_role(role);
  *member->mutable_profile()->mutable_display_name() =
      std::string(display_name);
  *member->mutable_user_id() = std::string(user_id);
  *member->mutable_profile()->mutable_email() = std::string(email);
  return member;
}

FamilyMember* AddParentTo(ListFamilyMembersResponse& response) {
  return AddFamilyMember(response, FamilyRole::PARENT, "Alice", "alice_id",
                         "alice@gmail.com");
}
FamilyMember* AddHeadOfHouseholdTo(ListFamilyMembersResponse& response) {
  return AddFamilyMember(response, FamilyRole::HEAD_OF_HOUSEHOLD, "Bob",
                         "bob_id", "bob@gmail.com");
}
FamilyMember* AddChildTo(ListFamilyMembersResponse& response) {
  return AddFamilyMember(response, FamilyRole::CHILD, "Eve", "eve_id",
                         "eve@gmail.com");
}

class KidsManagementServiceTest : public ::testing::Test {
 protected:
  void ActivateValidAccessToken() {
    identity_test_environment
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", Time::Max());
  }
  void ActivateInvalidAccessToken(GoogleServiceAuthError error) {
    identity_test_environment
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(error);
  }
  AccountInfo CreatePrimaryAccount(StringPiece email = "joedoe@gmail.com") {
    return identity_test_environment->MakePrimaryAccountAvailable(
        std::string(email), ConsentLevel::kSignin);
  }

  // Maps pending request at index to its url spec.
  const std::string& GetPendingUrlSpec() {
    return test_url_loader_factory_.GetPendingRequest(0)->request.url.spec();
  }
  bool HasPendingRequest() { return test_url_loader_factory_.NumPending() > 0; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::
          MOCK_TIME};  // The test must run on Chrome_UIThread.
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_ =
      MakeTestingProfile(test_url_loader_factory_);
  raw_ptr<KidsManagementService> under_test_ =
      KidsManagementServiceFactory::GetForProfile(profile_.get());
  IdentityTestEnvironmentProfileAdaptor
      identity_test_environment_profile_adaptor_{
          profile_.get()};  // Must be owned by test fixture.
  raw_ptr<IdentityTestEnvironment> identity_test_environment{
      identity_test_environment_profile_adaptor_.identity_test_env()};
};

// Test if the data from endpoint is properly stored in the service.
TEST_F(KidsManagementServiceTest, FetchesData) {
  ListFamilyMembersResponse response;
  FamilyMember* member = AddChildTo(response);

  identity_test_environment->MakePrimaryAccountAvailable(
      member->profile().email(), ConsentLevel::kSignin);

  under_test_->StartFetchFamilyMembers();
  ActivateValidAccessToken();

  ASSERT_TRUE(HasPendingRequest());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingUrlSpec(), response.SerializeAsString());

  ASSERT_EQ(under_test_->family_members().size(), std::size_t(1));
  EXPECT_EQ(under_test_->family_members().at(0).profile().display_name(),
            member->profile().display_name());
}

// Test if the parent and head of household are set.
TEST_F(KidsManagementServiceTest, SetsCustodians) {
  ListFamilyMembersResponse response;
  FamilyMember* child = AddChildTo(response);
  FamilyMember* parent = AddParentTo(response);
  FamilyMember* head_of_household = AddHeadOfHouseholdTo(response);

  CreatePrimaryAccount(child->profile().email());

  under_test_->StartFetchFamilyMembers();

  ActivateValidAccessToken();

  ASSERT_TRUE(HasPendingRequest());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingUrlSpec(), response.SerializeAsString());

  EXPECT_EQ(profile_->GetTestingPrefService()->GetString(
                prefs::kSupervisedUserCustodianName),
            head_of_household->profile().display_name());
  EXPECT_EQ(profile_->GetTestingPrefService()->GetString(
                prefs::kSupervisedUserSecondCustodianName),
            parent->profile().display_name());
}

// Test if the daily fetches are scheduled.
TEST_F(KidsManagementServiceTest, SchedulesNextFetch) {
  ListFamilyMembersResponse response;
  FamilyMember* child = AddChildTo(response);

  CreatePrimaryAccount(child->profile().email());

  under_test_->StartFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());
  ActivateValidAccessToken();

  ASSERT_TRUE(HasPendingRequest());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingUrlSpec(), response.SerializeAsString());

  EXPECT_TRUE(under_test_->IsPendingNextFetchFamilyMembers());
}

// Test if server's persistent error is ignored to avoid ddosing it.
TEST_F(KidsManagementServiceTest, PersistentErrorsAreNotRetried) {
  CreatePrimaryAccount();

  under_test_->StartFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());
  ActivateValidAccessToken();

  ASSERT_TRUE(HasPendingRequest());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingUrlSpec(), "garbage");

  // Mocking time and FastForwardBy is impossible:
  // https://chromium.googlesource.com/chromium/src/+/master/docs/threading_and_tasks_testing.md#mock_time-in-browser-tests
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());
}

// Test if auth's transient errors are scheduled for retry.
TEST_F(KidsManagementServiceTest, TransientAuthErrorsAreRetried) {
  CreatePrimaryAccount();

  under_test_->StartFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());

  GoogleServiceAuthError error(
      GoogleServiceAuthError::State::CONNECTION_FAILED);
  ASSERT_TRUE(error.IsTransientError());
  ActivateInvalidAccessToken(error);

  // Mocking time and FastForwardBy is impossible:
  // https://chromium.googlesource.com/chromium/src/+/master/docs/threading_and_tasks_testing.md#mock_time-in-browser-tests
  EXPECT_TRUE(under_test_->IsPendingNextFetchFamilyMembers());
}

// Test if auth's persistent errors are not scheduled for retry.
TEST_F(KidsManagementServiceTest, PersistentAuthErrorsAreNotRetried) {
  CreatePrimaryAccount();

  under_test_->StartFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());

  GoogleServiceAuthError error(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  ASSERT_TRUE(error.IsPersistentError());
  ActivateInvalidAccessToken(error);

  // Mocking time and FastForwardBy is impossible:
  // https://chromium.googlesource.com/chromium/src/+/master/docs/threading_and_tasks_testing.md#mock_time-in-browser-tests
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());
}

// Test if server's transient errors are scheduled for retry.
TEST_F(KidsManagementServiceTest, TransientServerErrorsAreRetried) {
  CreatePrimaryAccount();

  under_test_->StartFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsPendingNextFetchFamilyMembers());

  ActivateValidAccessToken();

  ASSERT_TRUE(HasPendingRequest());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingUrlSpec(), /*content=*/"", net::HTTP_BAD_REQUEST);

  // Mocking time and FastForwardBy is impossible:
  // https://chromium.googlesource.com/chromium/src/+/master/docs/threading_and_tasks_testing.md#mock_time-in-browser-tests
  EXPECT_TRUE(under_test_->IsPendingNextFetchFamilyMembers());
}

// Tests whether the service handles the creation of accounts.
TEST_F(KidsManagementServiceTest, HandlePrimaryAccountChange) {
  bool state{false};

  identity_test_environment->identity_manager()->AddObserver(under_test_);
  under_test_->AddChildStatusReceivedCallback(
      base::BindLambdaForTesting([&state] { state = true; }));

  EXPECT_FALSE(state);
  AccountInfo account_info = CreatePrimaryAccount();
  account_info.picture_url = "http://example.com/picture.jpg";
  ASSERT_FALSE(account_info.IsEmpty())
      << "Setting picture_url should make account_info non-empty.";
  identity_test_environment->UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(state);
}

// Tests whether the service handles the removal of accounts.
TEST_F(KidsManagementServiceTest, HandlePrimaryAccountRemoval) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GTEST_SKIP() << "Removing primary accounts is not supported in ChromeOs, see "
                  "https://source.chromium.org/chromium/chromium/src/+/"
                  "main:components/signin/public/identity_manager/"
                  "identity_test_utils.cc;l=258;drc="
                  "53ff537cffdec939a5cb20fdb7df69f812d4a421 for reference";
#else

  identity_test_environment->identity_manager()->AddObserver(under_test_);
  AccountInfo account_info = CreatePrimaryAccount();
  account_info.picture_url = "http://example.com/picture.jpg";
  ASSERT_FALSE(account_info.IsEmpty())
      << "Setting picture_url should make account_info non-empty.";
  identity_test_environment->UpdateAccountInfoForAccount(account_info);

  {
    // The account status was still known when callback was registered, so it
    // was called immediately.
    bool state{false};
    under_test_->AddChildStatusReceivedCallback(
        base::BindLambdaForTesting([&state] { state = true; }));
    EXPECT_TRUE(state);
  }

  {
    // The account status was still known when callback was registered, so it
    // was called immediately.
    identity_test_environment->ClearPrimaryAccount();
    bool state{false};
    under_test_->AddChildStatusReceivedCallback(
        base::BindLambdaForTesting([&state] { state = true; }));
    EXPECT_TRUE(state);
  }
#endif
}

TEST_F(KidsManagementServiceTest, IsStoppable) {
  EXPECT_FALSE(under_test_->IsFetchFamilyMembersStarted());

  // One way to make the service active and inactive.
  under_test_->StartFetchFamilyMembers();
  EXPECT_TRUE(under_test_->IsFetchFamilyMembersStarted());

  under_test_->StopFetchFamilyMembers();
  EXPECT_FALSE(under_test_->IsFetchFamilyMembersStarted());

  // Another way to do so.
  under_test_->SetActive(true);
  EXPECT_TRUE(under_test_->IsFetchFamilyMembersStarted());

  under_test_->SetActive(false);
  EXPECT_FALSE(under_test_->IsFetchFamilyMembersStarted());
}

// If the primary account is not supervised, the service must not be activated.
TEST_F(KidsManagementServiceTest, DoesNotActivate) {
  std::unique_ptr<TestingProfile> unsupervised_profile =
      MakeTestingProfile(test_url_loader_factory_, /* is_supervised=*/false);
  KidsManagementService* under_test =
      KidsManagementServiceFactory::GetForProfile(unsupervised_profile.get());

  under_test->SetActive(true);
  EXPECT_FALSE(under_test->IsFetchFamilyMembersStarted());
}

TEST_F(KidsManagementServiceTest, ActivationIsIdempotent) {
  EXPECT_FALSE(under_test_->IsFetchFamilyMembersStarted());

  under_test_->SetActive(true);
  EXPECT_TRUE(under_test_->IsFetchFamilyMembersStarted());

  under_test_->SetActive(true);
  EXPECT_TRUE(under_test_->IsFetchFamilyMembersStarted());
}

// If the delegate for SupervisedUserService is properly set, activating the
// SupervisedUserService will activate the service under test.
TEST_F(KidsManagementServiceTest, InitSetsSupervisedUserServiceDelegate) {
  under_test_->Init();
  EXPECT_FALSE(under_test_->IsFetchFamilyMembersStarted());

  SupervisedUserServiceFactory::GetForProfile(profile_.get())->Init();
  EXPECT_TRUE(under_test_->IsFetchFamilyMembersStarted());
}
}  // namespace
