// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
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
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;

const std::u16string kOldPassword = u"qwerty123";
const std::u16string kNewPassword = u"cE1L45Vgxyzlu8";
const int password_renderer_id = 1;
const int new_password_renderer_id = 2;

class FakeChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static FakeChromePasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* contents) {
    auto* client = new FakeChromePasswordManagerClient(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
    return client;
  }

  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver*) override {
    return nullptr;
  }

 private:
  explicit FakeChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {}
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
              SubmitFormWithEnter,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
};

autofill::FormData CreateTestPasswordFormData() {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(
      autofill::FieldRendererId(password_renderer_id));
  fields.push_back(CreateTestFormField(
      /*label=*/"New Password:", /*name=*/"new-password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(
      autofill::FieldRendererId(new_password_renderer_id));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  return form;
}

template <bool success>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      success
          ? PasswordChangeOutcome::
                PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME
          : PasswordChangeOutcome::
                PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

}  // namespace

class ChangePasswordFormFillingSubmissionHelperTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChangePasswordFormFillingSubmissionHelperTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ChangePasswordFormFillingSubmissionHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));

    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
    // `ChromePasswordManagerClient` observes `AutofillManager`s, so
    // `ChromeAutofillClient` needs to be set up, too.
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
    FakeChromePasswordManagerClient::CreateForWebContentsAndGet(web_contents());
    logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(web_contents());
  }

  void TearDown() override {
    logs_uploader_.reset();
    OSCryptMocker::TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager() {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), CreateTestPasswordFormData(),
        &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    return form_manager;
  }

  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper> CreateVerifier(
      password_manager::PasswordFormManager* manager,
      base::OnceCallback<void(bool)> result_callback) {
    auto verifier = std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
        web_contents(), logs_uploader_.get(), std::move(result_callback));
    verifier->FillChangePasswordForm(manager, kOldPassword, kNewPassword);
    return verifier;
  }

  ChromePasswordManagerClient* client() {
    return ChromePasswordManagerClient::FromWebContents(web_contents());
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

  MockStubPasswordManagerDriver& driver() { return driver_; }
  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  password_manager::FakeFormFetcher form_fetcher_;
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
  MockStubPasswordManagerDriver driver_;
};

TEST_F(ChangePasswordFormFillingSubmissionHelperTest, Succeeded) {
  base::HistogramTester histogram_tester;
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  base::RunLoop run_loop;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(RunOnceCallback<5>(CreateTestPasswordFormData()));
  EXPECT_CALL(driver(), SubmitFormWithEnter)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      RunOnceCallback<1>(/*success=*/true)));
  run_loop.Run();

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  ASSERT_TRUE(verifier->submission_verifier());
  verifier->submission_verifier()->set_annotated_page_callback(
      capture_annotated_page_content.Get());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get());
  histogram_tester.ExpectTotalCount(
      PasswordChangeSubmissionVerifier::
          kPasswordChangeVerificationTimeHistogram,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically", true,
      1);
}

TEST_F(ChangePasswordFormFillingSubmissionHelperTest, Failed) {
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  base::RunLoop run_loop;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(RunOnceCallback<5>(CreateTestPasswordFormData()));
  EXPECT_CALL(driver(), SubmitFormWithEnter)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      RunOnceCallback<1>(/*success=*/true)));
  run_loop.Run();

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  ASSERT_TRUE(verifier->submission_verifier());
  verifier->submission_verifier()->set_annotated_page_callback(
      capture_annotated_page_content.Get());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<false>)));
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_FALSE(completion_future.Get());
}

TEST_F(ChangePasswordFormFillingSubmissionHelperTest, OnTimeout) {
  base::HistogramTester histogram_tester;
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  base::RunLoop run_loop;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(RunOnceCallback<5>(CreateTestPasswordFormData()));
  EXPECT_CALL(driver(), SubmitFormWithEnter)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      RunOnceCallback<1>(/*success=*/true)));
  run_loop.Run();

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ASSERT_TRUE(verifier->submission_verifier());
  verifier->submission_verifier()->set_annotated_page_callback(
      capture_annotated_page_content.Get());

  // Verify submission isn't verified for `kSubmissionWaitingTimeout` seconds.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  task_environment()->AdvanceClock(
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout);
  testing::Mock::VerifyAndClearExpectations(optimization_service());

  // Now verification should be triggered on timeout.
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));

  EXPECT_TRUE(completion_future.Wait());
  EXPECT_TRUE(completion_future.Take());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically", false,
      1);
}

TEST_F(ChangePasswordFormFillingSubmissionHelperTest, FailedFilling) {
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  // Expect a call to FillChangePasswordForm, although don't invoke completion
  // callback.
  EXPECT_CALL(driver(), FillChangePasswordForm).Times(1);
  // Password change isn't verified.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);

  task_environment()->AdvanceClock(
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout);

  EXPECT_FALSE(completion_future.Get());
}

TEST_F(ChangePasswordFormFillingSubmissionHelperTest,
       SubmissionBeforeFillingIsDoneIgnored) {
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  base::RunLoop run_loop;
  base::OnceCallback<void(const std::optional<autofill::FormData>&)> callback;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      MoveArg<5>(&callback)));
  run_loop.Run();

  // Verify that `ExecuteModel` isn't called.
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  verifier->OnPasswordFormSubmission(web_contents());
  testing::Mock::VerifyAndClearExpectations(optimization_service());

  EXPECT_CALL(driver(), SubmitFormWithEnter)
      .WillOnce(RunOnceCallback<1>(/*success=*/true));
  std::move(callback).Run(CreateTestPasswordFormData());

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  ASSERT_TRUE(verifier->submission_verifier());
  verifier->submission_verifier()->set_annotated_page_callback(
      capture_annotated_page_content.Get());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get());
}

TEST_F(ChangePasswordFormFillingSubmissionHelperTest,
       MultipleSubmissionsAreIgnored) {
  auto form_manager = CreateFormManager();

  base::test::TestFuture<bool> completion_future;
  auto verifier =
      CreateVerifier(form_manager.get(), completion_future.GetCallback());

  base::RunLoop run_loop;
  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(RunOnceCallback<5>(CreateTestPasswordFormData()));
  EXPECT_CALL(driver(), SubmitFormWithEnter)
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      RunOnceCallback<1>(/*success=*/true)));
  run_loop.Run();

  // Verify that `ExecuteModel` is called once.
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  ASSERT_TRUE(verifier->submission_verifier());
  verifier->submission_verifier()->set_annotated_page_callback(
      capture_annotated_page_content.Get());
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .Times(1)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  verifier->OnPasswordFormSubmission(web_contents());
  verifier->OnPasswordFormSubmission(web_contents());
  verifier->OnPasswordFormSubmission(web_contents());

  EXPECT_TRUE(completion_future.Get());
}
