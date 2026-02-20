// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/form_filling_helper.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using autofill::test::CreateTestFormField;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::WithArg;
using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;
using SubmissionError =
    ChangePasswordFormFillingSubmissionHelper::SubmissionError;
using SubmissionResult =
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult;

const std::u16string kUsername = u"user";
const std::u16string kOldPassword = u"qwerty123";
const std::u16string kNewPassword = u"cE1L45Vgxyzlu8";
const char kUrlString[] = "https://www.foo.com/";
const int password_renderer_id = 1;
const int new_password_renderer_id = 2;

class MockChromePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (override, const));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));
};

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

class MockStubPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
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
};

autofill::FormData CreateTestPasswordFormData(
    const std::string& old_password,
    const std::string& new_password,
    int password_id = password_renderer_id,
    int new_password_id = new_password_renderer_id) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/old_password, autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(password_id));
  fields.push_back(CreateTestFormField(
      /*label=*/"New Password:", /*name=*/"new-password",
      /*value=*/new_password, autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(new_password_id));
  autofill::FormData form;
  form.set_url(GURL(kUrlString));
  form.set_fields(std::move(fields));
  return form;
}
autofill::FormData CreateEmptyTestPasswordFormData() {
  return CreateTestPasswordFormData("", "");
}

autofill::FormData CreateFilledTestPasswordFormData() {
  return CreateTestPasswordFormData(base::UTF16ToUTF8(kOldPassword),
                                    base::UTF16ToUTF8(kNewPassword));
}

template <bool success>
void PostResponseForSubmissionButtonClick(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_submit_form_data()->set_dom_node_id_to_click(success ? 1
                                                                        : 0);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

void CheckSubmitFormStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().submit_form().status(),
            expected_status);
}

void PostResponseForUserIntervention(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  auto* data = response.mutable_submit_form_data();
  data->set_is_user_intervention_needed(true);

  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

}  // namespace

class ChangePasswordFormFillingSubmissionHelperTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  ChangePasswordFormFillingSubmissionHelperTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureStates(
        {{password_manager::features::kFillChangePasswordFormByTyping,
          GetParam()}});
  }
  ~ChangePasswordFormFillingSubmissionHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));

    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(testing::Return(password_store_.get()));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(testing::Return(&mock_manager_));
    ON_CALL(mock_manager_, GetPasswordFormCache)
        .WillByDefault(testing::Return(&mock_cache_));
    ON_CALL(driver_, CheckViewAreaVisible)
        .WillByDefault(base::test::RunOnceCallback<1>(true));

    logs_uploader_ =
        std::make_unique<ModelQualityLogsUploader>(web_contents(), GURL());

    existing_credential_.username_value = kUsername;
    existing_credential_.password_value = kOldPassword;
    existing_credential_.url = url();
    existing_credential_.match_type =
        password_manager::PasswordForm::MatchType::kExact;
    existing_credential_.in_store =
        password_manager::PasswordForm::Store::kProfileStore;
    existing_credential_.scheme = password_manager::PasswordForm::Scheme::kHtml;

    ON_CALL(capture_content_for_submit_form_step_, Run)
        .WillByDefault(base::test::RunOnceCallback<0>(
            optimization_guide::AIPageContentResult()));
  }

  void TearDown() override {
    logs_uploader_.reset();
    OSCryptMocker::TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  password_manager::PasswordFormManager* CreateFormManagerFromFormData(
      const autofill::FormData& form_data,
      const std::vector<password_manager::PasswordForm>& credentials_to_seed) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), form_data, &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.SetBestMatches(credentials_to_seed);
    form_fetcher_.SetNonFederated(credentials_to_seed);
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    managers_.push_back(std::move(form_manager));
    EXPECT_CALL(mock_cache_, GetFormManagers)
        .WillRepeatedly(testing::Return(base::span(managers_)));
    return managers_.back().get();
  }

  password_manager::PasswordFormManager* CreateFormManager(
      const std::vector<password_manager::PasswordForm>& credentials_to_seed) {
    return CreateFormManagerFromFormData(CreateEmptyTestPasswordFormData(),
                                         credentials_to_seed);
  }

  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper> CreateVerifier(
      password_manager::PasswordFormManager* manager,
      base::OnceCallback<void(SubmissionResult)> result_callback) {
    auto verifier = std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
        base::PassKey<class ChangePasswordFormFillingSubmissionHelperTest>(),
        web_contents(), client(), logs_uploader_.get(),
        capture_content_for_submit_form_step_.Get(),
        std::move(result_callback));
    return verifier;
  }

  void CompleteFormFilling(password_manager::PasswordFormManager* manager,
                           ChangePasswordFormFillingSubmissionHelper* verifier,
                           std::optional<autofill::FormData> result) {
    if (base::FeatureList::IsEnabled(
            password_manager::features::kFillChangePasswordFormByTyping)) {
      EXPECT_CALL(driver(), FillChangePasswordForm).Times(0);
      verifier->FillChangePasswordForm(manager, kUsername, kOldPassword,
                                       kNewPassword);
      ASSERT_TRUE(verifier->form_filler());
      verifier->form_filler()->SimulateFillingResult(result);
    } else {
      base::RunLoop run_loop;
      EXPECT_CALL(driver(), FillChangePasswordForm)
          .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                          base::test::RunOnceCallback<5>(result)));
      verifier->FillChangePasswordForm(manager, kUsername, kOldPassword,
                                       kNewPassword);
      run_loop.Run();
    }
  }

  void WaitForFillingAndSuccessfulSubmission(
      password_manager::PasswordFormManager* manager,
      ChangePasswordFormFillingSubmissionHelper* verifier) {
    base::RunLoop run_loop;
    EXPECT_CALL(*optimization_service(), ExecuteModel)
        .WillOnce(WithArg<3>(
            [&run_loop](
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              PostResponseForSubmissionButtonClick<true>(
                  std::move(callback).Then(run_loop.QuitClosure()));
            }));
    CompleteFormFilling(manager, verifier, CreateFilledTestPasswordFormData());
    run_loop.Run();
  }

  password_manager::PasswordForm* existing_credential() {
    return &existing_credential_;
  }

  GURL url() const { return GURL(kUrlString); }

  password_manager::PasswordManagerClient* client() { return &client_; }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

  MockStubPasswordManagerDriver& driver() { return driver_; }
  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }
  const std::unique_ptr<ModelQualityLogsUploader>& logs_uploader() const {
    return logs_uploader_;
  }

  password_manager::MockPasswordStoreInterface* profile_password_store() {
    return password_store_.get();
  }

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>*
  capture_content_for_submit_form_step() {
    return &capture_content_for_submit_form_step_;
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList scoped_feature_list_;
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::MockPasswordManager mock_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
  MockStubPasswordManagerDriver driver_;
  password_manager::PasswordForm existing_credential_;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_content_for_submit_form_step_;
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>> managers_;
};

// If the password being changed was stored, we will update it.
TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       SucceededForExistingCredential) {
  base::HistogramTester histogram_tester;
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  task_environment()->AdvanceClock(base::Milliseconds(1534));

  // Presave generated password as backup
  password_manager::PasswordForm presaved_generated_password_form;
  password_manager::PasswordForm saved_generated_password_form;
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(testing::SaveArg<0>(&presaved_generated_password_form));

  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());
  // Simulate submission detected.
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically", true,
      1);
  EXPECT_EQ(presaved_generated_password_form.username_value,
            existing_credential()->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            existing_credential()->password_value);
  EXPECT_EQ(presaved_generated_password_form.url, existing_credential()->url);
  EXPECT_EQ(presaved_generated_password_form.signon_realm,
            existing_credential()->signon_realm);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);

  verifier.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TimeSpentChangingPassword", 1534, 1);
  EXPECT_EQ(1534, logs_uploader()
                      ->GetFinalLog()
                      .password_change_submission()
                      .quality()
                      .submit_form()
                      .request_latency_ms());
}

// If the password being changed was not stored, we will add a new credential.
TEST_P(ChangePasswordFormFillingSubmissionHelperTest, SucceededNewCredential) {
  base::HistogramTester histogram_tester;
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());

  password_manager::PasswordForm presaved_generated_password_form;
  // Presave generated password as backup
  EXPECT_CALL(*profile_password_store(), AddLogin)
      .WillOnce(testing::SaveArg<0>(&presaved_generated_password_form));
  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());

  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically", true,
      1);
  EXPECT_EQ(presaved_generated_password_form.username_value, kUsername);
  EXPECT_EQ(presaved_generated_password_form.password_value, kOldPassword);
  EXPECT_EQ(presaved_generated_password_form.url, url());
  EXPECT_EQ(presaved_generated_password_form.signon_realm, kUrlString);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);
}

// Tests that we do not overwrite the stored password during the presave phase
// if the password used for log in doesn't match the stored password.
TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       PresaveGeneratedPasswordForDifferentInputPassword) {
  password_manager::PasswordForm* stored_form = existing_credential();
  stored_form->password_value = u"stored_password";
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*stored_form});
  auto verifier = CreateVerifier(form_manager, base::DoNothing());

  password_manager::PasswordForm presaved_generated_password_form;
  base::RunLoop run_loop;
  // Presave generated password.
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      testing::SaveArg<0>(&presaved_generated_password_form)));
  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());
  run_loop.Run();

  EXPECT_EQ(presaved_generated_password_form.username_value,
            stored_form->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            stored_form->password_value);
  EXPECT_EQ(presaved_generated_password_form.url, stored_form->url);
  EXPECT_EQ(presaved_generated_password_form.signon_realm,
            stored_form->signon_realm);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       FailsCapturingAnnotatedPageContent) {
  base::HistogramTester histogram_tester;
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});
  base::test::TestFuture<SubmissionResult> completion_future;

  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());

  EXPECT_CALL(*capture_content_for_submit_form_step(), Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          base::unexpected("APC Capture Failed")));

  // Execution isn't triggered because page content capture failed.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());

  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kFailedToCaptureContent);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedCapturingPageContent",
      password_manager::metrics_util::PasswordChangeFlowStep::kSubmitFormStep,
      1);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, FailedDueToTimeout) {
  base::HistogramTester histogram_tester;
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  verifier->FillChangePasswordForm(form_manager, kUsername, kOldPassword,
                                   kNewPassword);

  EXPECT_FALSE(completion_future.IsReady());
  task_environment()->AdvanceClock(
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout);

  EXPECT_EQ(completion_future.Get().error(), SubmissionError::kTimeout);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, SubmissionOnTimeout) {
  base::HistogramTester histogram_tester;
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  password_manager::PasswordForm presaved_generated_password_form;
  // Presave generated password as backup
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(testing::SaveArg<0>(&presaved_generated_password_form));
  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());

  EXPECT_FALSE(completion_future.IsReady());
  task_environment()->AdvanceClock(
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout);

  EXPECT_TRUE(completion_future.Wait());
  EXPECT_TRUE(completion_future.Take().has_value());

  EXPECT_EQ(presaved_generated_password_form.username_value,
            existing_credential()->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            existing_credential()->password_value);
  EXPECT_EQ(presaved_generated_password_form.url, existing_credential()->url);
  EXPECT_EQ(presaved_generated_password_form.signon_realm,
            existing_credential()->signon_realm);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically", false,
      1);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, FailedFilling) {
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto time = base::Time::Now();
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  task_environment()->FastForwardBy(base::Milliseconds(1534));

  // Expect a call to FillChangePasswordForm, although don't invoke completion
  // callback.
  password_manager::PasswordForm presaved_generated_password_form;
  // Presave generated password as backup
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(testing::SaveArg<0>(&presaved_generated_password_form));
  // Password change isn't verified.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);

  CompleteFormFilling(form_manager, verifier.get(), std::nullopt);

  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kFailedToFillForm);
  EXPECT_EQ(presaved_generated_password_form.username_value,
            existing_credential()->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            existing_credential()->password_value);
  EXPECT_EQ(presaved_generated_password_form.url, existing_credential()->url);
  EXPECT_EQ(presaved_generated_password_form.signon_realm,
            existing_credential()->signon_realm);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);

  verifier.reset();
  EXPECT_EQ((base::Time::Now() - time).InMilliseconds(),
            logs_uploader()
                ->GetFinalLog()
                .password_change_submission()
                .quality()
                .submit_form()
                .request_latency_ms());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, ProvisionallySaveFailed) {
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  auto verifier = CreateVerifier(form_manager, base::DoNothing());

  EXPECT_CALL(*capture_content_for_submit_form_step(), Run).Times(0);
  // Expect a call to FillChangePasswordForm, although the returned form is
  // empty.
  CompleteFormFilling(form_manager, verifier.get(),
                      CreateEmptyTestPasswordFormData());

  EXPECT_TRUE(verifier->form_waiter());

  auto* new_form_manager = CreateFormManagerFromFormData(
      CreateTestPasswordFormData("", "", 101, 102), /*credentials_to_seed=*/{});

  // Verify that Chrome attempts to fill and submit a newly found form.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    EXPECT_CALL(driver(), FillChangePasswordForm)
        .WillOnce(
            base::test::RunOnceCallback<5>(CreateFilledTestPasswordFormData()));
  }
  EXPECT_CALL(*capture_content_for_submit_form_step(), Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      verifier->form_waiter())
      ->OnPasswordFormParsed(new_form_manager);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    ASSERT_TRUE(verifier->form_filler());
    verifier->form_filler()->SimulateFillingResult(
        CreateFilledTestPasswordFormData());
  }
  task_environment()->RunUntilIdle();
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       SubmissionBeforeFillingIsDoneIgnored_FillingWithDriver) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    return;
  }
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  base::RunLoop run_loop;
  base::OnceCallback<void(const std::optional<autofill::FormData>&)> callback;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      MoveArg<5>(&callback)));
  verifier->FillChangePasswordForm(form_manager, kUsername, kOldPassword,
                                   kNewPassword);
  run_loop.Run();

  // Verify that `ExecuteModel` isn't called.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  verifier->OnPasswordFormSubmission(web_contents());
  EXPECT_FALSE(completion_future.IsReady());
  testing::Mock::VerifyAndClearExpectations(optimization_service());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponseForSubmissionButtonClick<true>));
  std::move(callback).Run(CreateFilledTestPasswordFormData());

  // Submission detected after filling.
  verifier->OnPasswordFormSubmission(web_contents());
  EXPECT_TRUE(completion_future.Get().has_value());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       SubmissionBeforeFillingIsDoneIgnored_FillingByTyping) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    return;
  }
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());

  verifier->FillChangePasswordForm(form_manager, kUsername, kOldPassword,
                                   kNewPassword);
  ASSERT_TRUE(verifier->form_filler());

  // Verify that `ExecuteModel` isn't called.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  verifier->OnPasswordFormSubmission(web_contents());
  testing::Mock::VerifyAndClearExpectations(optimization_service());

  // Now the filling is complete.
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponseForSubmissionButtonClick<true>));
  verifier->form_filler()->SimulateFillingResult(
      CreateFilledTestPasswordFormData());

  // Submission detected after filling.
  verifier->OnPasswordFormSubmission(web_contents());
  EXPECT_TRUE(completion_future.Get().has_value());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       MultipleSubmissionsAreIgnored) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());

  verifier->OnPasswordFormSubmission(web_contents());
  verifier->OnPasswordFormSubmission(web_contents());
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get().has_value());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       ClickingSubmitButtonWorks) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());

  // Sets up clicking on the Submit button using MES to find the button.
  // Expects MES to be called for checking if the submission was successful.
  verifier->click_helper()->SimulateClickResult(true);

  // Simulates successful form submission detection.
  verifier->OnPasswordFormSubmission(web_contents());

  // Expects that form submission succeeded.
  EXPECT_TRUE(completion_future.Get().has_value());

  CheckSubmitFormStatus(
      logs_uploader()->GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, SubmitButtonNotFound) {
  base::test::ScopedFeatureList feature_list;
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponseForSubmissionButtonClick<false>));

  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());

  EXPECT_FALSE(verifier->click_helper());

  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kSubmitButtonNotFound);

  CheckSubmitFormStatus(
      logs_uploader()->GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest, SubmitButtonClickFailed) {
  base::test::ScopedFeatureList feature_list;
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  WaitForFillingAndSuccessfulSubmission(form_manager, verifier.get());

  verifier->click_helper()->SimulateClickResult(false);

  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kFailedToClickSubmit);

  CheckSubmitFormStatus(
      logs_uploader()->GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       WhenFormFillingFailedHelpersLooksForNewForm) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  EXPECT_CALL(*capture_content_for_submit_form_step(), Run).Times(0);
  CompleteFormFilling(form_manager, verifier.get(), std::nullopt);

  CheckSubmitFormStatus(
      logs_uploader()->GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);

  EXPECT_TRUE(verifier->form_waiter());
  auto* new_form_manager = CreateFormManagerFromFormData(
      CreateTestPasswordFormData("", "", 101, 102), /*credentials_to_seed=*/{});

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    EXPECT_CALL(driver(), FillChangePasswordForm)
        .WillOnce(
            base::test::RunOnceCallback<5>(CreateFilledTestPasswordFormData()));
  }
  EXPECT_CALL(*capture_content_for_submit_form_step(), Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      verifier->form_waiter())
      ->OnPasswordFormParsed(new_form_manager);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    ASSERT_TRUE(verifier->form_filler());
    verifier->form_filler()->SimulateFillingResult(
        CreateFilledTestPasswordFormData());
  }
  task_environment()->RunUntilIdle();
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       ReturnsUserInterventionNeeded_UserInterventionEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kUserInterventionForPasswordChange);

  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponseForUserIntervention));
  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());

  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kInterventionDetected);
  EXPECT_FALSE(verifier->click_helper());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       IgnoresUserIntervention_UserInterventionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kUserInterventionForPasswordChange);
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponseForUserIntervention));

  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());
  EXPECT_EQ(completion_future.Get().error(),
            SubmissionError::kSubmitButtonNotFound);
  EXPECT_FALSE(verifier->click_helper());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       WhenFormFillingFailedItIgnoresTheSameForm) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  // Mock that filling fails.
  CompleteFormFilling(form_manager, verifier.get(), std::nullopt);

  // A form waiter should be created.
  ASSERT_TRUE(verifier->form_waiter());

  // If the same form is parsed again, it should be ignored.
  // No new filling attempt should be made.
  EXPECT_CALL(driver(), FillChangePasswordForm).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(
      verifier->form_waiter())
      ->OnPasswordFormParsed(form_manager);

  // To ensure no async tasks are pending that would call
  // FillChangePasswordForm.
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&driver());
}

TEST_P(ChangePasswordFormFillingSubmissionHelperTest,
       PasswordChangeFormInfoIsLogged) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<SubmissionResult> completion_future;
  auto verifier = CreateVerifier(form_manager, completion_future.GetCallback());
  CompleteFormFilling(form_manager, verifier.get(),
                      CreateFilledTestPasswordFormData());

  optimization_guide::proto::PasswordChangeQuality quality =
      logs_uploader()->GetFinalLog().password_change_submission().quality();
  EXPECT_TRUE(quality.has_change_password_form_data());
}

INSTANTIATE_TEST_SUITE_P(,
                         ChangePasswordFormFillingSubmissionHelperTest,
                         testing::Bool());
