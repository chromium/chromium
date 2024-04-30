// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::Return;

using policy::MockCloudPolicyClient;
using policy::MockUserCloudPolicyStore;
using policy::UserCloudPolicyManager;
using RegistrationParameters =
    policy::CloudPolicyClient::RegistrationParameters;

using signin::IdentityManager;

namespace {

const ProfileManagementOicdTokens kExampleOidcTokens =
    ProfileManagementOicdTokens{.auth_token = "example_auth_token",
                                .id_token = "example_id_token"};
constexpr char kExampleSubjectIdentifier[] = "example_sbuject_id";
constexpr char kExampleUserDisplayName[] = "Test User";
constexpr char kExampleUserEmail[] = "user@test.com";
constexpr char kExampleGaiaId[] = "123";
constexpr char kExampleDmToken[] = "example_dm_token";

// Fake OIDC policy sign in service that simulates policy fetch success/failure.
class FakeUserPolicyOidcSigninService
    : public policy::UserPolicyOidcSigninService {
 public:
  static std::unique_ptr<KeyedService> CreateFakeUserPolicyOidcSigninService(
      bool will_policy_fetch_succeed,
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<FakeUserPolicyOidcSigninService>(
        profile, will_policy_fetch_succeed);
  }

  FakeUserPolicyOidcSigninService(Profile* profile,
                                  bool will_policy_fetch_succeed)
      : UserPolicyOidcSigninService(profile,
                                    nullptr,
                                    nullptr,
                                    profile->GetUserCloudPolicyManager(),
                                    nullptr,
                                    nullptr),
        test_profile_(profile),
        will_policy_fetch_succeed_(will_policy_fetch_succeed) {}

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override {
    if (!will_policy_fetch_succeed_) {
      std::move(callback).Run(will_policy_fetch_succeed_);
      return;
    }
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_gaia_id(kExampleGaiaId);
    static_cast<MockUserCloudPolicyStore*>(
        test_profile_->GetUserCloudPolicyManager()->core()->store())
        ->set_policy_data_for_testing(std::move(policy_data));
    std::move(callback).Run(will_policy_fetch_succeed_);
  }

  raw_ptr<Profile> test_profile_ = nullptr;
  bool will_policy_fetch_succeed_;
};

// Customized profile manager that ensures the created profiles are properly set
// up for testing.
class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir,
                                  bool will_policy_fetch_succeed_on_new_profile)
      : FakeProfileManager(user_data_dir),
        will_policy_fetch_succeed_on_new_profile_(
            will_policy_fetch_succeed_on_new_profile) {}

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate) override {
    TestingProfile::Builder builder;
    builder.SetPath(path);
    builder.SetDelegate(delegate);
    builder.SetUserCloudPolicyManager(std::move(policy_manager_));
    builder.AddTestingFactory(
        policy::UserPolicyOidcSigninServiceFactory::GetInstance(),
        base::BindRepeating(&FakeUserPolicyOidcSigninService::
                                CreateFakeUserPolicyOidcSigninService,
                            will_policy_fetch_succeed_on_new_profile_));
    builder.AddTestingFactory(
        policy::UserPolicySigninServiceFactory::GetInstance(),
        base::BindRepeating(&policy::FakeUserPolicySigninService::Build));

    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
  }

  void SetPolicyManagerForNextProfile(
      std::unique_ptr<UserCloudPolicyManager> policy_manager) {
    policy_manager_ = std::move(policy_manager);
  }

 private:
  std::unique_ptr<UserCloudPolicyManager> policy_manager_;
  bool will_policy_fetch_succeed_on_new_profile_;
};

// TODO(b/319479018): This mock should be removed after consent dialog is
// implemented and utilized here instead of the old interception dialog.
class MockDelegate : public OidcAuthenticationSigninInterceptor::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(bool,
              IsSigninInterceptionSupported,
              (const content::WebContents&),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowSigninInterceptionBubble,
              (content::WebContents*,
               const WebSigninInterceptor::Delegate::BubbleParameters&,
               base::OnceCallback<void(SigninInterceptionResult)>),
              (override));
  MOCK_METHOD(void,
              ShowFirstRunExperienceInNewProfile,
              (Browser*,
               const CoreAccountId&,
               WebSigninInterceptor::SigninInterceptionType),
              (override));
};

}  // namespace

class OidcAuthenticationSigninInterceptorTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<bool>,
      public ProfileManagerObserver {
 public:
  enum class RegistrationResult {
    kNoRegistrationExpected,
    kSuccess,
    kFailure,
  };

  OidcAuthenticationSigninInterceptorTest() {
    scoped_feature_list_.InitWithFeatureState(
        profile_management::features::kOidcAuthProfileManagement, true);
  }

  explicit OidcAuthenticationSigninInterceptorTest(
      bool will_policy_fetch_succeed)
      : OidcAuthenticationSigninInterceptorTest() {
    will_policy_fetch_succeed_ = will_policy_fetch_succeed;
  }

  ~OidcAuthenticationSigninInterceptorTest() override = default;

  void SetUp() override {
    auto profile_path = base::MakeAbsoluteFilePath(
        base::CreateUniqueTempDirectoryScopedToTest());
    auto profile_manager_unique = std::make_unique<UnittestProfileManager>(
        profile_path, will_policy_fetch_succeed_);
    unit_test_profile_manager_ = profile_manager_unique.get();
    SetUpProfileManager(profile_path, std::move(profile_manager_unique));
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->profile_manager()->AddObserver(this);

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    interceptor_ = std::make_unique<OidcAuthenticationSigninInterceptor>(
        profile(), std::move(delegate));
    interceptor_->SetDisableBrowserCreationAfterInterceptionForTesting(true);
    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->profile_manager()->RemoveObserver(this);
    added_profile_ = nullptr;
    unit_test_profile_manager_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  // If the 3P identity is not synced to Google, the interceptor should follow
  // the Dasherless workflow.
  bool is_3p_identity_synced() { return GetParam(); }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override { added_profile_ = profile; }

  // ProfileManagerObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    if (added_profile_ == profile) {
      added_profile_ = nullptr;
    }
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Build a test version CloudPolicyManager for testing profiles. Using
  // UserCloudPolicyManager should work for dasherless profiles should work too,
  // since we are using a fake policy sign in service.
  std::unique_ptr<UserCloudPolicyManager> BuildCloudPolicyManager() {
    auto mock_user_cloud_policy_store =
        std::make_unique<MockUserCloudPolicyStore>();
    EXPECT_CALL(*mock_user_cloud_policy_store, Load())
        .Times(testing::AnyNumber());

    return std::make_unique<UserCloudPolicyManager>(
        std::move(mock_user_cloud_policy_store), base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  // Test if profile is correctly created (or not created) by the interceptor
  // class with supplied arguments.
  void TestProfileCreationOrSwitch(
      const ProfileManagementOicdTokens& oidc_tokens,
      const std::string& subject_id,
      bool expect_profile_created,
      RegistrationResult expect_registration_attempt =
          RegistrationResult::kSuccess,
      SigninInterceptionResult interception_result =
          SigninInterceptionResult::kAccepted,
      OidcInterceptionStatus expected_interception_status =
          OidcInterceptionStatus::kCompleted,
      bool expect_dialog_to_show = true) {
    auto mock_client = std::make_unique<MockCloudPolicyClient>();
    base::RunLoop register_run_loop;
    auto* mock_client_ptr = mock_client.get();

    if (expect_registration_attempt == RegistrationResult::kFailure) {
      EXPECT_CALL(*mock_client_ptr,
                  RegisterWithOidcResponse(_, kExampleOidcTokens.auth_token,
                                           kExampleOidcTokens.id_token, _))
          .WillOnce(Invoke([&]() {
            mock_client_ptr->SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
            mock_client_ptr->NotifyClientError();
            register_run_loop.Quit();
          }));
    } else if (expect_registration_attempt == RegistrationResult::kSuccess) {
      EXPECT_CALL(*mock_client_ptr,
                  RegisterWithOidcResponse(_, kExampleOidcTokens.auth_token,
                                           kExampleOidcTokens.id_token, _))
          .WillOnce(Invoke([&]() {
            mock_client_ptr->SetDMToken(kExampleDmToken);
            mock_client_ptr->SetStatus(policy::DM_STATUS_SUCCESS);
            mock_client_ptr->oidc_user_display_name_ = kExampleUserDisplayName;
            mock_client_ptr->oidc_user_email_ = kExampleUserEmail;
            mock_client_ptr->third_party_identity_type_ =
                is_3p_identity_synced() ? policy::ThirdPartyIdentityType::
                                              OIDC_MANAGEMENT_DASHER_BASED
                                        : policy::ThirdPartyIdentityType::
                                              OIDC_MANAGEMENT_DASHERLESS;
            mock_client_ptr->NotifyRegistrationStateChanged();
            register_run_loop.Quit();
          }));
    }

    interceptor_->SetCloudPolicyClientForTesting(std::move(mock_client));

    const int num_profiles_before = TestingBrowserProcess::GetGlobal()
                                        ->profile_manager()
                                        ->GetNumberOfProfiles();
    const int expected_num_profiles_after =
        expect_profile_created ? num_profiles_before + 1 : num_profiles_before;

    if (expect_profile_created) {
      unit_test_profile_manager_->SetPolicyManagerForNextProfile(
          BuildCloudPolicyManager());
    }

    if (expect_dialog_to_show) {
      EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _))
          .Times(1)
          .WillOnce(Invoke(
              [&interception_result](
                  content::WebContents*,
                  const WebSigninInterceptor::Delegate::BubbleParameters&,
                  base::OnceCallback<void(SigninInterceptionResult)> callback) {
                std::move(callback).Run(interception_result);
                return nullptr;
              }));
    } else {
      EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _)).Times(0);
    }

    interceptor_->MaybeInterceptOidcAuthentication(
        web_contents(), oidc_tokens, subject_id,
        task_environment()->QuitClosure());

    if (expect_registration_attempt !=
        RegistrationResult::kNoRegistrationExpected) {
      register_run_loop.Run();
    }

    task_environment()->RunUntilQuit();
    EXPECT_EQ(interceptor_->interception_status(),
              expected_interception_status);

    int num_profiles_after = TestingBrowserProcess::GetGlobal()
                                 ->profile_manager()
                                 ->GetNumberOfProfiles();
    EXPECT_EQ(expected_num_profiles_after, num_profiles_after);

    if (expect_profile_created) {
      ASSERT_TRUE(added_profile_);
      auto* entry =
          TestingBrowserProcess::GetGlobal()
              ->profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(added_profile_->GetPath());

      EXPECT_EQ(entry->GetProfileManagementOidcTokens(), oidc_tokens);
      EXPECT_EQ(entry->GetProfileManagementId(), subject_id);
      if (will_policy_fetch_succeed_) {
        CoreAccountId account_id =
            IdentityManagerFactory::GetForProfile(added_profile_)
                ->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

        EXPECT_EQ(account_id.empty(), !is_3p_identity_synced());
        if (is_3p_identity_synced()) {
          ASSERT_TRUE(!account_id.IsEmail());
          EXPECT_EQ(account_id.ToString(), kExampleGaiaId);
        }
      }
    }
  }

 protected:
  std::unique_ptr<OidcAuthenticationSigninInterceptor> interceptor_;
  raw_ptr<MockDelegate> delegate_ = nullptr;  // Owned by `interceptor_`

  base::test::ScopedFeatureList scoped_feature_list_;
  bool will_policy_fetch_succeed_ = true;

  raw_ptr<Profile> added_profile_;
  raw_ptr<UnittestProfileManager> unit_test_profile_manager_;
};

TEST_P(OidcAuthenticationSigninInterceptorTest, ProfileCreationThenSwitch) {
  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/true);

  // Adding a new tab since the old one will be closed on successful
  // interception.
  AddTab(browser(), GURL("about:blank"));

  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/false,
                              /*expect_registration_attempt=*/
                              RegistrationResult::kNoRegistrationExpected);
}

TEST_P(OidcAuthenticationSigninInterceptorTest, MultipleProfileCreation) {
  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/true);
  AddTab(browser(), GURL("about:blank"));

  TestProfileCreationOrSwitch(kExampleOidcTokens, "new_subject_id",
                              /*expect_profile_created=*/true);
}

TEST_P(OidcAuthenticationSigninInterceptorTest, UserDidNotAccept) {
  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/false,
                              RegistrationResult::kNoRegistrationExpected,
                              SigninInterceptionResult::kDeclined,
                              OidcInterceptionStatus::kNoInterception);

  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/false,
                              RegistrationResult::kNoRegistrationExpected,
                              SigninInterceptionResult::kIgnored,
                              OidcInterceptionStatus::kNoInterception);

  TestProfileCreationOrSwitch(kExampleOidcTokens, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/false,
                              RegistrationResult::kNoRegistrationExpected,
                              SigninInterceptionResult::kDismissed,
                              OidcInterceptionStatus::kNoInterception);

  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false,
      RegistrationResult::kNoRegistrationExpected,
      SigninInterceptionResult::kAcceptedWithExistingProfile,
      OidcInterceptionStatus::kNoInterception);
}

TEST_P(OidcAuthenticationSigninInterceptorTest, InterceptionForSameProfile) {
  ProfileManagementOicdTokens new_example_token = ProfileManagementOicdTokens{
      .auth_token = "new_auth_token", .id_token = "new_id_token"};

  // Fake current TestProfile as an OIDC profile with the same subject ID.
  ProfileAttributesEntry* entry =
      TestingBrowserProcess::GetGlobal()
          ->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile()->GetPath());

  entry->SetProfileManagementOidcTokens(kExampleOidcTokens);
  entry->SetProfileManagementId(kExampleSubjectIdentifier);

  TestProfileCreationOrSwitch(new_example_token, kExampleSubjectIdentifier,
                              /*expect_profile_created=*/false,
                              RegistrationResult::kNoRegistrationExpected,
                              SigninInterceptionResult::kAccepted,
                              OidcInterceptionStatus::kNoInterception,
                              /*expect_dialog_to_show=*/false);
}

TEST_P(OidcAuthenticationSigninInterceptorTest, RegistrationFailure) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false, RegistrationResult::kFailure,
      SigninInterceptionResult::kAccepted, OidcInterceptionStatus::kError,
      /*expect_dialog_to_show=*/true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthenticationSigninInterceptorTest,
                         /*enable_oidc_interception=*/testing::Bool());

// Extra test class for cases when policy fetch fails.
class OidcAuthenticationSigninInterceptorFailureTest
    : public OidcAuthenticationSigninInterceptorTest {
 public:
  OidcAuthenticationSigninInterceptorFailureTest()
      : OidcAuthenticationSigninInterceptorTest(false) {}
};

TEST_P(OidcAuthenticationSigninInterceptorFailureTest, PolicyFetchFailure) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true, RegistrationResult::kSuccess,
      SigninInterceptionResult::kAccepted, OidcInterceptionStatus::kError,
      /*expect_dialog_to_show=*/true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthenticationSigninInterceptorFailureTest,
                         /*enable_oidc_interception=*/testing::Bool());
