// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/features.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using UkmEntry = ukm::builders::PasswordManager_ChangeFlowOutcome;

constexpr char kChangePasswordURL[] = "https://example.com/password/";
const std::u16string kTestEmail = u"elisa.buckett@gmail.com";
const std::u16string kPassword = u"cE1L45Vgxyzlu8";

class MockPageNavigator : public content::PageNavigator {
 public:
  MOCK_METHOD(content::WebContents*,
              OpenURL,
              (const content::OpenURLParams&,
               base::OnceCallback<void(content::NavigationHandle&)>),
              (override));
};

class MockPasswordChangeUIController : public PasswordChangeUIController {
 public:
  MockPasswordChangeUIController(
      PasswordChangeDelegate* password_change_delegate)
      : PasswordChangeUIController(password_change_delegate, nullptr) {}
  ~MockPasswordChangeUIController() override = default;

  MOCK_METHOD(void, UpdateState, (PasswordChangeDelegate::State), (override));
};

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;

  const GURL& GetLastCommittedURL() const override { return url_; }

 private:
  GURL url_ = GURL("example.com");
};

class MockManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit MockManagePasswordsUIController(content::WebContents* web_contents)
      : ManagePasswordsUIController(web_contents) {}
  ~MockManagePasswordsUIController() override = default;

  MOCK_METHOD(base::WeakPtr<PasswordsModelDelegate>,
              GetModelDelegateProxy,
              (),
              (override));
  MOCK_METHOD(void, OnPasswordChangeFinishedSuccessfully, (), (override));
  MOCK_METHOD(void,
              ShowChangePasswordBubble,
              (const std::u16string&, const std::u16string&),
              (override));
  void PrimaryPageChanged(content::Page& page) override {}
};

class MockPasswordChangeDelegateObserver
    : public PasswordChangeDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnPasswordChangeStopped,
              (PasswordChangeDelegate*),
              (override));
};

class MockPasswordChangeActuator : public PasswordChangeActuator {
 public:
  MockPasswordChangeActuator() = default;
  ~MockPasswordChangeActuator() override = default;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(content::WebContents*,
              GetExecutorWebContents,
              (),
              (const, override));
  MOCK_METHOD(void, OpenPasswordChangeTab, (content::WebContents*), (override));
  MOCK_METHOD(std::u16string, GetGeneratedPassword, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));

  base::WeakPtr<MockPasswordChangeActuator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeActuator> weak_ptr_factory_{this};
};

const ukm::mojom::UkmEntry* GetUkmEntry(
    const ukm::TestAutoSetUkmRecorder& test_ukm_recorder) {
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  CHECK_EQ(ukm_entries.size(), 1u);
  return ukm_entries[0];
}

}  // namespace

class PasswordChangeDelegateImplTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangeDelegateImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PasswordChangeDelegateImplTest() override = default;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  PrefService* prefs() { return profile()->GetPrefs(); }
  MockPageNavigator& navigator() { return navigator_; }

  void SetOptimizationFeatureEnabled(bool enabled) {
    ON_CALL(*mock_optimization_guide_keyed_service_,
            ShouldFeatureBeCurrentlyEnabledForUser(
                optimization_guide::UserVisibleFeatureKey::
                    kPasswordChangeSubmission))
        .WillByDefault(Return(enabled));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          NiceMock<MockOptimizationGuideKeyedService>>();
                    })));
    auto logs_uploader = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        TestingBrowserProcess::GetGlobal()->local_state());
    mock_optimization_guide_keyed_service_
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));
    tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab_interface_, GetContents).WillByDefault(Return(web_contents()));
    ON_CALL(*tab_interface_, RegisterWillDetach)
        .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
          tab_will_detach_callback_ = std::move(callback);
          return base::CallbackListSubscription();
        });
    web_contents()->SetUserData(
        ManagePasswordsUIController::UserDataKey(),
        std::make_unique<::testing::NiceMock<MockManagePasswordsUIController>>(
            web_contents()));
  }

  void TearDown() override {
    tab_interface_.reset();
    actuator_.reset();
    delegate_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
    layout_provider_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordChangeDelegateImpl* delegate() { return delegate_.get(); }

  void CreateDelegate() {
    password_manager::PasswordForm form;
    form.url = GURL(kChangePasswordURL);
    form.signon_realm = GURL(kChangePasswordURL).GetWithEmptyPath().spec();
    form.username_value = kTestEmail;
    form.password_value =
        password_manager::PasswordString(std::u16string(kPassword));
    delegate_ = std::make_unique<PasswordChangeDelegateImpl>(
        GURL(kChangePasswordURL), std::move(form), tab_interface_.get());
    delegate_->SetCustomUIController(
        std::make_unique<NiceMock<MockPasswordChangeUIController>>(
            delegate_.get()));
    actuator_ = InjectMockActuator();
  }

  void ResetDelegate() {
    actuator_.reset();
    delegate_.reset();
  }

  MockManagePasswordsUIController* manage_passwords_ui_controller() {
    return static_cast<MockManagePasswordsUIController*>(
        web_contents()->GetUserData(
            ManagePasswordsUIController::UserDataKey()));
  }

  MockPasswordChangeUIController* mock_ui_controller() {
    return static_cast<MockPasswordChangeUIController*>(
        delegate()->ui_controller());
  }

  base::WeakPtr<MockPasswordChangeActuator> InjectMockActuator() {
    auto mock_actuator =
        std::make_unique<NiceMock<MockPasswordChangeActuator>>();
    base::WeakPtr<MockPasswordChangeActuator> actuator_ptr =
        mock_actuator->GetWeakPtr();
    delegate()->inject_actuator_for_testing(std::move(mock_actuator));
    return actuator_ptr;
  }

  MockPasswordChangeActuator* actuator() { return actuator_.get(); }

  void SimulateTabWillDetach(tabs::TabInterface::DetachReason reason) {
    if (tab_will_detach_callback_) {
      tab_will_detach_callback_.Run(tab_interface_.get(), reason);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  MockPageNavigator navigator_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<PasswordChangeDelegateImpl> delegate_;
  base::WeakPtr<MockPasswordChangeActuator> actuator_;
  tabs::TabInterface::WillDetach tab_will_detach_callback_;
  std::unique_ptr<views::LayoutProvider> layout_provider_;

  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(PasswordChangeDelegateImplTest, WaitingForAgreement) {
  base::HistogramTester histogram_tester;
  CreateDelegate();
  EXPECT_EQ(
      prefs()->GetInteger(optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::
              kPasswordChangeSubmission)),
      static_cast<int>(
          optimization_guide::prefs::FeatureOptInState::kNotInitialized));

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);

  delegate()->OnPrivacyNoticeAccepted();
  SetOptimizationFeatureEnabled(true);
  // Both pref and state reflect acceptance.
  EXPECT_EQ(
      prefs()->GetInteger(optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::
              kPasswordChangeSubmission)),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  ResetDelegate();

  histogram_tester.ExpectTotalCount(
      PasswordChangeDelegateImpl::kPasswordChangeTimeOverallHistogram, 1);
}

TEST_F(PasswordChangeDelegateImplTest, PasswordChangeFormNotFound) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  EXPECT_CALL(*actuator(), Start());

  delegate()->StartPasswordChangeFlow();
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn);

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  FastForwardBy(base::Milliseconds(1234));
  delegate()->OnActuationStateChanged(
      PasswordChangeActuator::State::kChangePasswordFormNotFound);

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kChangePasswordFormNotFound);
  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kChangePasswordFormNotFound,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kFormNotDetected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kPasswordChangeTimeOverallHistogram, 1234, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegate::CoarseFinalPasswordChangeState::
                           kFormNotDetected));
}

TEST_F(PasswordChangeDelegateImplTest, MetricsReportedFlowOffered) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kOfferingPasswordChange,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOffered,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(
          PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOffered));
}

TEST_F(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledInPrivacyNotice) {
  SetOptimizationFeatureEnabled(false);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kWaitingForAgreement,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOffered,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(
          PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOffered));
}

TEST_F(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledDuringSignInCheck) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  delegate()->StartPasswordChangeFlow();

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kCanceled,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(
          PasswordChangeDelegate::CoarseFinalPasswordChangeState::kCanceled));
}

TEST_F(PasswordChangeDelegateImplTest, PasswordChangeFlowCanceled) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  EXPECT_CALL(*actuator(), Start());

  delegate()->StartPasswordChangeFlow();
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_CALL(*actuator(), Cancel());
  delegate()->CancelPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kCanceled);

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kCanceled, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kCanceled,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(
          PasswordChangeDelegate::CoarseFinalPasswordChangeState::kCanceled));
}

TEST_F(PasswordChangeDelegateImplTest, OnPasswordChangeDeclined) {
  CreateDelegate();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);

  PasswordsModelDelegateMock mock_model_delegate;
  EXPECT_CALL(*manage_passwords_ui_controller(), GetModelDelegateProxy)
      .WillOnce(Return(mock_model_delegate.AsWeakPtr()));
  delegate()->OnPasswordChangeDeclined();

  PasswordsLeakDialogDelegateMock mock_leak_delegate;
  EXPECT_CALL(mock_model_delegate, GetPasswordsLeakDialogDelegate)
      .WillOnce(Return(&mock_leak_delegate));
  EXPECT_CALL(mock_leak_delegate, OnLeakDialogHidden);

  task_environment()->RunUntilIdle();
}

TEST_F(PasswordChangeDelegateImplTest,
       ActuationStateChangedUpdatesUIAndObservers) {
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  const std::vector<
      std::pair<PasswordChangeActuator::State, PasswordChangeDelegate::State>>
      kTestStates = {
          {PasswordChangeActuator::State::kChangingPassword,
           PasswordChangeDelegate::State::kChangingPassword},
          {PasswordChangeActuator::State::kOtpDetected,
           PasswordChangeDelegate::State::kOtpDetected},
          {PasswordChangeActuator::State::kChangePasswordFormNotFound,
           PasswordChangeDelegate::State::kChangePasswordFormNotFound},
          {PasswordChangeActuator::State::kPasswordChangeFailed,
           PasswordChangeDelegate::State::kPasswordChangeFailed},
      };

  for (const auto& [actuator_state, delegate_state] : kTestStates) {
    EXPECT_CALL(*mock_ui_controller(), UpdateState(delegate_state));
    delegate()->OnActuationStateChanged(actuator_state);
    EXPECT_EQ(delegate()->GetCurrentState(), delegate_state);
  }

  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest,
       PasswordSuccessfullyChangedUpdatesUIAndNotifiesController) {
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_CALL(
      *mock_ui_controller(),
      UpdateState(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged));
  EXPECT_CALL(*manage_passwords_ui_controller(),
              OnPasswordChangeFinishedSuccessfully());

  delegate()->OnActuationStateChanged(
      PasswordChangeActuator::State::kPasswordSuccessfullyChanged);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kPasswordSuccessfullyChanged);

  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest,
       LoginCheck_LoggedIn_StartsActuatorAndRecordsQuality) {
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();

  EXPECT_CALL(*actuator(), Start());

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn);

  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_logged_in_check());
}

TEST_F(PasswordChangeDelegateImplTest,
       LoginCheck_LoggedOut_TransitionsToLoginFormDetected_AndRetry) {
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedOut);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kLoginFormDetected);

  delegate()->RetryLoginCheck();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
}

TEST_F(PasswordChangeDelegateImplTest,
       LoginCheck_Error_TransitionsToFormNotFound) {
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();
  ASSERT_TRUE(delegate()->login_checker());

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kError);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kChangePasswordFormNotFound);
  EXPECT_FALSE(delegate()->login_checker());
}

TEST_F(PasswordChangeDelegateImplTest,
       OnTabWillDetach_DeleteReason_CancelsActuatorAndStopsFlow) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  delegate()->StartPasswordChangeFlow();
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn);

  EXPECT_CALL(*actuator(), Cancel());
  EXPECT_CALL(observer, OnPasswordChangeStopped(delegate()));
  base::HistogramTester histogram_tester;

  SimulateTabWillDetach(tabs::TabInterface::DetachReason::kDelete);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.UserClosedTab",
      PasswordChangeDelegate::State::kWaitingForChangePasswordForm, 1);
  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest, OnTabWillDetach_OtherReason_Ignored) {
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_CALL(*actuator(), Cancel()).Times(0);
  EXPECT_CALL(observer, OnPasswordChangeStopped).Times(0);

  SimulateTabWillDetach(
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);
  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest, StopCalledAfterTimeout_OnCanceled) {
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasswordChangeStopped).Times(0);
  delegate()->CancelPasswordChangeFlow();

  EXPECT_CALL(observer, OnPasswordChangeStopped(delegate()));
  FastForwardBy(base::Seconds(8));
  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest, StopCalledAfterTimeout_OnSuccess) {
  CreateDelegate();
  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasswordChangeStopped).Times(0);
  delegate()->OnActuationStateChanged(
      PasswordChangeActuator::State::kPasswordSuccessfullyChanged);

  EXPECT_CALL(observer, OnPasswordChangeStopped(delegate()));
  FastForwardBy(base::Seconds(8));
  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest, PrivateInferenceLoginCheck_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_change::features::kPasswordChangeWithPrivateInferenceLoginCheck);
  SetOptimizationFeatureEnabled(true);

  CreateDelegate();

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kNoState);
  ASSERT_TRUE(delegate()->login_checker());

  auto logging_data = std::make_unique<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>();
  logging_data->mutable_response()
      ->mutable_is_logged_in_data()
      ->set_is_logged_in(true);

  // Even though Optimization Guide feature is enabled (e.g. legacy APC was
  // accepted), the user must still agree to the new Private Inference notice.
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::
          kPasswordChangeWithPrivateInferenceNoticeAgreement));

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn, std::move(logging_data));
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);
  EXPECT_FALSE(delegate()->login_checker());
  EXPECT_FALSE(delegate()->logs_uploader());

  EXPECT_CALL(*actuator(), Start());

  delegate()->OnPrivacyNoticeAccepted();

  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::
          kPasswordChangeWithPrivateInferenceNoticeAgreement));
  EXPECT_EQ(
      prefs()->GetInteger(optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::
              kPasswordChangeSubmission)),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));

  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_logged_in_check());
  EXPECT_TRUE(
      quality.logged_in_check().response().is_logged_in_data().is_logged_in());

  // Subsequent flow with notice already accepted transitions directly to
  // offering without re-showing the privacy notice.
  ResetDelegate();
  CreateDelegate();
  ASSERT_TRUE(delegate()->login_checker());
  auto subsequent_logging_data = std::make_unique<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>();
  subsequent_logging_data->mutable_response()
      ->mutable_is_logged_in_data()
      ->set_is_logged_in(true);
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedIn, std::move(subsequent_logging_data));
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kOfferingPasswordChange);
}

TEST_F(PasswordChangeDelegateImplTest, PrivateInferenceLoginCheck_Failure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_change::features::kPasswordChangeWithPrivateInferenceLoginCheck);

  CreateDelegate();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kNoState);
  ASSERT_TRUE(delegate()->login_checker());

  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasswordChangeStopped(delegate()));
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kError);
  EXPECT_FALSE(delegate()->logs_uploader());

  delegate()->RemoveObserver(&observer);
}

TEST_F(PasswordChangeDelegateImplTest, IsPasswordChangeOngoing) {
  CreateDelegate();

  std::unique_ptr<content::WebContents> test_executor =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  EXPECT_CALL(*actuator(), GetExecutorWebContents())
      .WillRepeatedly(Return(test_executor.get()));

  EXPECT_TRUE(delegate()->IsPasswordChangeOngoing(web_contents()));
  EXPECT_TRUE(delegate()->IsPasswordChangeOngoing(test_executor.get()));
  EXPECT_FALSE(delegate()->IsPasswordChangeOngoing(nullptr));
}

TEST_F(PasswordChangeDelegateImplTest, OpenPasswordChangeTab) {
  CreateDelegate();

  EXPECT_CALL(*actuator(), OpenPasswordChangeTab(web_contents()));
  delegate()->OpenPasswordChangeTab();
}

TEST_F(PasswordChangeDelegateImplTest, OpenPasswordDetails) {
  CreateDelegate();
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kChangePasswordURL));

  const std::u16string kNewPassword = u"NewSecurePassword";
  EXPECT_CALL(*actuator(), GetGeneratedPassword())
      .WillOnce(Return(kNewPassword));
  EXPECT_CALL(*manage_passwords_ui_controller(),
              ShowChangePasswordBubble(kTestEmail, kNewPassword));

  delegate()->OpenPasswordDetails();
}

TEST_F(PasswordChangeDelegateImplTest,
       OpenPasswordDetails_ShowTabFeatureEnabled) {
  base::test::ScopedFeatureList scoped_features(
      password_manager::features::kShowTabWithPasswordChangeOnSuccess);
  CreateDelegate();

  EXPECT_CALL(*actuator(), OpenPasswordChangeTab(web_contents()));
  EXPECT_CALL(*manage_passwords_ui_controller(), ShowChangePasswordBubble)
      .Times(0);

  delegate()->OpenPasswordDetails();
}

TEST_F(PasswordChangeDelegateImplTest, LoginPasswordFormIsLogged) {
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();
  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_login_form_data());
}
