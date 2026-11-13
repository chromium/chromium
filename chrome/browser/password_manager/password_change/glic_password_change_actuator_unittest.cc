// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/glic_password_change_actuator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/affiliations/core/browser/match_type.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/origin_gating/core/task_policy_config.h"
#include "components/password_manager/core/browser/actor_login/password_change_from_checkup_actor_login_service.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_prediction_waiter.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;

constexpr char kTestUrl[] = "https://example.com/password";
constexpr char16_t kTestUsername[] = u"test_user@example.com";
constexpr char16_t kTestPassword[] = u"test_password";

class MockPasswordChangeActuatorObserver
    : public PasswordChangeActuator::Observer {
 public:
  MOCK_METHOD(void,
              OnActuationStateChanged,
              (PasswordChangeActuator::State),
              (override));
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MOCK_METHOD(content::WebContents*,
              OpenURLFromTab,
              (content::WebContents*,
               const content::OpenURLParams&,
               base::OnceCallback<void(content::NavigationHandle&)>),
              (override));
};

class MockGenerationPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
};

class MockGenerationPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(password_manager::PasswordGenerationFrameHelper*,
              GetPasswordGenerationHelper,
              (),
              (override));
};

class MockPasswordGenerationFrameHelper
    : public password_manager::PasswordGenerationFrameHelper {
 public:
  using password_manager::PasswordGenerationFrameHelper::
      PasswordGenerationFrameHelper;
  MOCK_METHOD(autofill::PasswordRequirementsSpec,
              GetPasswordRequirementsSpec,
              (const GURL&,
               autofill::password_generation::PasswordGenerationType,
               autofill::FormSignature,
               autofill::FieldSignature,
               uint64_t),
              (override));
};

// Creates a change password form with a current and a new password field.
autofill::FormData CreateChangePasswordFormData() {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(autofill::test::CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password", /*value=*/"",
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.back().set_is_enabled(true);
  fields.push_back(autofill::test::CreateTestFormField(
      /*label=*/"New Password:", /*name=*/"new-password", /*value=*/"",
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.back().set_is_enabled(true);
  autofill::FormData form;
  form.set_url(GURL(kTestUrl));
  form.set_fields(std::move(fields));
  return form;
}

// Creates a login form with a username and a password field.
autofill::FormData CreateLoginFormData() {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(autofill::test::CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username", /*value=*/"",
      autofill::FormControlType::kInputText));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.back().set_is_enabled(true);
  fields.push_back(autofill::test::CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password", /*value=*/"",
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.back().set_is_enabled(true);
  autofill::FormData form;
  form.set_url(GURL(kTestUrl));
  form.set_fields(std::move(fields));
  return form;
}

password_manager::StoredCredential CreateTestCredential() {
  GURL url(kTestUrl);
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = url::Origin::Create(url).GetURL().spec();
  form.username_value = kTestUsername;
  form.password_value = password_manager::PasswordString(kTestPassword);
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  form.match_type = affiliations::MatchType::kExact;
  return password_manager::FromPasswordForm(std::move(form));
}

}  // namespace

class GlicPasswordChangeActuatorTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicPasswordChangeActuatorTest() = default;
  ~GlicPasswordChangeActuatorTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            actor::ActorKeyedServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<actor::ActorKeyedServiceFake>(
                  Profile::FromBrowserContext(context));
            })},
        TestingProfile::TestingFactory{
            glic::GlicKeyedServiceFactory::GetInstance(),
            base::BindRepeating(
                &GlicPasswordChangeActuatorTest::CreateMockGlicService,
                base::Unretained(
                    const_cast<GlicPasswordChangeActuatorTest*>(this)))},
        TestingProfile::TestingFactory{
            AffiliationServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  NiceMock<affiliations::MockAffiliationService>>();
            })},
    };
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));
    ChromeRenderViewHostTestHarness::SetUp();

    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_originator_tab_);
    EXPECT_CALL(mock_originator_tab_, GetProfile())
        .WillRepeatedly(Return(profile()));
    EXPECT_CALL(mock_originator_tab_, GetContents())
        .WillRepeatedly(Return(web_contents()));
    ON_CALL(mock_originator_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    actuation_web_contents_ = CreateTestWebContents();
    tabs::TabLookupFromWebContents::CreateForWebContents(
        actuation_web_contents_.get(), &mock_actuation_tab_);
    EXPECT_CALL(mock_actuation_tab_, GetProfile())
        .WillRepeatedly(Return(profile()));
    EXPECT_CALL(mock_actuation_tab_, GetContents())
        .WillRepeatedly(Return(actuation_web_contents_.get()));
    ON_CALL(mock_actuation_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    actor::ActorKeyedService::Get(profile())->GetSurfaceRegistry().OnTabCreated(
        mock_actuation_tab_);

    web_contents()->SetDelegate(&mock_web_contents_delegate_);
    ON_CALL(mock_web_contents_delegate_, OpenURLFromTab(web_contents(), _, _))
        .WillByDefault(Return(actuation_web_contents_.get()));

    ON_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
            affiliations::AffiliatedFacets(), true));
    actuator_ = std::make_unique<GlicPasswordChangeActuator>(
        CreateTestCredential(), web_contents(), profile());
    actuator_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    actuator_->RemoveObserver(&mock_observer_);
    actuator_.reset();
    custom_actuator_.reset();
    mock_glic_service_ = nullptr;
    actuation_web_contents_.reset();
    web_contents()->SetDelegate(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
  }

  std::unique_ptr<KeyedService> CreateMockGlicService(
      content::BrowserContext* context) {
    Profile* p = Profile::FromBrowserContext(context);
    auto service = std::make_unique<NiceMock<glic::MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(p),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);
    mock_glic_service_ = service.get();
    return service;
  }

  actor::ActorKeyedServiceFake* actor_service() {
    return static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedService::Get(profile()));
  }

  const origin_gating::TaskPolicyConfig& GetContainerConfig(
      const GURL& target_url = GURL(),
      const GURL& credential_url = GURL(kTestUrl)) {
    if (target_url.is_empty() && credential_url == GURL(kTestUrl)) {
      actuator_->Start();
    } else {
      password_manager::PasswordForm form;
      form.url = credential_url;
      form.signon_realm = url::Origin::Create(credential_url).GetURL().spec();
      form.username_value = kTestUsername;
      form.password_value = password_manager::PasswordString(kTestPassword);
      form.in_store = password_manager::PasswordForm::Store::kProfileStore;
      form.match_type = affiliations::MatchType::kExact;
      custom_actuator_ = std::make_unique<GlicPasswordChangeActuator>(
          password_manager::FromPasswordForm(std::move(form)), web_contents(),
          profile(), target_url);
      custom_actuator_->Start();
    }
    actor::TaskId task_id = actor_service()->CreateTaskForTesting();
    actor::ActorTask* task = actor_service()->GetTask(task_id);
    CHECK(task);
    actor::AddTabToTask(mock_actuation_tab(), *task);
    actor_service()->NotifyTaskStateChanged(*task);
    const auto& slot = task->GetExecutionEngine()
                           .GetOriginGatingChecker()
                           .task_policy_config_slot();
    CHECK(slot.has_value());
    return slot.value();
  }

  GlicPasswordChangeActuator* actuator() { return actuator_.get(); }
  NiceMock<MockPasswordChangeActuatorObserver>& observer() {
    return mock_observer_;
  }
  tabs::MockTabInterface& mock_actuation_tab() { return mock_actuation_tab_; }
  NiceMock<glic::MockGlicKeyedService>* mock_glic_service() {
    return mock_glic_service_;
  }

  affiliations::MockAffiliationService& mock_affiliation_service() {
    return *static_cast<affiliations::MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(profile()));
  }

  void SetupAffiliations(const std::string& affiliated_realm) {
    EXPECT_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
        .WillOnce(base::test::RunOnceCallback<1>(
            affiliations::AffiliatedFacets{affiliations::Facet(
                affiliations::FacetURI::FromPotentiallyInvalidSpec(
                    affiliated_realm))},
            true));
  }

 private:
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting
      scoped_glic_bypass_;
  base::ScopedTempDir temp_dir_;
  glic::GlicProfileManager glic_profile_manager_;
  raw_ptr<NiceMock<glic::MockGlicKeyedService>> mock_glic_service_ = nullptr;
  ui::UnownedUserDataHost unowned_user_data_host_;
  tabs::MockTabInterface mock_originator_tab_;
  tabs::MockTabInterface mock_actuation_tab_;
  MockWebContentsDelegate mock_web_contents_delegate_;
  std::unique_ptr<content::WebContents> actuation_web_contents_;
  NiceMock<MockPasswordChangeActuatorObserver> mock_observer_;
  std::unique_ptr<GlicPasswordChangeActuator> actuator_;
  std::unique_ptr<GlicPasswordChangeActuator> custom_actuator_;
};

// 1. PasswordChangeSuccessNotifiesObserver:
// Simulates the flow and OnUpdate with kTerminalCompletion /
// PASSWORD_CHANGE_FINISHED_SUCCESSFULLY, verifying state transitions to
// kPasswordSuccessfullyChanged and actor login service configuration.
TEST_F(GlicPasswordChangeActuatorTest, PasswordChangeSuccessNotifiesObserver) {
  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));

  // Start the actuator.
  actuator()->Start();

  // Create task and verify its ActorLoginService is set.
  actor::TaskId task_id = actor_service()->CreateTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  ASSERT_TRUE(task);
  actor::AddTabToTask(mock_actuation_tab(), *task);
  actor_service()->NotifyTaskStateChanged(*task);

  EXPECT_NE(
      static_cast<actor_login::PasswordChangeFromCheckupActorLoginService*>(
          &task->GetExecutionEngine().GetActorLoginService()),
      nullptr);

  // Deliver successful completion update via public OnUpdate.
  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kPasswordSuccessfullyChanged));

  auto update = glic::mojom::ExperimentalTriggeringUpdate::New();
  update->type =
      glic::mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  update->data =
      "Verification complete: PASSWORD_CHANGE_FINISHED_SUCCESSFULLY on page.";
  actuator()->OnUpdate(std::move(update),
                       glic::mojom::SubscriberObservationType::kUpdate);
}

// 2. PasswordChangeInterruptionPausedNotifiesOtpDetected:
// Simulates OnUpdate with kPaused during actuation, verifying kOtpDetected.
TEST_F(GlicPasswordChangeActuatorTest,
       PasswordChangeInterruptionPausedNotifiesOtpDetected) {
  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));
  EXPECT_CALL(observer(), OnActuationStateChanged(
                              PasswordChangeActuator::State::kOtpDetected));

  actuator()->Start();

  auto update = glic::mojom::ExperimentalTriggeringUpdate::New();
  update->type = glic::mojom::ExperimentalTriggeringUpdateType::kYieldToUser;
  actuator()->OnUpdate(std::move(update),
                       glic::mojom::SubscriberObservationType::kUpdate);
}

// 3. PasswordChangeFailedNotifiesObserver:
// Simulates OnUpdate with kTerminalFailed and FAILED_TO_CHANGE_PASSWORD,
// verifying observer is notified with kPasswordChangeFailed.
TEST_F(GlicPasswordChangeActuatorTest, PasswordChangeFailedNotifiesObserver) {
  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));

  actuator()->Start();

  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kPasswordChangeFailed));

  auto update = glic::mojom::ExperimentalTriggeringUpdate::New();
  update->type = glic::mojom::ExperimentalTriggeringUpdateType::kTerminalFailed;
  update->data = "FAILED_TO_CHANGE_PASSWORD: could not submit change form";
  actuator()->OnUpdate(std::move(update),
                       glic::mojom::SubscriberObservationType::kUpdate);
}

// 4. FindFormFailedNotifiesChangePasswordFormNotFound:
// Simulates OnUpdate with FAILED_TO_FIND_CHANGE_PASSWORD_FORM during the find
// form step, verifying observer is notified with kChangePasswordFormNotFound.
TEST_F(GlicPasswordChangeActuatorTest,
       FindFormFailedNotifiesChangePasswordFormNotFound) {
  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));
  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kChangePasswordFormNotFound));

  actuator()->Start();

  auto update = glic::mojom::ExperimentalTriggeringUpdate::New();
  update->type =
      glic::mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  update->data =
      "FAILED_TO_FIND_CHANGE_PASSWORD_FORM: could not locate password form";
  actuator()->OnUpdate(std::move(update),
                       glic::mojom::SubscriberObservationType::kUpdate);
}

// Verifies that TaskPolicyConfig permits actuation on the target site and
// its subdomains.
TEST_F(GlicPasswordChangeActuatorTest,
       TaskPolicyConfigAllowsTargetSiteAndSubdomains) {
  const auto& config =
      GetContainerConfig(GURL("https://auth.target.org/change_password"),
                         GURL("https://credential.com/login"));
  const origin_gating::ClientTool kClickTool(
      optimization_guide::proto::RuleMetadata::CAPABILITY_CLICK);

  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://auth.target.org")), kClickTool));
  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://sub.auth.target.org")), kClickTool));
  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://target.org")), kClickTool));
}

// Verifies that TaskPolicyConfig permits actuation on the credential site
// and its subdomains.
TEST_F(GlicPasswordChangeActuatorTest,
       TaskPolicyConfigAllowsCredentialSiteAndSubdomains) {
  const auto& config =
      GetContainerConfig(GURL("https://auth.target.org/change_password"),
                         GURL("https://credential.com/login"));
  const origin_gating::ClientTool kClickTool(
      optimization_guide::proto::RuleMetadata::CAPABILITY_CLICK);

  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://credential.com")), kClickTool));
  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://accounts.credential.com")),
      kClickTool));
}

// Verifies that TaskPolicyConfig allows navigation between the target site
// and the credential site.
TEST_F(GlicPasswordChangeActuatorTest,
       TaskPolicyConfigAllowsNavigationBetweenAllowedSites) {
  const auto& config =
      GetContainerConfig(GURL("https://auth.target.org/change_password"),
                         GURL("https://credential.com/login"));

  EXPECT_TRUE(config.IsNavigationAllowed(
      url::Origin::Create(GURL("https://auth.target.org")),
      url::Origin::Create(GURL("https://credential.com"))));
  EXPECT_TRUE(config.IsNavigationAllowed(
      url::Origin::Create(GURL("https://credential.com")),
      url::Origin::Create(GURL("https://sub.auth.target.org"))));
}

// Verifies that TaskPolicyConfig blocks actuation and navigation on
// unauthorized external sites.
TEST_F(GlicPasswordChangeActuatorTest,
       TaskPolicyConfigBlocksUnauthorizedSites) {
  const auto& config =
      GetContainerConfig(GURL("https://auth.example.org/change_password"),
                         GURL("https://example.com/login"));
  const origin_gating::ClientTool kClickTool(
      optimization_guide::proto::RuleMetadata::CAPABILITY_CLICK);

  EXPECT_FALSE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://attacker.com")), kClickTool));
  EXPECT_FALSE(config.IsNavigationAllowed(
      url::Origin::Create(GURL("https://auth.example.org")),
      url::Origin::Create(GURL("https://attacker.com"))));
}

// Verifies that when change_password_url is not provided, the container config
// permits the credential site and subdomains, and blocks third-party sites.
TEST_F(GlicPasswordChangeActuatorTest,
       TaskPolicyConfigDefaultsToCredentialSite) {
  const auto& config = GetContainerConfig();
  const origin_gating::ClientTool kClickTool(
      optimization_guide::proto::RuleMetadata::CAPABILITY_CLICK);

  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://example.com")), kClickTool));
  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://auth.example.com")), kClickTool));

  EXPECT_FALSE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://attacker.com")), kClickTool));
  EXPECT_FALSE(config.IsNavigationAllowed(
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://attacker.com"))));
}

// Verifies that TaskPolicyConfig permits actuation and navigation on
// affiliated web facets.
TEST_F(GlicPasswordChangeActuatorTest, TaskPolicyConfigAllowsAffiliatedSites) {
  const std::string kAffiliatedRealm = "https://affiliated.com/";
  SetupAffiliations(kAffiliatedRealm);
  const auto& config = GetContainerConfig();
  const origin_gating::ClientTool kClickTool(
      optimization_guide::proto::RuleMetadata::CAPABILITY_CLICK);

  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL(kAffiliatedRealm)), kClickTool));
  EXPECT_TRUE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://sub.affiliated.com")), kClickTool));

  EXPECT_TRUE(
      config.IsNavigationAllowed(url::Origin::Create(GURL(kTestUrl)),
                                 url::Origin::Create(GURL(kAffiliatedRealm))));

  EXPECT_FALSE(config.IsActuationAllowed(
      url::Origin::Create(GURL("https://attacker.com")), kClickTool));
}

// Verifies that when AffiliationService fails to return affiliations, the
// observer is notified with kPasswordChangeFailed and actuation is not started.
TEST_F(GlicPasswordChangeActuatorTest,
       AffiliationFailureNotifiesPasswordChangeFailed) {
  EXPECT_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(base::test::RunOnceCallback<1>(
          affiliations::AffiliatedFacets{
              affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
                  "https://affiliated.com"))},
          false));

  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_, testing::Matcher<glic::GlicInvokeOptions>(_),
                  testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(0);
  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kPasswordChangeFailed));

  actuator()->Start();
}

// Verifies that Start() requests affiliations for the target URL first and only
// proceeds with actuation once OnAffiliationsReceived is invoked.
TEST_F(GlicPasswordChangeActuatorTest,
       StartRequestsAffiliationsForTargetUrlBeforeActuation) {
  const GURL kTargetUrl("https://target-change-password.com/settings");
  affiliations::AffiliationService::ResultCallback affiliation_callback;
  EXPECT_CALL(
      mock_affiliation_service(),
      GetAffiliationsAndBranding(affiliations::FacetURI::FromCanonicalSpec(
                                     "https://target-change-password.com"),
                                 _))
      .WillOnce([&](const affiliations::FacetURI&,
                    affiliations::AffiliationService::ResultCallback callback) {
        affiliation_callback = std::move(callback);
      });

  auto custom_actuator = std::make_unique<GlicPasswordChangeActuator>(
      CreateTestCredential(), web_contents(), profile(), kTargetUrl);

  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_, testing::Matcher<glic::GlicInvokeOptions>(_),
                  testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(0);
  custom_actuator->Start();
  ASSERT_TRUE(affiliation_callback);

  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_, testing::Matcher<glic::GlicInvokeOptions>(_),
                  testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(1);
  std::move(affiliation_callback)
      .Run(affiliations::AffiliatedFacets{affiliations::Facet(
               affiliations::FacetURI::FromCanonicalSpec(
                   "https://affiliated.com"))},
           true);
}

// Tests for `GlicPasswordChangeActuator::GetOrGenerateNewPassword()`.
class GlicPasswordChangeActuatorNewPasswordTest
    : public GlicPasswordChangeActuatorTest {
 public:
  void SetUp() override {
    GlicPasswordChangeActuatorTest::SetUp();
    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(Return(password_store_.get()));
    ON_CALL(driver_, GetPasswordGenerationHelper)
        .WillByDefault(Return(&generation_helper_));
  }

  // Creates a form manager observing a change password form, which is aware
  // of `stored_credentials`.
  password_manager::PasswordFormManager* CreateFormManager(
      const std::vector<password_manager::PasswordForm>& stored_credentials) {
    return CreateFormManager(CreateChangePasswordFormData(),
                             stored_credentials);
  }

  password_manager::PasswordFormManager* CreateFormManager(
      const autofill::FormData& form_data,
      const std::vector<password_manager::PasswordForm>& stored_credentials) {
    form_manager_ = std::make_unique<password_manager::PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), form_data, &form_fetcher_,
        std::make_unique<password_manager::PasswordSaveManagerImpl>(&client_),
        /*metrics_recorder=*/nullptr);
    form_fetcher_.SetBestMatches(stored_credentials);
    form_fetcher_.SetNonFederated(stored_credentials);
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager_.get())
        ->OnWaitCompleted();
    return form_manager_.get();
  }

  password_manager::PasswordForm CreateStoredCredential(
      std::u16string password) {
    password_manager::PasswordForm credential;
    credential.url = GURL(kTestUrl);
    credential.signon_realm = GURL(kTestUrl).GetWithEmptyPath().spec();
    credential.username_value = kTestUsername;
    credential.password_value =
        password_manager::PasswordString(std::move(password));
    credential.match_type = affiliations::MatchType::kExact;
    credential.in_store = password_manager::PasswordForm::Store::kProfileStore;
    credential.scheme = password_manager::PasswordForm::Scheme::kHtml;
    return credential;
  }

  MockGenerationPasswordManagerDriver& driver() { return driver_; }
  MockPasswordGenerationFrameHelper& generation_helper() {
    return generation_helper_;
  }
  password_manager::MockPasswordStoreInterface& password_store() {
    return *password_store_;
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  NiceMock<MockGenerationPasswordManagerClient> client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<
          NiceMock<password_manager::MockPasswordStoreInterface>>();
  NiceMock<MockGenerationPasswordManagerDriver> driver_;
  NiceMock<MockPasswordGenerationFrameHelper> generation_helper_{&client_,
                                                                 &driver_};
  password_manager::FakeFormFetcher form_fetcher_;
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;
};

TEST_F(GlicPasswordChangeActuatorNewPasswordTest,
       GeneratesPasswordBasedOnRequirementsAndSavesBackup) {
  password_manager::PasswordFormManager* form_manager =
      CreateFormManager({CreateStoredCredential(kTestPassword)});
  autofill::PasswordRequirementsSpec spec;
  spec.set_max_length(10);
  EXPECT_CALL(
      generation_helper(),
      GetPasswordRequirementsSpec(
          GURL(kTestUrl),
          autofill::password_generation::PasswordGenerationType::kAutomatic, _,
          _, _))
      .WillOnce(Return(spec));
  password_manager::StoredCredential saved_credential;
  EXPECT_CALL(password_store(), UpdateLogin)
      .WillOnce(MoveArg<0>(&saved_credential));

  PasswordChangeActuator::NewPasswordResult new_password =
      actuator()->GetOrGenerateNewPassword(form_manager);

  ASSERT_TRUE(new_password.has_value());
  EXPECT_EQ(new_password->size(), 10u);
  EXPECT_EQ(saved_credential.username_value, kTestUsername);
  EXPECT_EQ(saved_credential.password_value.value(), kTestPassword);
  EXPECT_EQ(saved_credential.GetPasswordBackup(), new_password->value());
  EXPECT_THAT(actuator()->GetOrGenerateNewPassword(/*form_manager=*/nullptr),
              base::test::ValueIs(*new_password));
}

TEST_F(GlicPasswordChangeActuatorNewPasswordTest,
       ReturnsAlreadyGeneratedPassword) {
  password_manager::PasswordFormManager* form_manager =
      CreateFormManager({CreateStoredCredential(kTestPassword)});
  EXPECT_CALL(generation_helper(), GetPasswordRequirementsSpec).Times(1);
  EXPECT_CALL(password_store(), UpdateLogin).Times(1);

  PasswordChangeActuator::NewPasswordResult first_password =
      actuator()->GetOrGenerateNewPassword(form_manager);
  PasswordChangeActuator::NewPasswordResult second_password =
      actuator()->GetOrGenerateNewPassword(form_manager);

  ASSERT_TRUE(first_password.has_value());
  EXPECT_FALSE(first_password->empty());
  EXPECT_EQ(first_password, second_password);
}

TEST_F(GlicPasswordChangeActuatorNewPasswordTest,
       UsesCurrentPasswordFieldWithoutNewPasswordField) {
  const autofill::FormData form_data = CreateLoginFormData();
  password_manager::PasswordFormManager* form_manager =
      CreateFormManager(form_data, {CreateStoredCredential(kTestPassword)});
  ASSERT_FALSE(
      form_manager->GetParsedObservedForm()->new_password_element_renderer_id);
  const autofill::FormFieldData& password_field = form_data.fields()[1];
  EXPECT_CALL(
      generation_helper(),
      GetPasswordRequirementsSpec(
          _, _, _, autofill::CalculateFieldSignatureForField(password_field),
          password_field.max_length()));

  PasswordChangeActuator::NewPasswordResult new_password =
      actuator()->GetOrGenerateNewPassword(form_manager);

  ASSERT_TRUE(new_password.has_value());
  EXPECT_FALSE(new_password->empty());
}

TEST_F(GlicPasswordChangeActuatorNewPasswordTest,
       NoPasswordWithoutFormManager) {
  EXPECT_THAT(
      actuator()->GetOrGenerateNewPassword(/*form_manager=*/nullptr),
      base::test::ErrorIs(password_manager::PasswordChangeCredentialsProvider::
                              Error::kGenerationFailed));
}

TEST_F(GlicPasswordChangeActuatorNewPasswordTest,
       NoPasswordWithoutGenerationHelper) {
  password_manager::PasswordFormManager* form_manager =
      CreateFormManager({CreateStoredCredential(kTestPassword)});
  EXPECT_CALL(driver(), GetPasswordGenerationHelper)
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(password_store(), UpdateLogin).Times(0);
  EXPECT_CALL(password_store(), AddLogin).Times(0);

  EXPECT_THAT(
      actuator()->GetOrGenerateNewPassword(form_manager),
      base::test::ErrorIs(password_manager::PasswordChangeCredentialsProvider::
                              Error::kGenerationFailed));
}
