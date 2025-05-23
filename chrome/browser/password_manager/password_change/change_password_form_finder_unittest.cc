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
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
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

class FakeChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static void CreateForWebContents(content::WebContents* contents) {
    auto* client = new FakeChromePasswordManagerClient(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
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

    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));
    // `ChromePasswordManagerClient` observes `AutofillManager`s, so
    // `ChromeAutofillClient` needs to be set up, too.
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
    FakeChromePasswordManagerClient::CreateForWebContents(web_contents());
  }

  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager() {
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

  ChromePasswordManagerClient* client() {
    return ChromePasswordManagerClient::FromWebContents(web_contents());
  }

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
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::StubPasswordManagerDriver driver_;
};

TEST_F(ChangePasswordFormFinderTest, PasswordChangeFormFound) {
  auto form_manager = CreateFormManager();

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  ChangePasswordFormFinder form_waiter(pass_key(), web_contents(),
                                       completion_callback.Get(),
                                       capture_annotated_page_content.Get());

  ASSERT_TRUE(form_waiter.form_waiter());
  EXPECT_CALL(capture_annotated_page_content, Run).Times(0);
  EXPECT_CALL(completion_callback, Run(form_manager.get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_waiter.form_waiter())
      ->OnPasswordFormParsed(form_manager.get());
}

TEST_F(ChangePasswordFormFinderTest, ExecuteModelModelFailedWhenFormNotFound) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), completion_callback.Get(),
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
  EXPECT_CALL(completion_callback, Run(nullptr));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_F(ChangePasswordFormFinderTest, ButtonClickRequestedButFailed) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), completion_callback.Get(),
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
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());
  EXPECT_FALSE(form_finder->form_waiter());

  EXPECT_CALL(completion_callback, Run(nullptr));
  form_finder->click_helper()->SimulateClickResult(/*result=*/false);
}

TEST_F(ChangePasswordFormFinderTest, ButtonClickRequestedAndSucceeded) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  auto form_finder = std::make_unique<ChangePasswordFormFinder>(
      pass_key(), web_contents(), completion_callback.Get(),
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
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  // Since ExecuteModel() call was successful, `form_finder` is now attempting
  // to click an underlying button.
  EXPECT_TRUE(form_finder->click_helper());
  EXPECT_FALSE(form_finder->form_waiter());

  form_finder->click_helper()->SimulateClickResult(/*result=*/true);
  EXPECT_FALSE(form_finder->click_helper());

  // Now `form_finder` is waiting for the change password form again.
  EXPECT_TRUE(form_finder->form_waiter());

  auto form_manager = CreateFormManager();
  EXPECT_CALL(completion_callback, Run(form_manager.get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      form_finder->form_waiter())
      ->OnPasswordFormParsed(form_manager.get());
}
