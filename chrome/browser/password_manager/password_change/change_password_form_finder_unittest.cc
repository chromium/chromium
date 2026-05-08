// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/fake_annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_prediction_waiter.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using autofill::test::CreateTestFormField;
using testing::Return;
using testing::WithArg;

using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;

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

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
};

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

template <bool success>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_dom_node_id_to_click(success ? 1 : 0);
  response.mutable_open_form_data()->set_page_type(
      ::optimization_guide::proto::OpenFormResponseData_PageType::
          OpenFormResponseData_PageType_SETTINGS_PAGE);

  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
}

void CheckOpenFormStatus(const optimization_guide::proto::LogAiDataRequest& log,
                         const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().open_form().status(),
            expected_status);
}

autofill::FormFieldData CreateNonFocusableTestFormField(
    std::string label,
    std::string name,
    std::string value,
    autofill::FormControlType type) {
  auto field = CreateTestFormField(label, name, value, type);
  field.set_is_focusable(false);
  return field;
}

autofill::FormData CreateFormData() {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form_data;
  form_data.set_url(GURL("https://www.foo.com"));
  form_data.set_fields(std::move(fields));

  return form_data;
}

autofill::FormData CreateHiddenFormData() {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form_data;
  form_data.set_url(GURL("https://www.foo.com"));
  form_data.set_fields(std::move(fields));

  return form_data;
}

}  // namespace

class ChangePasswordFormFinderTest : public ChromeRenderViewHostTestHarness,
                                     public testing::WithParamInterface<bool> {
 public:
  ChangePasswordFormFinderTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatureStates(
        {{password_manager::features::kAwaitPageStabilityForPasswordChange,
          GetParam()}});
  }
  ~ChangePasswordFormFinderTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));

    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(testing::Return(password_store_.get()));
    ON_CALL(client_, GetPasswordManager).WillByDefault(Return(&mock_manager_));
    ON_CALL(mock_manager_, GetPasswordFormCache)
        .WillByDefault(Return(&mock_cache_));
    ON_CALL(driver_, CheckViewAreaVisible)
        .WillByDefault(base::test::RunOnceCallback<1>(true));

    AnnotatedPageContentCapturer::SetFactoryForTesting(base::BindRepeating(
        [](content::WebContents* web_contents,
           blink::mojom::AIPageContentOptionsPtr options,
           optimization_guide::OnAIPageContentDone callback)
            -> std::unique_ptr<AnnotatedPageContentCapturer> {
          return std::make_unique<FakeAnnotatedPageContentCapturer>(
              std::move(callback));
        }));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  password_manager::PasswordFormManager* CreateFormManager(
      autofill::FormData form_data = CreateFormData()) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), form_data, &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    managers_.push_back(std::move(form_manager));
    EXPECT_CALL(mock_cache_, GetFormManagers)
        .WillRepeatedly(testing::Return(base::span(managers_)));
    return managers_.back().get();
  }

  password_manager::PasswordManagerClient* client() { return &client_; }

  MockPasswordManagerDriver& driver() { return driver_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }


  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList feature_list_;
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::MockPasswordManager mock_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  MockPasswordManagerDriver driver_;
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>> managers_;
};

TEST_P(ChangePasswordFormFinderTest, PasswordChangeFormFound) {
  base::HistogramTester histogram_tester;
  auto* form_manager = CreateFormManager();
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ChangePasswordFormFinder form_finder(web_contents(), client(), &logs_uploader,
                                       completion_callback.Get(),
                                       base::DoNothing());

  if (!form_finder.form_waiter()) {
    form_finder.TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder.form_waiter());
  EXPECT_CALL(completion_callback, Run(form_manager));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder.form_waiter())
      ->OnPasswordFormParsed(form_manager);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ChangePasswordFormDetected", true, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ChangePasswordFormDetectionTime", 1);
}

TEST_P(ChangePasswordFormFinderTest, ChangePasswordFormNotDetected) {
  base::HistogramTester histogram_tester;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void(ChangePasswordFormFinder::ErrorCase)>
      error_callback;
  ChangePasswordFormFinder form_finder(web_contents(), client(), &logs_uploader,
                                       completion_callback.Get(),
                                       error_callback.Get());

  if (!form_finder.form_waiter()) {
    form_finder.TriggerPageStabilityForTesting();
  }

  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_CALL(error_callback,
              Run(ChangePasswordFormFinder::ErrorCase::kFormNotFound));

  task_environment()->FastForwardBy(
      ChangePasswordFormFinder::kFormWaitingTimeout);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ChangePasswordFormDetected", false, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ChangePasswordFormDetectionTime", 0);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_TIME_OUT);
}

TEST_P(ChangePasswordFormFinderTest,
       InitialFormWaiter_InvisiblePasswordChangeFormIgnored) {
  auto invisible_form = CreateHiddenFormData();
  auto* form_manager = CreateFormManager(invisible_form);
  EXPECT_CALL(driver(), CheckViewAreaVisible)
      .WillOnce(base::test::RunOnceCallback<1>(false));
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ChangePasswordFormFinder form_finder(web_contents(), client(), &logs_uploader,
                                       completion_callback.Get(),
                                       base::DoNothing());

  if (!form_finder.form_waiter()) {
    form_finder.TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder.form_waiter());
  EXPECT_CALL(completion_callback, Run(form_manager)).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder.form_waiter())
      ->OnPasswordFormParsed(form_manager);
}

TEST_P(ChangePasswordFormFinderTest, ExecuteModelModelFailedWhenFormNotFound) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void(ChangePasswordFormFinder::ErrorCase)>
      error_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      error_callback.Get());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  if (GetParam()) {
    EXPECT_FALSE(form_finder->form_waiter());
    task_environment()->FastForwardBy(
        password_manager::features::kAwaitPageStabilityTimeout.Get());
  }

  // Now `form_finder` is waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_CALL(error_callback,
              Run(ChangePasswordFormFinder::ErrorCase::kFormNotFound));
  task_environment()->FastForwardBy(
      ChangePasswordFormFinder::kFormWaitingTimeout);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_NOT_FOUND);
}

TEST_P(ChangePasswordFormFinderTest, ExecuteModelOpenFormRequestHasArgs) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      base::DoNothing());

  GURL test_url("https://example.com/change-password");
  std::u16string test_title = u"Change Your Password";

  content::WebContentsTester::For(web_contents())
      ->SetLastCommittedURL(test_url);
  content::WebContentsTester::For(web_contents())->SetTitle(test_title);

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(
          [test_url, test_title](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& request = static_cast<
                const optimization_guide::proto::PasswordChangeRequest&>(
                request_metadata);

            EXPECT_EQ(request.page_context().url(), test_url.spec());
            EXPECT_EQ(request.page_context().title(),
                      base::UTF16ToUTF8(test_title));

            PostResponse<true>(std::move(callback));
          });
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_P(ChangePasswordFormFinderTest, ButtonClickRequestedButFailed) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void(ChangePasswordFormFinder::ErrorCase)>
      error_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      error_callback.Get());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());

  EXPECT_CALL(error_callback,
              Run(ChangePasswordFormFinder::ErrorCase::kFailedToClickButton));
  EXPECT_CALL(completion_callback, Run).Times(0);
  form_finder->click_helper()->SimulateClickResult(/*result=*/false);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_P(ChangePasswordFormFinderTest, FailsCapturingAnnotatedPageContent) {
  base::HistogramTester histogram_tester;
  base::MockOnceCallback<void(ChangePasswordFormFinder::ErrorCase)>
      error_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, base::DoNothing(),
      error_callback.Get());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(base::unexpected("Failure"));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedCapturingPageContent",
      password_manager::metrics_util::PasswordChangeFlowStep::kOpenFormStep, 1);
}

TEST_P(ChangePasswordFormFinderTest, ButtonClickRequestedAndSucceeded) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      base::DoNothing());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  if (GetParam()) {
    EXPECT_FALSE(form_finder->form_waiter());
    task_environment()->FastForwardBy(
        password_manager::features::kAwaitPageStabilityTimeout.Get());
  }

  // Now `form_finder` is waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());

  auto* form_manager = CreateFormManager();
  EXPECT_CALL(completion_callback, Run(form_manager));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  // Form finder holds a pointer to `form_manager`
  form_finder.reset();
}

TEST_P(ChangePasswordFormFinderTest,
       ButtonClickRequestedAndSucceeded_InvisibleFormNotIgnored) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      base::DoNothing());
  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  if (GetParam()) {
    EXPECT_FALSE(form_finder->form_waiter());
    task_environment()->FastForwardBy(
        password_manager::features::kAwaitPageStabilityTimeout.Get());
  }

  // Now `form_finder` is waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());

  auto invisible_form = CreateHiddenFormData();
  auto* form_manager = CreateFormManager(invisible_form);
  EXPECT_CALL(completion_callback, Run(form_manager));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  // Form finder holds a pointer to `form_manager`
  form_finder.reset();
}

TEST_P(ChangePasswordFormFinderTest,
       ButtonClickRequested_FormFound_ButtonClickSucceeded) {
  base::test::TestFuture<password_manager::PasswordFormManager*>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader,
      completion_callback.GetCallback(), base::DoNothing());
  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  ASSERT_TRUE(form_finder->click_helper());
  EXPECT_FALSE(form_finder->form_waiter());

  // Still waiting for the click helper to respond.
  EXPECT_FALSE(completion_callback.IsReady());
  form_finder->click_helper()->SimulateClickResult(/*result=*/true);

  EXPECT_FALSE(form_finder->click_helper());

  if (GetParam()) {
    EXPECT_FALSE(form_finder->form_waiter());
    task_environment()->FastForwardBy(
        password_manager::features::kAwaitPageStabilityTimeout.Get());
  }

  ASSERT_TRUE(form_finder->form_waiter());
  auto* form_manager = CreateFormManager();
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager);

  EXPECT_TRUE(completion_callback.IsReady());
  EXPECT_EQ(completion_callback.Get(), form_manager);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  // Form finder holds a pointer to `form_manager`
  form_finder.reset();
}

TEST_P(ChangePasswordFormFinderTest, DurationRecordedOnDestruction) {
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, base::DoNothing(),
      base::DoNothing());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  task_environment()->FastForwardBy(base::Milliseconds(1232));

  form_finder.reset();
  EXPECT_EQ(1232, logs_uploader.GetFinalLog()
                      .password_change_submission()
                      .quality()
                      .open_form()
                      .request_latency_ms());
}

TEST_P(ChangePasswordFormFinderTest, FailsWhenPageTypeIsNotSettingsPage) {
  base::test::TestFuture<ChangePasswordFormFinder::ErrorCase>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, base::DoNothing(),
      completion_callback.GetCallback());
  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>([](auto callback) {
        optimization_guide::proto::PasswordChangeResponse response;
        response.mutable_open_form_data()->set_dom_node_id_to_click(1);
        response.mutable_open_form_data()->set_page_type(
            ::optimization_guide::proto::OpenFormResponseData_PageType::
                OpenFormResponseData_PageType_LOG_IN_PAGE);

        auto result = optimization_guide::OptimizationGuideModelExecutionResult(
            optimization_guide::AnyWrapProto(response),
            /*execution_info=*/nullptr);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                      /*log_entry=*/nullptr));
      }));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_EQ(completion_callback.Get(),
            ChangePasswordFormFinder::ErrorCase::kNoButtonToClick);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_P(ChangePasswordFormFinderTest, InterventionNeededPageCausesFailure) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kUserInterventionForPasswordChange);
  base::test::TestFuture<ChangePasswordFormFinder::ErrorCase>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, base::DoNothing(),
      completion_callback.GetCallback());
  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>([](auto callback) {
        optimization_guide::proto::PasswordChangeResponse response;
        response.mutable_open_form_data()->set_page_type(
            ::optimization_guide::proto::OpenFormResponseData_PageType::
                OpenFormResponseData_PageType_USER_INTERVENTION_NEEDED_PAGE);

        auto result = optimization_guide::OptimizationGuideModelExecutionResult(
            optimization_guide::AnyWrapProto(response),
            /*execution_info=*/nullptr);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                      /*log_entry=*/nullptr));
      }));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());
  EXPECT_EQ(completion_callback.Get(),
            ChangePasswordFormFinder::ErrorCase::kInterruptionDetected);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_USER_INTERVENTION_NEEDED);
}

TEST_P(ChangePasswordFormFinderTest, PageStabilityAfterClick_WaiterNotCreatedUntilStable) {
  // This test only makes sense when the feature is enabled.
  if (!GetParam()) {
    return;
  }

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      web_contents(), client(), &logs_uploader, completion_callback.Get(),
      base::DoNothing());

  if (!form_finder->form_waiter()) {
    form_finder->TriggerPageStabilityForTesting();
  }

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DidStopLoading();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(&PostResponse<true>));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  ASSERT_TRUE(form_finder->capturer());
  static_cast<FakeAnnotatedPageContentCapturer*>(form_finder->capturer())
      ->SimulateResponse(optimization_guide::AIPageContentResult());

  EXPECT_TRUE(form_finder->click_helper());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  // Verify that form_waiter is NOT created yet because we are waiting for stability.
  EXPECT_FALSE(form_finder->form_waiter());

  // Fast forward to trigger the timeout of the stability waiter.
  task_environment()->FastForwardBy(
      password_manager::features::kAwaitPageStabilityTimeout.Get());

  // Now `form_finder` should be waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());

  auto* form_manager = CreateFormManager();
  EXPECT_CALL(completion_callback, Run(form_manager));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager);
}

INSTANTIATE_TEST_SUITE_P(All, ChangePasswordFormFinderTest, testing::Bool());
