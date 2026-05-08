// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"

#include <variant>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/mock_oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/mock_user_policy_oidc_signin_service.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_profile_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#include "components/policy/test_support/signature_provider.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

using policy::MockProfileCloudPolicyStore;
using policy::MockUserCloudPolicyStore;
using policy::ProfileCloudPolicyManager;
using policy::UserCloudPolicyManager;
using RegistrationParameters =
    policy::CloudPolicyClient::RegistrationParameters;

using signin::IdentityManager;

namespace policy {
namespace {

const char kOidcEnrollmentHistogramName[] = "Enterprise.OidcEnrollment";

const ProfileManagementOidcTokens kExampleOidcTokens =
    ProfileManagementOidcTokens("example_auth_token",
                                "example_id_token",
                                /*identity_name=*/u"");
constexpr char kExampleSubjectIdentifier[] = "example_subject_id";
constexpr char kExampleIssuerIdentifier[] = "example_issuer_id";
constexpr char kExampleUserDisplayName[] = "Test User";
constexpr char kExampleUserEmail[] = "user@test.com";
constexpr GaiaId::Literal kExampleGaiaId("123");
constexpr char kExampleDmToken[] = "example_dm_token";
constexpr char kExampleClientId[] = "random_client_id";

constexpr char kUniqueIdentifierTemplate[] = "iss:%s,sub:%s";

const char kOidcInterceptionSuffix[] = ".Interception";
const char kOidcProfileCreationSuffix[] = ".ProfileCreation";

const char kOidcFunnelSuffix[] = ".Funnel";
const char kOidcResultSuffix[] = ".Result";

constexpr char kFakeDeviceID[] = "fake-id";

class FakeBubbleHandle : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  FakeBubbleHandle(
      signin::SigninChoice choice,
      signin::SigninChoiceWithConfirmAndRetryCallback callback,
      base::OnceClosure done_callback,
      signin::SigninChoiceOperationResult expected_operation_result)
      : choice_(choice),
        callback_(std::move(callback)),
        done_callback_(std::move(done_callback)),
        expected_operation_result_(expected_operation_result) {}

  ~FakeBubbleHandle() override = default;

  void SimulateClick() {
    std::move(callback_).Run(
        choice_,
        base::BindOnce(
            [](base::OnceClosure done,
               signin::SigninChoiceOperationResult expected_result,
               signin::SigninChoiceOperationResult result,
               signin::SigninChoiceErrorType error_type) {
              CHECK_EQ(result, expected_result);
              std::move(done).Run();
            },
            std::move(done_callback_), expected_operation_result_),
        base::BindRepeating(
            [](signin::SigninChoiceOperationResult expected_result,
               signin::SigninChoiceOperationResult result,
               signin::SigninChoiceErrorType error_type) {
              CHECK_EQ(result, expected_result);
              // Retry callback should only be used in case of registration
              // timeout.
              CHECK_EQ(result,
                       signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT);
            },
            expected_operation_result_));
  }

  base::WeakPtr<FakeBubbleHandle> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const signin::SigninChoice choice_;
  signin::SigninChoiceWithConfirmAndRetryCallback callback_;
  base::OnceClosure done_callback_;
  const signin::SigninChoiceOperationResult expected_operation_result_;

  base::WeakPtrFactory<FakeBubbleHandle> weak_ptr_factory_{this};
};

// Fake OIDC policy sign in service that simulates policy fetch success/failure.
class FakeUserPolicyOidcSigninService
    : public policy::UserPolicyOidcSigninService {
 public:
  static std::unique_ptr<KeyedService> CreateFakeUserPolicyOidcSigninService(
      bool will_policy_fetch_succeed,
      MockJobCreationHandler* job_creation_handler,
      FakeDeviceManagementService* device_management_service,
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    auto fake_service =
        (profile->GetUserCloudPolicyManager())
            ? std::make_unique<FakeUserPolicyOidcSigninService>(
                  profile, job_creation_handler, device_management_service,
                  profile->GetUserCloudPolicyManager(),
                  will_policy_fetch_succeed)
            : std::make_unique<FakeUserPolicyOidcSigninService>(
                  profile, job_creation_handler, device_management_service,
                  profile->GetProfileCloudPolicyManager(),
                  will_policy_fetch_succeed);

    return std::move(fake_service);
  }

  FakeUserPolicyOidcSigninService(
      Profile* profile,
      MockJobCreationHandler* job_creation_handler,
      FakeDeviceManagementService* device_management_service,
      std::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
          policy_manager,
      bool will_policy_fetch_succeed)
      : policy::UserPolicyOidcSigninService(
            profile,
            g_browser_process->local_state(),
            static_cast<DeviceManagementService*>(device_management_service),
            policy_manager,
            IdentityManagerFactory::GetForProfile(profile),
            nullptr),
        test_profile_(profile),
        job_creation_handler_(job_creation_handler),
        device_management_service_(device_management_service),
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

    captured_callback_ = std::move(callback);

    EXPECT_CALL(*job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            device_management_service_->CaptureJobType(&captured_job_type_),
            [this]() { SimulatePolicyFetch(); }));

    UserPolicySigninServiceBase::FetchPolicyForSignedInUser(
        account_id, dm_token, client_id, user_affiliation_ids,
        test_shared_loader_factory, base::DoNothing());
  }

  void SimulatePolicyFetch() {
    if (captured_job_type_ ==
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH) {
      auto policy_data = std::make_unique<enterprise_management::PolicyData>();
      policy_data->set_gaia_id(kExampleGaiaId.ToString());
      if (test_profile_->GetProfileCloudPolicyManager()) {
        test_profile_->GetProfileCloudPolicyManager()
            ->core()
            ->store()
            ->set_policy_data_for_testing(std::move(policy_data));
      } else {
        test_profile_->GetUserCloudPolicyManager()
            ->core()
            ->store()
            ->set_policy_data_for_testing(std::move(policy_data));
      }
      std::move(captured_callback_).Run(will_policy_fetch_succeed_);
    }
  }

  raw_ptr<Profile> test_profile_ = nullptr;
  raw_ptr<MockJobCreationHandler> job_creation_handler_;
  raw_ptr<FakeDeviceManagementService> device_management_service_;
  bool will_policy_fetch_succeed_;

  DeviceManagementService::JobConfiguration::JobType captured_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  PolicyFetchCallback captured_callback_;
};

std::unique_ptr<KeyedService> CreateMalfunctionProfileIdService(
    content::BrowserContext* context) {
  // Intentionally return a wrong profile ID.
  std::string fake_profile_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  return std::make_unique<enterprise::ProfileIdService>(fake_profile_id);
}

std::unique_ptr<KeyedService> BuildMockInterceptor(
    int number_of_windows,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto mock_interceptor =
      std::make_unique<MockOidcAuthenticationSigninInterceptor>(
          profile, std::make_unique<DiceWebSigninInterceptorDelegate>());
  EXPECT_CALL(*mock_interceptor, CreateBrowserAfterSigninInterception())
      .Times(number_of_windows)
      .WillRepeatedly(testing::Return());

  return std::move(mock_interceptor);
}

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
  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowOidcInterceptionDialog,
              (content::WebContents*,
               const WebSigninInterceptor::Delegate::BubbleParameters&,
               signin::SigninChoiceWithConfirmAndRetryCallback,
               base::OnceClosure,
               base::RepeatingClosure),
              (override));
  MOCK_METHOD(void,
              ShowFirstRunExperienceInNewProfile,
              (Browser*,
               const CoreAccountId&,
               WebSigninInterceptor::SigninInterceptionType),
              (override));
  MOCK_METHOD(void,
              ShowSigninError,
              (content::WebContents*, const SigninUIError&),
              (override));
};

}  // namespace

class OidcAuthenticationSigninInterceptorTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool>,
      public ProfileManagerObserver {
 public:
  enum class RegistrationResult {
    kNoRegistrationExpected,
    kSuccess,
    kFailure,
    kTimeout,
  };

  OidcAuthenticationSigninInterceptorTest(bool will_policy_fetch_succeed = true,
                                          bool will_id_service_succeed = true) {
    scoped_feature_list_.InitWithFeatureState(
        profile_management::features::kOidcAuthProfileManagement, true);
    will_policy_fetch_succeed_ = will_policy_fetch_succeed;
    will_id_service_succeed_ = will_id_service_succeed;

    // Without setting test dm token storage, the profile ID service will fail
    // to retrieve client ID hence failing the service.
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetClientId(kFakeDeviceID);
  }

  ~OidcAuthenticationSigninInterceptorTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);

    UserPolicyOidcSigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FakeUserPolicyOidcSigninService::
                                CreateFakeUserPolicyOidcSigninService,
                            will_policy_fetch_succeed_, &job_creation_handler_,
                            device_management_service_.get()));

    OidcAuthenticationSigninInterceptorFactory::GetInstance()
        ->SetTestingFactory(context, base::BindRepeating(&BuildMockInterceptor,
                                                         number_of_windows_));

    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&policy::FakeUserPolicySigninService::Build));

    if (!will_id_service_succeed_) {
      enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
          context, base::BindRepeating(&CreateMalfunctionProfileIdService));
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    device_management_service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    BrowserPolicyConnector::SetDeviceManagementServiceForTesting(
        device_management_service_.get());

    g_browser_process->profile_manager()->AddObserver(this);

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    interceptor_ = std::make_unique<OidcAuthenticationSigninInterceptor>(
        browser()->profile(), std::move(delegate));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDownOnMainThread() override {
    g_browser_process->profile_manager()->RemoveObserver(this);
    added_profile_ = nullptr;
    BrowserPolicyConnector::SetDeviceManagementServiceForTesting(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // If the 3P identity is not synced to Google, the interceptor should follow
  // the Dasherless workflow.
  bool is_3p_identity_synced() { return GetParam(); }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override { added_profile_ = profile; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AddTabToCurrentBrowser(const GURL& url) {
    chrome::AddTabAt(browser(), url, -1, true);
  }

  // Test if profile is correctly created (or not created) by the interceptor
  // class with supplied arguments.
  void TestProfileCreationOrSwitch(
      const ProfileManagementOidcTokens& oidc_tokens,
      const std::string& issuer_id,
      const std::string& subject_id,
      bool expect_profile_created,
      int expected_number_of_windows,
      std::variant<OidcInterceptionFunnelStep, OidcProfileCreationFunnelStep>
          expected_last_funnel_step,
      std::variant<OidcInterceptionResult, OidcProfileCreationResult>
          expected_enrollment_result =
              OidcProfileCreationResult::kEnrollmentSucceeded,
      RegistrationResult expect_registration_attempt =
          RegistrationResult::kSuccess,
      signin::SigninChoice choice =
          signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE,
      bool expect_dialog_to_show = true,
      signin::SigninChoiceOperationResult expected_operation_result =
          signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS) {
    auto mock_client = std::make_unique<MockCloudPolicyClient>();
    base::RunLoop register_run_loop;
    auto* mock_client_ptr = mock_client.get();

    if (expect_registration_attempt == RegistrationResult::kFailure) {
      EXPECT_CALL(
          *mock_client_ptr,
          RegisterWithOidcResponse(_, kExampleOidcTokens.auth_token,
                                   kExampleOidcTokens.id_token, _, _, _, _))
          .WillOnce([&](const RegistrationParameters&, const std::string&,
                        const std::string&, const std::string&,
                        const base::TimeDelta&, bool,
                        CloudPolicyClient::ResultCallback callback) {
            mock_client_ptr->SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
            mock_client_ptr->NotifyClientError();
            std::move(callback).Run(CloudPolicyClient::Result(
                policy::DM_STATUS_TEMPORARY_UNAVAILABLE, /*net_error=*/1));
            register_run_loop.Quit();
          });
    } else if (expect_registration_attempt == RegistrationResult::kTimeout) {
      EXPECT_CALL(
          *mock_client_ptr,
          RegisterWithOidcResponse(_, kExampleOidcTokens.auth_token,
                                   kExampleOidcTokens.id_token, _, _, _, _))
          .WillOnce([&](const RegistrationParameters&, const std::string&,
                        const std::string&, const std::string&,
                        const base::TimeDelta&, bool,
                        CloudPolicyClient::ResultCallback callback) {
            mock_client_ptr->SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
            mock_client_ptr->NotifyClientError();
            std::move(callback).Run(CloudPolicyClient::Result(
                policy::DM_STATUS_TEMPORARY_UNAVAILABLE,
                /*net_error=*/net::ERR_TIMED_OUT));
            register_run_loop.Quit();
          });
    } else if (expect_registration_attempt == RegistrationResult::kSuccess) {
      EXPECT_CALL(
          *mock_client_ptr,
          RegisterWithOidcResponse(_, kExampleOidcTokens.auth_token,
                                   kExampleOidcTokens.id_token, _, _, _, _))
          .WillOnce([&](const RegistrationParameters&, const std::string&,
                        const std::string&, const std::string&,
                        const base::TimeDelta&, bool,
                        CloudPolicyClient::ResultCallback callback) {
            mock_client_ptr->SetDMToken(kExampleDmToken);
            mock_client_ptr->SetStatus(policy::DM_STATUS_SUCCESS);
            mock_client_ptr->client_id_ = kExampleClientId;
            mock_client_ptr->oidc_user_display_name_ = kExampleUserDisplayName;
            mock_client_ptr->oidc_user_email_ = kExampleUserEmail;
            mock_client_ptr->third_party_identity_type_ =
                is_3p_identity_synced() ? policy::ThirdPartyIdentityType::
                                              OIDC_MANAGEMENT_DASHER_BASED
                                        : policy::ThirdPartyIdentityType::
                                              OIDC_MANAGEMENT_DASHERLESS;
            mock_client_ptr->NotifyRegistrationStateChanged();
            std::move(callback).Run(CloudPolicyClient::Result(
                policy::DM_STATUS_SUCCESS, /*net_error=*/0));
            register_run_loop.Quit();
          });
    }

    interceptor_->SetCloudPolicyClientForTesting(std::move(mock_client));

    const int num_profiles_before =
        g_browser_process->profile_manager()->GetNumberOfProfiles();
    const int expected_num_profiles_after =
        expect_profile_created ? num_profiles_before + 1 : num_profiles_before;

    if (expect_profile_created) {
      number_of_windows_ = expected_number_of_windows;
    } else {
      CHECK_EQ(expected_number_of_windows, 0);
    }

    if (std::holds_alternative<OidcProfileCreationResult>(
            expected_enrollment_result) &&
        std::get<OidcProfileCreationResult>(expected_enrollment_result) ==
            OidcProfileCreationResult::kSwitchedToExistingProfile) {
      EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _))
          .Times(1)
          .WillRepeatedly(
              [](content::WebContents*,
                 const WebSigninInterceptor::Delegate::BubbleParameters&,
                 base::OnceCallback<void(SigninInterceptionResult)> callback) {
                std::move(callback).Run(SigninInterceptionResult::kAccepted);
                return nullptr;
              });
    } else if (expect_dialog_to_show) {
      EXPECT_CALL(*delegate_, ShowOidcInterceptionDialog(_, _, _, _, _))
          .Times(1)
          .WillRepeatedly(
              [&](content::WebContents*,
                  const WebSigninInterceptor::Delegate::BubbleParameters&,
                  signin::SigninChoiceWithConfirmAndRetryCallback callback,
                  base::OnceClosure done_callback, base::RepeatingClosure) {
                auto fake_bubble_handle = std::make_unique<FakeBubbleHandle>(
                    choice, std::move(callback), std::move(done_callback),
                    expected_operation_result);
                fake_bubble_handle_ = fake_bubble_handle->AsWeakPtr();
                return fake_bubble_handle;
              });
    } else {
      EXPECT_CALL(*delegate_, ShowOidcInterceptionDialog(_, _, _, _, _))
          .Times(0);
    }

    base::RunLoop run_loop;

    // Create the first tab so that web_contents() exists.
    AddTabToCurrentBrowser(GURL("about:blank"));

    interceptor_->MaybeInterceptOidcAuthentication(
        web_contents(), oidc_tokens, issuer_id, subject_id, std::string(),
        run_loop.QuitClosure());

    if (fake_bubble_handle_) {
      fake_bubble_handle_->SimulateClick();
      fake_bubble_handle_ = nullptr;
    }

    if (expect_registration_attempt !=
        RegistrationResult::kNoRegistrationExpected) {
      register_run_loop.Run();
    }

    if (expect_registration_attempt != RegistrationResult::kTimeout) {
      run_loop.Run();
    }

    int num_profiles_after =
        g_browser_process->profile_manager()->GetNumberOfProfiles();
    EXPECT_EQ(expected_num_profiles_after, num_profiles_after);

    if (expect_profile_created) {
      ASSERT_TRUE(base::test::RunUntil([&]() { return !!added_profile_; }));
      auto* entry =
          g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(added_profile_->GetPath());

      ProfileManagementOidcTokens tokens =
          entry->GetProfileManagementOidcTokens();
      EXPECT_EQ(tokens.auth_token, oidc_tokens.auth_token);
      EXPECT_EQ(tokens.auth_token, oidc_tokens.auth_token);
      EXPECT_EQ(tokens.identity_name, oidc_tokens.identity_name);
      EXPECT_EQ(tokens.state, oidc_tokens.state);
      EXPECT_EQ(base::UTF16ToUTF8(entry->GetGAIAName()),
                kExampleUserDisplayName);
      EXPECT_EQ(entry->GetProfileManagementId(),
                base::StringPrintf(kUniqueIdentifierTemplate, issuer_id.c_str(),
                                   subject_id.c_str()));

      EXPECT_EQ(added_profile_->GetPrefs()->GetString(
                    enterprise_signin::prefs::kPolicyRecoveryToken),
                kExampleDmToken);
      EXPECT_EQ(added_profile_->GetPrefs()->GetString(
                    enterprise_signin::prefs::kPolicyRecoveryClientId),
                kExampleClientId);

      if (will_policy_fetch_succeed_) {
        CoreAccountId account_id =
            IdentityManagerFactory::GetForProfile(added_profile_)
                ->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

        EXPECT_EQ(account_id.empty(), !is_3p_identity_synced());
        if (is_3p_identity_synced()) {
          ASSERT_TRUE(!account_id.IsEmail());
          EXPECT_EQ(account_id, CoreAccountId::FromGaiaId(kExampleGaiaId));
        }
      }
    }

    CheckFunnelAndResultHistogram(expected_last_funnel_step,
                                  expected_enrollment_result,
                                  expect_registration_attempt);
  }

  std::string GetIdentitySuffix() {
    return is_3p_identity_synced() ? ".Dasher-based" : ".Dasherless";
  }

  void CheckFunnelAndResultHistogram(
      std::variant<OidcInterceptionFunnelStep, OidcProfileCreationFunnelStep>
          expected_last_funnel_step,
      std::variant<OidcInterceptionResult, OidcProfileCreationResult>
          expected_enrollment_result,
      RegistrationResult expect_registration_attempt) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      if (std::holds_alternative<OidcInterceptionResult>(
              expected_enrollment_result)) {
        return histogram_tester_->GetBucketCount(
                   base::StrCat({kOidcEnrollmentHistogramName,
                                 kOidcInterceptionSuffix, kOidcResultSuffix}),
                   std::get<OidcInterceptionResult>(
                       expected_enrollment_result)) >= 1;
      } else {
        return histogram_tester_->GetBucketCount(
                   base::StrCat({kOidcEnrollmentHistogramName,
                                 kOidcProfileCreationSuffix, kOidcResultSuffix,
                                 GetIdentitySuffix()}),
                   std::get<OidcProfileCreationResult>(
                       expected_enrollment_result)) >= 1;
      }
    }));

    if (std::holds_alternative<OidcInterceptionFunnelStep>(
            expected_last_funnel_step)) {
      histogram_tester_->ExpectBucketCount(
          base::StrCat({kOidcEnrollmentHistogramName, kOidcInterceptionSuffix,
                        kOidcFunnelSuffix}),
          std::get<OidcInterceptionFunnelStep>(expected_last_funnel_step), 1);
    } else {
      histogram_tester_->ExpectBucketCount(
          base::StrCat({kOidcEnrollmentHistogramName,
                        kOidcProfileCreationSuffix, kOidcFunnelSuffix,
                        GetIdentitySuffix()}),
          std::get<OidcProfileCreationFunnelStep>(expected_last_funnel_step),
          1);
    }

    if (std::holds_alternative<OidcInterceptionResult>(
            expected_enrollment_result)) {
      histogram_tester_->ExpectBucketCount(
          base::StrCat({kOidcEnrollmentHistogramName, kOidcInterceptionSuffix,
                        kOidcResultSuffix}),
          std::get<OidcInterceptionResult>(expected_enrollment_result), 1);
    } else {
      histogram_tester_->ExpectBucketCount(
          base::StrCat({kOidcEnrollmentHistogramName,
                        kOidcProfileCreationSuffix, kOidcResultSuffix,
                        GetIdentitySuffix()}),
          std::get<OidcProfileCreationResult>(expected_enrollment_result), 1);
    }

    if (expect_registration_attempt == RegistrationResult::kFailure) {
      histogram_tester_->ExpectTotalCount(
          base::StrCat(
              {kOidcEnrollmentHistogramName, ".RegistrationLatency.Failure"}),
          1);
    } else if (expect_registration_attempt == RegistrationResult::kSuccess) {
      histogram_tester_->ExpectTotalCount(
          base::StrCat({kOidcEnrollmentHistogramName, GetIdentitySuffix(),
                        ".RegistrationLatency.Success"}),
          1);

      histogram_tester_->ExpectTotalCount(
          base::StrCat({kOidcEnrollmentHistogramName, GetIdentitySuffix(),
                        ".PolicyFetchLatency",
                        will_policy_fetch_succeed_ ? ".Success" : ".Failure"}),
          1);
    }

    // Preset profile GUID should be either unused or working properly.
    histogram_tester_->ExpectBucketCount(
        base::StrCat({kOidcEnrollmentHistogramName, kOidcProfileCreationSuffix,
                      kOidcResultSuffix, GetIdentitySuffix()}),
        OidcProfileCreationResult::kMismatchingProfileId,
        (will_id_service_succeed_) ? 0 : 1);

    histogram_tester_.reset();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  OidcProfileCreationFunnelStep GetLastFunnelStepForSuccess() {
    return is_3p_identity_synced()
               ? OidcProfileCreationFunnelStep::kAddingPrimaryAccount
               : OidcProfileCreationFunnelStep::kPolicyFetchStarted;
  }

 protected:
  std::unique_ptr<OidcAuthenticationSigninInterceptor> interceptor_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<MockDelegate> delegate_ = nullptr;  // Owned by `interceptor_`
  base::WeakPtr<FakeBubbleHandle> fake_bubble_handle_;

  base::test::ScopedFeatureList scoped_feature_list_;
  bool will_policy_fetch_succeed_;
  bool will_id_service_succeed_;

  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  std::unique_ptr<policy::FakeDeviceManagementService>
      device_management_service_;

  raw_ptr<Profile> added_profile_;
  int number_of_windows_ = 0;

  policy::FakeBrowserDMTokenStorage storage_;
};

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       ProfileCreationThenSwitch) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/2, GetLastFunnelStepForSuccess());

  AddTabToCurrentBrowser(GURL("http://foo/1"));

  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false,
      /*expected_number_of_windows=*/0,
      OidcProfileCreationFunnelStep::kPolicyFetchStarted,
      OidcProfileCreationResult::kSwitchedToExistingProfile,
      /*expect_registration_attempt=*/
      RegistrationResult::kNoRegistrationExpected,
      signin::SigninChoice::SIGNIN_CHOICE_CONTINUE,
      /*expect_dialog_to_show=*/false,
      signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       MultipleProfileCreationSameIssuer) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());

  AddTabToCurrentBrowser(GURL("http://foo/1"));

  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, "new_subject_id",
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       MultipleProfileCreationSameSubject) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());

  AddTabToCurrentBrowser(GURL("http://foo/1"));

  TestProfileCreationOrSwitch(
      kExampleOidcTokens, "some_other_issuer", kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       UserDidNotAccept) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false,
      /*expected_number_of_windows=*/0,
      OidcInterceptionFunnelStep::kConsetDialogShown,
      OidcInterceptionResult::kConsetDialogRejected,
      RegistrationResult::kNoRegistrationExpected,
      signin::SigninChoice::SIGNIN_CHOICE_CANCEL,
      /*expect_dialog_to_show=*/true,
      signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       InterceptionForSameProfile) {
  ProfileManagementOidcTokens new_example_token = ProfileManagementOidcTokens(
      "new_auth_token", "new_id_token", /*identity_name=*/u"");

  // Fake current TestProfile as an OIDC profile with the same subject ID.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(browser()->profile()->GetPath());

  entry->SetProfileManagementOidcTokens(kExampleOidcTokens);
  entry->SetProfileManagementId(base::StringPrintf(kUniqueIdentifierTemplate,
                                                   kExampleIssuerIdentifier,
                                                   kExampleSubjectIdentifier));

  TestProfileCreationOrSwitch(
      new_example_token, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false,
      /*expected_number_of_windows=*/0,
      OidcInterceptionFunnelStep::kEnrollmentStarted,
      OidcInterceptionResult::kNoInterceptForCurrentProfile,
      RegistrationResult::kNoRegistrationExpected,
      signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE,
      /*expect_dialog_to_show=*/false,
      signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       RegistrationFailure) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false, /*expected_number_of_windows=*/0,
      OidcInterceptionFunnelStep::kProfileRegistrationStarted,
      OidcInterceptionResult::kFailedToRegisterProfile,
      RegistrationResult::kFailure,
      signin::SigninChoice::SIGNIN_CHOICE_CONTINUE,
      /*expect_dialog_to_show=*/true,
      signin::SigninChoiceOperationResult::SIGNIN_ERROR);
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       RegistrationTimeout) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/false, /*expected_number_of_windows=*/0,
      OidcInterceptionFunnelStep::kProfileRegistrationStarted,
      OidcInterceptionResult::kRegistrationTimeout,
      RegistrationResult::kTimeout,
      signin::SigninChoice::SIGNIN_CHOICE_CONTINUE,
      /*expect_dialog_to_show=*/true,
      signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT);
}

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorTest,
                       PolicyRecoveryFromPref) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());

  // Manually remove the fetched policies to simulate policy loss.
  if (is_3p_identity_synced()) {
    added_profile_->GetUserCloudPolicyManager()
        ->core()
        ->store()
        ->set_policy_data_for_testing(nullptr);
  } else {
    added_profile_->GetProfileCloudPolicyManager()
        ->core()
        ->store()
        ->set_policy_data_for_testing(nullptr);
  }

  policy::UserPolicyOidcSigninServiceFactory::GetForProfile(added_profile_)
      ->AttemptToRestorePolicy();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return is_3p_identity_synced()
               ? added_profile_->GetUserCloudPolicyManager()
                     ->core()
                     ->store()
                     ->has_policy()
               : added_profile_->GetProfileCloudPolicyManager()
                     ->core()
                     ->store()
                     ->has_policy();
  }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthenticationSigninInterceptorTest,
                         /*is_3p_identity_synced=*/testing::Bool());

// Extra test class for cases when policy fetch fails.
class OidcAuthenticationSigninInterceptorFetchFailureTest
    : public OidcAuthenticationSigninInterceptorTest {
 public:
  OidcAuthenticationSigninInterceptorFetchFailureTest()
      : OidcAuthenticationSigninInterceptorTest(
            /*will_policy_fetch_succeed=*/false,
            /*will_id_service_succeed=*/true) {}
};

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorFetchFailureTest,
                       PolicyFetchFailure) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true, /*expected_number_of_windows=*/1,
      OidcProfileCreationFunnelStep::kPolicyFetchStarted,
      OidcProfileCreationResult::kFailedToFetchPolicy,
      RegistrationResult::kSuccess,
      signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE,
      /*expect_dialog_to_show=*/true,
      signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthenticationSigninInterceptorFetchFailureTest,
                         /*is_3p_identity_synced=*/testing::Bool());

// Extra test class for cases when profile id service fails.
class OidcAuthenticationSigninInterceptorIdFailureTest
    : public OidcAuthenticationSigninInterceptorTest {
 public:
  OidcAuthenticationSigninInterceptorIdFailureTest()
      : OidcAuthenticationSigninInterceptorTest(
            /*will_policy_fetch_succeed=*/true,
            /*will_id_service_succeed=*/false) {}
};

IN_PROC_BROWSER_TEST_P(OidcAuthenticationSigninInterceptorIdFailureTest,
                       DeviceIdFailure) {
  TestProfileCreationOrSwitch(
      kExampleOidcTokens, kExampleIssuerIdentifier, kExampleSubjectIdentifier,
      /*expect_profile_created=*/true,
      /*expected_number_of_windows=*/1, GetLastFunnelStepForSuccess());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthenticationSigninInterceptorIdFailureTest,
                         /*is_3p_identity_synced=*/testing::Bool());

}  // namespace policy
