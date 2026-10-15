// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/script_password_change_actuator.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"
#include "chrome/browser/password_manager/password_change/detached_web_contents.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kChangePasswordURL[] = "https://example.com/password/";
const std::u16string kTestEmail = u"elisa.buckett@gmail.com";
const std::u16string kPassword = u"cE1L45Vgxyzlu8";

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

}  // namespace

class ScriptPasswordChangeActuatorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ScriptPasswordChangeActuatorTest() = default;
  ~ScriptPasswordChangeActuatorTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<testing::NiceMock<
                          MockOptimizationGuideKeyedService>>();
                    })));
    auto logs_uploader = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        TestingBrowserProcess::GetGlobal()->local_state());
    mock_optimization_guide_keyed_service_
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));
  }

  void TearDown() override {
    logs_uploader_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ModelQualityLogsUploader* GetLogsUploader() {
    if (!logs_uploader_) {
      logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(
          web_contents(), GURL(kChangePasswordURL));
    }
    return logs_uploader_.get();
  }

  password_manager::PasswordForm CreateTestForm() {
    password_manager::PasswordForm form;
    form.url = GURL(kChangePasswordURL);
    form.signon_realm = GURL(kChangePasswordURL).GetWithEmptyPath().spec();
    form.username_value = kTestEmail;
    form.password_value =
        password_manager::PasswordString(std::u16string(kPassword));
    return form;
  }

  void InjectHiddenExecutor(ScriptPasswordChangeActuator& actuator) {
    auto detached_web_contents = std::make_unique<DetachedWebContents>(
        base::PassKey<ScriptPasswordChangeActuatorTest>(), profile(),
        GURL(kChangePasswordURL));
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile());
    autofill_client_provider.CreateClientForWebContents(
        detached_web_contents->GetWebContents());
    ChromePasswordManagerClient::CreateForWebContents(
        detached_web_contents->GetWebContents());
    actuator.inject_hidden_executor_for_testing(
        std::move(detached_web_contents));
  }

 private:
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_ = nullptr;
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
};

TEST_F(ScriptPasswordChangeActuatorTest, StartCreatesExecutorAndFormFinder) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  EXPECT_FALSE(actuator.GetExecutorWebContents());
  EXPECT_FALSE(actuator.GetFormFinderForTesting());
  EXPECT_FALSE(actuator.GetNavigationObserverForTesting());

  InjectHiddenExecutor(actuator);
  actuator.Start();

  EXPECT_TRUE(actuator.GetExecutorWebContents());
  EXPECT_TRUE(actuator.GetFormFinderForTesting());
  EXPECT_TRUE(actuator.GetNavigationObserverForTesting());
}

TEST_F(ScriptPasswordChangeActuatorTest, StartCalledTwiceReusesExecutor) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  InjectHiddenExecutor(actuator);
  actuator.Start();
  content::WebContents* first_executor = actuator.GetExecutorWebContents();
  EXPECT_TRUE(first_executor);

  actuator.Start();
  EXPECT_EQ(actuator.GetExecutorWebContents(), first_executor);
}

TEST_F(ScriptPasswordChangeActuatorTest, CancelNotifiesObserverAndResetsState) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  MockPasswordChangeActuatorObserver observer;
  actuator.AddObserver(&observer);

  InjectHiddenExecutor(actuator);
  actuator.Start();
  EXPECT_TRUE(actuator.GetFormFinderForTesting());

  actuator.Cancel();

  EXPECT_FALSE(actuator.GetFormFinderForTesting());
  EXPECT_EQ(
      actuator.GetLogsUploaderForTesting()
          ->GetFinalLog()
          .password_change_submission()
          .quality()
          .open_form()
          .status(),
      optimization_guide::proto::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);

  actuator.RemoveObserver(&observer);
}

TEST_F(ScriptPasswordChangeActuatorTest, CancelWithoutActiveStepResetsState) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  MockPasswordChangeActuatorObserver observer;
  actuator.AddObserver(&observer);

  auto logging_data = std::make_unique<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>();
  logging_data->mutable_response()
      ->mutable_is_logged_in_data()
      ->set_is_logged_in(true);
  GetLogsUploader()->SetLoggedInCheckQuality(/*retry_count=*/0,
                                             std::move(logging_data));

  actuator.Cancel();

  EXPECT_EQ(
      actuator.GetLogsUploaderForTesting()
          ->GetFinalLog()
          .password_change_submission()
          .quality()
          .logged_in_check()
          .status(),
      optimization_guide::proto::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  actuator.RemoveObserver(&observer);
}

TEST_F(ScriptPasswordChangeActuatorTest,
       OpenPasswordChangeTabBeforeStartOpensURL) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  EXPECT_CALL(delegate,
              OpenURLFromTab(web_contents(), ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(web_contents()));

  actuator.OpenPasswordChangeTab(web_contents());

  web_contents()->SetDelegate(nullptr);
}

TEST_F(ScriptPasswordChangeActuatorTest,
       OnPasswordChangeFormNotFoundNotifiesObserver) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  MockPasswordChangeActuatorObserver observer;
  actuator.AddObserver(&observer);

  InjectHiddenExecutor(actuator);
  actuator.Start();
  EXPECT_CALL(observer,
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kChangePasswordFormNotFound));

  actuator.GetFormFinderForTesting()->RespondWithFormNotFound();

  EXPECT_FALSE(actuator.GetFormFinderForTesting());
  actuator.RemoveObserver(&observer);
}

TEST_F(ScriptPasswordChangeActuatorTest,
       OnCrossOriginNavigationDetectedDuringFormFinder) {
  ScriptPasswordChangeActuator actuator(
      GURL(kChangePasswordURL), CreateTestForm(), profile(), GetLogsUploader());
  MockPasswordChangeActuatorObserver observer;
  actuator.AddObserver(&observer);

  InjectHiddenExecutor(actuator);
  actuator.Start();
  EXPECT_TRUE(actuator.GetFormFinderForTesting());
  EXPECT_TRUE(actuator.GetNavigationObserverForTesting());

  EXPECT_CALL(observer,
              OnActuationStateChanged(
                  PasswordChangeActuator::State::kChangePasswordFormNotFound));

  actuator.GetNavigationObserverForTesting()
      ->TriggerCrossOriginNavigationForTesting();

  EXPECT_FALSE(actuator.GetFormFinderForTesting());
  EXPECT_FALSE(actuator.GetNavigationObserverForTesting());
  actuator.RemoveObserver(&observer);
}
