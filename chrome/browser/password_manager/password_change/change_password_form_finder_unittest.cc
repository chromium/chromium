// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
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
using testing::Invoke;
using testing::Return;
using testing::WithArg;

using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;

const char kUrlString[] = "https://www.foo.com/";

class MockChromePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (override, const));
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

void CheckOpenFormStatus(const optimization_guide::proto::LogAiDataRequest& log,
                         const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().open_form().status(),
            expected_status);
}

}  // namespace

class ChangePasswordFormFinderTest : public ChromeRenderViewHostTestHarness {
 public:
  ChangePasswordFormFinderTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ChangePasswordFormFinderTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));

    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(testing::Return(password_store_.get()));
  }

  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager(
      const autofill::FormData& form_data) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), form_data, &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    return form_manager;
  }

  std::unique_ptr<password_manager::PasswordFormManager>
  CreateChangePasswordFormManager() {
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

    return CreateFormManager(form_data);
  }

  std::unique_ptr<password_manager::PasswordFormManager>
  CreateLoginFormManager() {
    std::vector<autofill::FormFieldData> fields;
    fields.push_back(CreateTestFormField(
        /*label=*/"Username:", /*name=*/"username",
        /*value=*/"", autofill::FormControlType::kInputEmail));
    fields.push_back(CreateTestFormField(
        /*label=*/"Password:", /*name=*/"password",
        /*value=*/"", autofill::FormControlType::kInputPassword));
    autofill::FormData form_data;
    form_data.set_url(GURL("https://www.foo.com"));
    form_data.set_fields(std::move(fields));

    return CreateFormManager(form_data);
  }

  password_manager::PasswordManagerClient* client() { return &client_; }

  password_manager::StubPasswordManagerDriver& driver() { return driver_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }

  base::PassKey<class ChangePasswordFormFinderTest> pass_key() {
    return base::PassKey<class ChangePasswordFormFinderTest>();
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::StubPasswordManagerDriver driver_;
};

TEST_F(ChangePasswordFormFinderTest, PasswordChangeFormFound) {
  auto form_manager = CreateChangePasswordFormManager();
  ModelQualityLogsUploader logs_uploader(web_contents());
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ChangePasswordFormFinder form_finder(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());

  ASSERT_TRUE(form_finder.form_waiter());
  EXPECT_CALL(capture_annotated_page_content, Run).Times(0);
  EXPECT_CALL(login_form_found_callback, Run).Times(0);
  EXPECT_CALL(completion_callback, Run(form_manager.get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder.form_waiter())
      ->OnPasswordFormParsed(form_manager.get());
}

TEST_F(ChangePasswordFormFinderTest, ExecuteModelModelFailedWhenFormNotFound) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();

  // Simulate ExecuteModel responds with failure.
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<false>)));

  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  EXPECT_CALL(login_form_found_callback, Run).Times(0);
  EXPECT_CALL(completion_callback, Run(nullptr));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ChangePasswordFormFinderTest, ExecuteModelOpenFormRequestHasArgs) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());

  GURL test_url("https://example.com/change-password");
  std::u16string test_title = u"Change Your Password";

  content::WebContentsTester::For(web_contents())
      ->SetLastCommittedURL(test_url);
  content::WebContentsTester::For(web_contents())->SetTitle(test_title);

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(
          [test_url, test_title](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              std::optional<base::TimeDelta> execution_timeout,
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

  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ChangePasswordFormFinderTest, ButtonClickRequestedButFailed) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  EXPECT_CALL(login_form_found_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());
  EXPECT_FALSE(form_finder->form_waiter());

  EXPECT_CALL(completion_callback, Run(nullptr));
  form_finder->click_helper()->SimulateClickResult(/*result=*/false);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ChangePasswordFormFinderTest, FailsCapturingAnnotatedPageContent) {
  base::HistogramTester histogram_tester;
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  ModelQualityLogsUploader logs_uploader(web_contents());
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(std::nullopt));
  EXPECT_CALL(login_form_found_callback, Run).Times(0);

  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());
  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();

  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedCapturingPageContent",
      password_manager::metrics_util::PasswordChangeFlowStep::kOpenFormStep, 1);
}

TEST_F(ChangePasswordFormFinderTest, ButtonClickRequestedAndSucceeded) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());
  EXPECT_CALL(login_form_found_callback, Run).Times(0);

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());
  EXPECT_FALSE(form_finder->form_waiter());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  // Now `form_finder` is waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());

  auto form_manager = CreateChangePasswordFormManager();
  EXPECT_CALL(completion_callback, Run(form_manager.get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager.get());

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ChangePasswordFormFinderTest, ExecuteModelPredictsLoginPage) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ModelQualityLogsUploader logs_uploader(web_contents());
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());

  ASSERT_TRUE(form_finder->form_waiter());
  static_cast<content::WebContentsObserver*>(form_finder->form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_FALSE(form_finder->click_helper());

  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::PasswordChangeResponse response;
            response.mutable_open_form_data()->set_dom_node_id_to_click(0);
            response.mutable_open_form_data()->set_page_type(
                ::optimization_guide::proto::OpenFormResponseData_PageType::
                    OpenFormResponseData_PageType_LOG_IN_PAGE);

            auto result =
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::AnyWrapProto(response),
                    /*execution_info=*/nullptr);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(result),
                               /*log_entry=*/nullptr));
          })));
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  EXPECT_EQ(GURL(kUrlString), web_contents()->GetURL());
  EXPECT_TRUE(form_finder->form_waiter());

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_F(ChangePasswordFormFinderTest, PasswordLoginFormFound) {
  ModelQualityLogsUploader logs_uploader(web_contents());
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceCallback<void()> login_form_found_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ChangePasswordFormFinder form_finder(
      pass_key(), web_contents(), client(), &logs_uploader, GURL(kUrlString),
      completion_callback.Get(), login_form_found_callback.Get(),
      capture_annotated_page_content.Get());
  // Simulate a delay between the page load and the form detection.
  task_environment()->FastForwardBy(base::Seconds(1));

  ASSERT_TRUE(form_finder.form_waiter());
  EXPECT_CALL(capture_annotated_page_content, Run).Times(0);
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_CALL(login_form_found_callback, Run).Times(1);

  auto form_manager = CreateLoginFormManager();
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder.form_waiter())
      ->OnPasswordFormParsed(form_manager.get());
  static_cast<content::WebContentsObserver*>(form_finder.form_waiter())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  // Finding a login form resets the timer, so even after
  // kFormWaitingTimeout + 1 seconds, we shouldn't time out.
  task_environment()->FastForwardBy(
      ChangePasswordFormFinder::kFormWaitingTimeout);

  // Verify there was navigation to change-pwd url
  EXPECT_EQ(GURL(kUrlString), web_contents()->GetURL());
  // Verify that form waiter is created again.
  EXPECT_TRUE(form_finder.form_waiter());
}
