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
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
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

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {
    ON_CALL(*this, IsInPrimaryMainFrame).WillByDefault(Return(true));
  }
  MOCK_METHOD(
      void,
      FillChangePasswordForm,
      (autofill::FieldRendererId,
       autofill::FieldRendererId,
       autofill::FieldRendererId,
       const std::u16string&,
       const std::u16string&,
       base::OnceCallback<void(const std::optional<autofill::FormData>&)>),
      (override));
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));

  void SetPasswordGenerationHelper(
      password_manager::PasswordGenerationFrameHelper* helper) {
    generation_helper_ = helper;
  }
  password_manager::PasswordGenerationFrameHelper* GetPasswordGenerationHelper()
      override {
    return generation_helper_;
  }

 private:
  raw_ptr<password_manager::PasswordGenerationFrameHelper> generation_helper_ =
      nullptr;
};

class MockTestPasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static MockTestPasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* contents) {
    auto* client = new NiceMock<MockTestPasswordManagerClient>(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
    return client;
  }

  explicit MockTestPasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {}

  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (override, const));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));
  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver* driver) override {
    return nullptr;
  }
};

password_manager::StoredCredential CreateTestCredential() {
  GURL url(kTestUrl);
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = url::Origin::Create(url).GetURL().spec();
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  return password_manager::FromPasswordForm(std::move(form));
}

autofill::FormData CreateChangePasswordFormData(
    const std::u16string& old_password = u"",
    const std::u16string& new_password = u"") {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(autofill::test::CreateTestFormField(
      "Current password:", "password", base::UTF16ToUTF8(old_password),
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.back().set_is_enabled(true);

  fields.push_back(autofill::test::CreateTestFormField(
      "New password:", "new_password_1", base::UTF16ToUTF8(new_password),
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.back().set_is_enabled(true);

  fields.push_back(autofill::test::CreateTestFormField(
      "Confirm password:", "new_password_2", base::UTF16ToUTF8(new_password),
      autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  fields.back().set_is_enabled(true);

  autofill::FormData form;
  form.set_url(GURL(kTestUrl));
  form.set_fields(std::move(fields));
  return form;
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
    };
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
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

    web_contents()->SetDelegate(&mock_web_contents_delegate_);
    ON_CALL(mock_web_contents_delegate_, OpenURLFromTab(web_contents(), _, _))
        .WillByDefault(Return(actuation_web_contents_.get()));

    mock_pwm_client_ =
        MockTestPasswordManagerClient::CreateForWebContentsAndGet(
            actuation_web_contents_.get());
    password_generation_helper_ =
        std::make_unique<password_manager::PasswordGenerationFrameHelper>(
            mock_pwm_client_, &driver_);
    driver_.SetPasswordGenerationHelper(password_generation_helper_.get());
    password_store_ =
        base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
    ON_CALL(*mock_pwm_client_, GetProfilePasswordStore)
        .WillByDefault(Return(password_store_.get()));
    ON_CALL(*mock_pwm_client_, GetPasswordManager)
        .WillByDefault(Return(&mock_pwm_manager_));
    ON_CALL(mock_pwm_manager_, GetPasswordFormCache)
        .WillByDefault(Return(&mock_cache_));
    ON_CALL(driver_, CheckViewAreaVisible)
        .WillByDefault(base::test::RunOnceCallback<1>(true));

    actuator_ = std::make_unique<GlicPasswordChangeActuator>(
        CreateTestCredential(), web_contents(), profile());
    actuator_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    actuator_->RemoveObserver(&mock_observer_);
    actuator_.reset();
    form_managers_.clear();
    driver_.SetPasswordGenerationHelper(nullptr);
    password_generation_helper_.reset();
    mock_pwm_client_ = nullptr;
    mock_glic_service_ = nullptr;
    actuation_web_contents_.reset();
    web_contents()->SetDelegate(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
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

  void SetupFormInCache() {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        mock_pwm_client_, driver_.AsWeakPtr(), CreateChangePasswordFormData(),
        &form_fetcher_,
        std::make_unique<password_manager::PasswordSaveManagerImpl>(
            mock_pwm_client_),
        /*metrics_recorder=*/nullptr);

    std::vector<password_manager::PasswordForm> seed_credentials;
    password_manager::PasswordForm form;
    form.url = GURL(kTestUrl);
    form.signon_realm = url::Origin::Create(form.url).GetURL().spec();
    form.username_value = kTestUsername;
    form.password_value = kTestPassword;
    form.in_store = password_manager::PasswordForm::Store::kProfileStore;
    form.match_type = password_manager::PasswordForm::MatchType::kExact;
    seed_credentials.push_back(form);

    form_fetcher_.SetBestMatches(seed_credentials);
    form_fetcher_.SetNonFederated(seed_credentials);
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();

    form_managers_.push_back(std::move(form_manager));
    ON_CALL(mock_cache_, GetFormManagers)
        .WillByDefault(Return(base::span(form_managers_)));
  }

  GlicPasswordChangeActuator* actuator() { return actuator_.get(); }
  NiceMock<MockPasswordChangeActuatorObserver>& observer() {
    return mock_observer_;
  }
  MockPasswordManagerDriver& driver() { return driver_; }
  password_manager::MockPasswordStoreInterface* password_store() {
    return password_store_.get();
  }
  tabs::MockTabInterface& mock_actuation_tab() { return mock_actuation_tab_; }
  NiceMock<glic::MockGlicKeyedService>* mock_glic_service() {
    return mock_glic_service_;
  }

 private:
  base::ScopedTempDir temp_dir_;
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  glic::GlicProfileManager glic_profile_manager_;
  raw_ptr<NiceMock<glic::MockGlicKeyedService>> mock_glic_service_ = nullptr;
  ui::UnownedUserDataHost unowned_user_data_host_;
  tabs::MockTabInterface mock_originator_tab_;
  tabs::MockTabInterface mock_actuation_tab_;
  MockWebContentsDelegate mock_web_contents_delegate_;
  std::unique_ptr<content::WebContents> actuation_web_contents_;
  NiceMock<MockPasswordChangeActuatorObserver> mock_observer_;
  raw_ptr<MockTestPasswordManagerClient> mock_pwm_client_ = nullptr;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_;
  password_manager::MockPasswordManager mock_pwm_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  password_manager::FakeFormFetcher form_fetcher_;
  MockPasswordManagerDriver driver_;
  std::unique_ptr<password_manager::PasswordGenerationFrameHelper>
      password_generation_helper_;
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers_;
  std::unique_ptr<GlicPasswordChangeActuator> actuator_;
};

// 1. VerificationSuccessSavesPasswordAndNotifiesObserver:
// Simulates the full flow including form finding, form filling, and
// verification OnUpdate with kTerminalCompletion /
// PASSWORD_CHANGE_FINISHED_SUCCESSFULLY, verifying state transitions to
// kPasswordSuccessfullyChanged and password store saving.
TEST_F(GlicPasswordChangeActuatorTest,
       VerificationSuccessSavesPasswordAndNotifiesObserver) {
  SetupFormInCache();

  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));

  // Start the actuator.
  actuator()->Start();

  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kChangingPassword));
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(
          [](autofill::FieldRendererId old_password_field_id,
             autofill::FieldRendererId new_password_field_id,
             autofill::FieldRendererId confirm_password_field_id,
             const std::u16string& old_password,
             const std::u16string& new_password,
             base::OnceCallback<void(const std::optional<autofill::FormData>&)>
                 callback) {
            std::move(callback).Run(
                CreateChangePasswordFormData(old_password, new_password));
          });

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_, testing::Matcher<glic::GlicInvokeOptions>(_),
                  testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .WillOnce([&run_loop](auto, auto, auto) {
        run_loop.Quit();
        return nullptr;
      });

  // Complete find form task via triggering update, which triggers form waiter,
  // filler, and posts InvokeVerificationFlow.
  auto find_form_update = glic::mojom::ExperimentalTriggeringUpdate::New();
  find_form_update->type =
      glic::mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  find_form_update->data = "CHANGE_PASSWORD_FORM_FOUND on page.";
  actuator()->OnUpdate(std::move(find_form_update),
                       glic::mojom::SubscriberObservationType::kUpdate);

  run_loop.Run();

  // Deliver successful verification update via public OnUpdate.
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

// 2. VerificationInterruptionPausedNotifiesOtpDetected:
// Simulates OnUpdate with kPaused during actuation, verifying kOtpDetected.
TEST_F(GlicPasswordChangeActuatorTest,
       VerificationInterruptionPausedNotifiesOtpDetected) {
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

// 3. VerificationTerminalFailureNotifiesPasswordChangeFailed:
// Simulates OnUpdate with kTerminalFailed and FAILED_TO_CHANGE_PASSWORD,
// verifying observer is notified with kPasswordChangeFailed.
TEST_F(GlicPasswordChangeActuatorTest,
       VerificationTerminalFailureNotifiesPasswordChangeFailed) {
  SetupFormInCache();

  EXPECT_CALL(
      observer(),
      OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm));

  actuator()->Start();

  EXPECT_CALL(observer(),
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kChangingPassword));
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(
          [](autofill::FieldRendererId old_password_field_id,
             autofill::FieldRendererId new_password_field_id,
             autofill::FieldRendererId confirm_password_field_id,
             const std::u16string& old_password,
             const std::u16string& new_password,
             base::OnceCallback<void(const std::optional<autofill::FormData>&)>
                 callback) {
            std::move(callback).Run(
                CreateChangePasswordFormData(old_password, new_password));
          });

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_, testing::Matcher<glic::GlicInvokeOptions>(_),
                  testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .WillOnce([&run_loop](auto, auto, auto) {
        run_loop.Quit();
        return nullptr;
      });

  auto find_form_update = glic::mojom::ExperimentalTriggeringUpdate::New();
  find_form_update->type =
      glic::mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  find_form_update->data = "CHANGE_PASSWORD_FORM_FOUND on page.";
  actuator()->OnUpdate(std::move(find_form_update),
                       glic::mojom::SubscriberObservationType::kUpdate);

  run_loop.Run();

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
