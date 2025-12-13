// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
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
};

class MockPasswordChangeDelegateObserver
    : public PasswordChangeDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (PasswordChangeDelegate::State),
              (override));
  MOCK_METHOD(void,
              OnPasswordChangeStopped,
              (PasswordChangeDelegate*),
              (override));
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
    tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab_interface_, GetContents).WillByDefault(Return(web_contents()));
    web_contents()->SetUserData(
        ManagePasswordsUIController::UserDataKey(),
        std::make_unique<::testing::NiceMock<MockManagePasswordsUIController>>(
            web_contents()));
  }

  void TearDown() override {
    tab_interface_.reset();
    delegate_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordChangeDelegate* delegate() { return delegate_.get(); }

  void CreateDelegate() {
    password_manager::PasswordForm form;
    form.url = GURL(kChangePasswordURL);
    form.signon_realm = GURL(kChangePasswordURL).GetWithEmptyPath().spec();
    form.username_value = kTestEmail;
    form.password_value = kPassword;
    delegate_ = std::make_unique<PasswordChangeDelegateImpl>(
        GURL(kChangePasswordURL), std::move(form), tab_interface_.get());
    delegate_->SetCustomUIController(
        std::make_unique<MockPasswordChangeUIController>(delegate_.get()));
  }

  void ResetDelegate() { delegate_.reset(); }

  MockManagePasswordsUIController* manage_passwords_ui_controller() {
    return static_cast<MockManagePasswordsUIController*>(
        web_contents()->GetUserData(
            ManagePasswordsUIController::UserDataKey()));
  }

 private:
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  MockPageNavigator navigator_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<PasswordChangeDelegateImpl> delegate_;

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

  delegate()->StartPasswordChangeFlow();
  static_cast<PasswordChangeDelegateImpl*>(delegate())
      ->login_checker()
      ->RespondWithLoginStatus(LoginCheckResult::kLoggedIn);

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  FastForwardBy(base::Milliseconds(1234));
  static_cast<PasswordChangeDelegateImpl*>(delegate())
      ->form_finder()
      ->RespondWithFormNotFound();

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

TEST_F(PasswordChangeDelegateImplTest, OtpDetectionProcessed) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  autofill::FormData form = autofill::test::CreateTestUnclassifiedFormData();
  FakePasswordManagerClient fake_client;

  delegate()->StartPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  static_cast<PasswordChangeDelegateImpl*>(delegate())->OnOtpFieldDetected();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kOtpDetected);

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kOtpDetected, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::CoarseFinalPasswordChangeState::kOtpDetected,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegate::CoarseFinalPasswordChangeState::
                           kOtpDetected));
}

TEST_F(PasswordChangeDelegateImplTest, PasswordChangeFlowCanceled) {
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  delegate()->StartPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

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

TEST_F(PasswordChangeDelegateImplTest, LoginPasswordFormIsLogged) {
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();

  optimization_guide::proto::PasswordChangeQuality quality =
      static_cast<PasswordChangeDelegateImpl*>(delegate())
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_login_form_data());
}

TEST_F(PasswordChangeDelegateImplTest, DelegateNotifiesObserver) {
  CreateDelegate();

  MockPasswordChangeDelegateObserver observer;
  delegate()->AddObserver(&observer);

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);

  EXPECT_CALL(
      observer,
      OnStateChanged(
          PasswordChangeDelegate::State::kWaitingForChangePasswordForm));
  delegate()->OnPrivacyNoticeAccepted();
  delegate()->RemoveObserver(&observer);
}
