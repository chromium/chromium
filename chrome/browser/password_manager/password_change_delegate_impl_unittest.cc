// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/with_feature_override.h"
#include "base/types/pass_key.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/features.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
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

password_manager::LeakedPasswordDetails CreateLeakedPasswordDetails() {
  password_manager::PasswordForm form;
  form.url = GURL(kChangePasswordURL);
  form.signon_realm = GURL(kChangePasswordURL).GetWithEmptyPath().spec();
  form.username_value = kTestEmail;
  form.password_value =
      password_manager::PasswordString(std::u16string(kPassword));
  return password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(
          password_manager::IsSaved(true), password_manager::IsReused(false),
          password_manager::IsSyncing(true),
          password_manager::HasChangePasswordUrl(true)),
      std::move(form), /*in_account_store=*/true);
}

class MockPasswordChangeUIController : public PasswordChangeUIController {
 public:
  explicit MockPasswordChangeUIController(PasswordChangeUIDelegate* ui_delegate)
      : PasswordChangeUIController(ui_delegate, nullptr) {}
  ~MockPasswordChangeUIController() override = default;

  MOCK_METHOD(void, UpdateState, (PasswordChangeUIDelegate::State), (override));
  MOCK_METHOD(void,
              OnLoginCheckFailedWithServerError,
              (password_manager::LeakedPasswordDetails),
              (override));
};

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;

  const GURL& GetLastCommittedURL() const override { return url_; }

 private:
  GURL url_ = GURL("example.com");
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
  MOCK_METHOD(password_manager::PasswordString,
              GetGeneratedPassword,
              (),
              (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));

  base::WeakPtr<MockPasswordChangeActuator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeActuator> weak_ptr_factory_{this};
};

// Owns a `PasswordFormManager` observing an empty form.
class FormManagerForTesting {
 public:
  FormManagerForTesting()
      : form_manager_(
            &client_,
            driver_.AsWeakPtr(),
            autofill::FormData(),
            &form_fetcher_,
            std::make_unique<password_manager::PasswordSaveManagerImpl>(
                &client_),
            /*metrics_recorder=*/nullptr) {}

  password_manager::PasswordFormManager& get() { return form_manager_; }

 private:
  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::PasswordFormManager form_manager_;
};

const ukm::mojom::UkmEntry* GetUkmEntry(
    const ukm::TestAutoSetUkmRecorder& test_ukm_recorder) {
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  CHECK_EQ(ukm_entries.size(), 1u);
  return ukm_entries[0];
}

class CustomTestOtpFieldDetector : public autofill::OtpFieldDetector {
 public:
  using OtpFieldDetector::AddFormAndNotifyIfNecessary;
  using OtpFieldDetector::OtpFieldDetector;
  using OtpFieldDetector::RemoveFormAndNotifyIfNecessary;
};

class CustomTestContentAutofillClient
    : public autofill::TestContentAutofillClient {
 public:
  explicit CustomTestContentAutofillClient(content::WebContents* web_contents)
      : autofill::TestContentAutofillClient(web_contents),
        otp_field_detector_(this) {}
  ~CustomTestContentAutofillClient() override = default;

  CustomTestOtpFieldDetector* GetOtpFieldDetector() override {
    return &otp_field_detector_;
  }

 private:
  CustomTestOtpFieldDetector otp_field_detector_;
};

}  // namespace

class PasswordChangeDelegateImplTest : public base::test::WithFeatureOverride,
                                       public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangeDelegateImplTest()
      : base::test::WithFeatureOverride(
            password_change::features::
                kPasswordChangeWithPrivateInferenceLoginCheck),
        ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PasswordChangeDelegateImplTest() override = default;

  bool IsPrivateInferenceLoginCheckEnabled() const {
    return IsParamFeatureEnabled();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  PrefService* prefs() { return profile()->GetPrefs(); }

  void SetOptimizationFeatureEnabled(bool enabled) {
    ON_CALL(*mock_optimization_guide_keyed_service_,
            ShouldFeatureBeCurrentlyEnabledForUser(
                optimization_guide::UserVisibleFeatureKey::
                    kPasswordChangeSubmission))
        .WillByDefault(Return(enabled));
  }

  void SetPrivacyNoticeAcknowledged(bool acknowledged) {
    SetOptimizationFeatureEnabled(acknowledged);
    if (IsPrivateInferenceLoginCheckEnabled()) {
      prefs()->SetBoolean(
          password_manager::prefs::
              kPasswordChangeWithPrivateInferenceNoticeAgreement,
          acknowledged);
    }
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
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         tab_interface_.get());
    ON_CALL(*tab_interface_, RegisterWillDetach)
        .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
          tab_will_detach_callback_ = std::move(callback);
          return base::CallbackListSubscription();
        });
  }

  void TearDown() override {
    actuator_.reset();
    delegate_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
    layout_provider_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
    tab_interface_.reset();
  }

  PasswordChangeDelegateImpl* delegate() { return delegate_.get(); }

  void CreateDelegate(bool inject_actuator = true) {
    delegate_ = std::make_unique<PasswordChangeDelegateImpl>(
        GURL(kChangePasswordURL), CreateLeakedPasswordDetails(),
        tab_interface_.get());
    delegate_->SetCustomUIController(
        std::make_unique<NiceMock<MockPasswordChangeUIController>>(
            delegate_.get()));
    delegate_->Start(completion_callback_.Get());
    if (inject_actuator) {
      actuator_ = InjectMockActuator();
    }
  }

  void CreateDelegateAndOffer() {
    CreateDelegate();
    if (IsPrivateInferenceLoginCheckEnabled()) {
      delegate()->login_checker()->RespondWithLoginStatus(
          LoginCheckResult::Status::kLoggedIn);
    }
  }

  void CreateDelegateAndStartFlow() {
    CreateDelegateAndOffer();
    EXPECT_CALL(*actuator(), Start());
    delegate()->StartPasswordChangeFlow();
    if (IsPrivateInferenceLoginCheckEnabled()) {
      delegate()->OnActuationStateChanged(
          PasswordChangeActuator::State::kWaitingForChangePasswordForm);
    } else {
      delegate()->login_checker()->RespondWithLoginStatus(
          LoginCheckResult::Status::kLoggedIn);
    }
  }

  void ResetDelegate() {
    actuator_.reset();
    delegate_.reset();
  }

  base::MockCallback<base::OnceClosure>& completion_callback() {
    return completion_callback_;
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

  CustomTestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

 private:
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  NiceMock<base::MockCallback<base::OnceClosure>> completion_callback_;
  std::unique_ptr<PasswordChangeDelegateImpl> delegate_;
  base::WeakPtr<MockPasswordChangeActuator> actuator_;
  tabs::TabInterface::WillDetach tab_will_detach_callback_;
  std::unique_ptr<views::LayoutProvider> layout_provider_;

  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  autofill::TestAutofillClientInjector<CustomTestContentAutofillClient>
      autofill_client_injector_;
};

TEST_P(PasswordChangeDelegateImplTest,
       GeneratedPasswordWithoutActuatorIsEmpty) {
  CreateDelegate(/*inject_actuator=*/false);

  EXPECT_TRUE(delegate()->GetGeneratedPassword().empty());
}

TEST_P(PasswordChangeDelegateImplTest, GeneratedPasswordOutlivesDelegate) {
  CreateDelegate();
  const password_manager::PasswordString password(u"generated_password");
  EXPECT_CALL(*actuator(), GetGeneratedPassword()).WillOnce(Return(password));

  const password_manager::PasswordString generated_password =
      delegate()->GetGeneratedPassword();
  ResetDelegate();

  EXPECT_EQ(generated_password, password);
}

TEST_P(PasswordChangeDelegateImplTest, ProvidesOldPassword) {
  CreateDelegate();

  EXPECT_THAT(delegate()->GetOldPassword(),
              base::test::ValueIs(password_manager::PasswordString(
                  std::u16string(kPassword))));
}

TEST_P(PasswordChangeDelegateImplTest, OldPasswordFailsWithoutActuator) {
  CreateDelegate(/*inject_actuator=*/false);

  EXPECT_THAT(delegate()->GetOldPassword(),
              base::test::ErrorIs(
                  PasswordChangeDelegateImpl::Error::kNoPasswordChangeFlow));
}

TEST_P(PasswordChangeDelegateImplTest, ProvidesActuatorGeneratedPassword) {
  CreateDelegate();
  FormManagerForTesting form_manager;
  const password_manager::PasswordString password(u"generated_password");
  EXPECT_CALL(*actuator(), GetGeneratedPassword()).WillOnce(Return(password));

  EXPECT_THAT(delegate()->GetNewPassword(&form_manager.get()),
              base::test::ValueIs(password));
}

TEST_P(PasswordChangeDelegateImplTest, NewPasswordFailsIfNotGenerated) {
  CreateDelegate();
  FormManagerForTesting form_manager;
  EXPECT_CALL(*actuator(), GetGeneratedPassword())
      .WillOnce(Return(password_manager::PasswordString()));

  EXPECT_THAT(delegate()->GetNewPassword(&form_manager.get()),
              base::test::ErrorIs(
                  PasswordChangeDelegateImpl::Error::kGenerationFailed));
}

TEST_P(PasswordChangeDelegateImplTest, NewPasswordFailsWithoutActuator) {
  CreateDelegate(/*inject_actuator=*/false);
  FormManagerForTesting form_manager;

  EXPECT_THAT(delegate()->GetNewPassword(&form_manager.get()),
              base::test::ErrorIs(
                  PasswordChangeDelegateImpl::Error::kNoPasswordChangeFlow));
}

TEST_P(PasswordChangeDelegateImplTest, WaitingForAgreement) {
  base::HistogramTester histogram_tester;
  CreateDelegateAndOffer();
  EXPECT_EQ(
      prefs()->GetInteger(optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::
              kPasswordChangeSubmission)),
      static_cast<int>(
          optimization_guide::prefs::FeatureOptInState::kNotInitialized));
  if (IsPrivateInferenceLoginCheckEnabled()) {
    EXPECT_FALSE(prefs()->GetBoolean(
        password_manager::prefs::
            kPasswordChangeWithPrivateInferenceNoticeAgreement));
  }

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForAgreement);

  if (IsPrivateInferenceLoginCheckEnabled()) {
    EXPECT_CALL(*actuator(), Start());
  }
  delegate()->OnPrivacyNoticeAccepted();
  SetPrivacyNoticeAcknowledged(true);
  // Both pref and state reflect acceptance.
  EXPECT_EQ(
      prefs()->GetInteger(optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::
              kPasswordChangeSubmission)),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  if (IsPrivateInferenceLoginCheckEnabled()) {
    EXPECT_TRUE(prefs()->GetBoolean(
        password_manager::prefs::
            kPasswordChangeWithPrivateInferenceNoticeAgreement));
    delegate()->OnActuationStateChanged(
        PasswordChangeActuator::State::kWaitingForChangePasswordForm);
  }
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);
  ResetDelegate();

  histogram_tester.ExpectTotalCount(
      PasswordChangeDelegateImpl::kPasswordChangeTimeOverallHistogram, 1);
}

TEST_P(PasswordChangeDelegateImplTest, PasswordChangeFormNotFound) {
  SetPrivacyNoticeAcknowledged(true);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  CreateDelegateAndStartFlow();

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

  FastForwardBy(base::Milliseconds(1234));
  delegate()->OnActuationStateChanged(
      PasswordChangeActuator::State::kChangePasswordFormNotFound);

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kChangePasswordFormNotFound);
  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeUIDelegate::State::kChangePasswordFormNotFound,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegateImpl::CoarseFinalPasswordChangeState::
          kFormNotDetected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kPasswordChangeTimeOverallHistogram, 1234, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegateImpl::
                           CoarseFinalPasswordChangeState::kFormNotDetected));
}

TEST_P(PasswordChangeDelegateImplTest, MetricsReportedFlowOffered) {
  SetPrivacyNoticeAcknowledged(true);
  CreateDelegateAndOffer();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeUIDelegate::State::kOfferingPasswordChange,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegateImpl::CoarseFinalPasswordChangeState::kOffered,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegateImpl::
                           CoarseFinalPasswordChangeState::kOffered));
}

TEST_P(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledInPrivacyNotice) {
  SetPrivacyNoticeAcknowledged(false);
  CreateDelegateAndOffer();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeUIDelegate::State::kWaitingForAgreement,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegateImpl::CoarseFinalPasswordChangeState::kOffered,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegateImpl::
                           CoarseFinalPasswordChangeState::kOffered));
}

TEST_P(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledDuringSignInCheck) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Login check is performed before flow starts when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  delegate()->StartPasswordChangeFlow();

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegateImpl::CoarseFinalPasswordChangeState::kCanceled,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegateImpl::
                           CoarseFinalPasswordChangeState::kCanceled));
}

TEST_P(PasswordChangeDelegateImplTest,
       PasswordChangeFlowCanceledDuringSignInCheck) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Login check is performed before flow starts when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  SetOptimizationFeatureEnabled(true);
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();

  EXPECT_CALL(*actuator(), Cancel());
  delegate()->CancelPasswordChangeFlow();

  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_EQ(
      quality.logged_in_check().status(),
      optimization_guide::proto::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
}

TEST_P(PasswordChangeDelegateImplTest, PasswordChangeFlowCanceled) {
  SetPrivacyNoticeAcknowledged(true);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  CreateDelegateAndStartFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_CALL(*actuator(), Cancel());
  delegate()->CancelPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kCanceled);

  ResetDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeUIDelegate::State::kCanceled, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kCoarseFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegateImpl::CoarseFinalPasswordChangeState::kCanceled,
      /*expected_bucket_count=*/1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      UkmEntry::kCoarseFinalPasswordChangeStatusName,
      static_cast<int>(PasswordChangeDelegateImpl::
                           CoarseFinalPasswordChangeState::kCanceled));
}

TEST_P(PasswordChangeDelegateImplTest, OnPasswordChangeDeclined) {
  CreateDelegateAndOffer();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForAgreement);

  EXPECT_CALL(completion_callback(), Run());
  delegate()->OnPasswordChangeDeclined();
}

TEST_P(PasswordChangeDelegateImplTest, ActuationStateChangedUpdatesUI) {
  CreateDelegate();

  const std::vector<
      std::pair<PasswordChangeActuator::State, PasswordChangeUIDelegate::State>>
      kTestStates = {
          {PasswordChangeActuator::State::kChangingPassword,
           PasswordChangeUIDelegate::State::kChangingPassword},
          {PasswordChangeActuator::State::kOtpDetected,
           PasswordChangeUIDelegate::State::kOtpDetected},
          {PasswordChangeActuator::State::kChangePasswordFormNotFound,
           PasswordChangeUIDelegate::State::kChangePasswordFormNotFound},
          {PasswordChangeActuator::State::kPasswordChangeFailed,
           PasswordChangeUIDelegate::State::kPasswordChangeFailed},
          {PasswordChangeActuator::State::kPasswordSuccessfullyChanged,
           PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged},
      };

  for (const auto& [actuator_state, delegate_state] : kTestStates) {
    EXPECT_CALL(*mock_ui_controller(), UpdateState(delegate_state));
    delegate()->OnActuationStateChanged(actuator_state);
    EXPECT_EQ(delegate()->GetCurrentState(), delegate_state);
  }
}

TEST_P(PasswordChangeDelegateImplTest,
       LoginCheck_LoggedIn_StartsActuatorAndRecordsQuality) {
  CreateDelegateAndStartFlow();

  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_logged_in_check());
}

TEST_P(PasswordChangeDelegateImplTest,
       LoginCheck_LoggedOut_TransitionsToLoginFormDetected_AndRetry) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Login check is performed before flow starts when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kLoggedOut);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kLoginFormDetected);

  delegate()->RetryLoginCheck();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);
}

TEST_P(PasswordChangeDelegateImplTest,
       LoginCheck_Error_TransitionsToFormNotFound) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Login check is performed before flow starts when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  CreateDelegate();
  delegate()->StartPasswordChangeFlow();
  ASSERT_TRUE(delegate()->login_checker());

  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kError);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kChangePasswordFormNotFound);
  EXPECT_FALSE(delegate()->login_checker());
}

TEST_P(PasswordChangeDelegateImplTest,
       OnTabWillDetach_DeleteReason_CancelsActuatorAndStopsFlow) {
  SetPrivacyNoticeAcknowledged(true);
  CreateDelegateAndStartFlow();

  EXPECT_CALL(*actuator(), Cancel());
  EXPECT_CALL(completion_callback(), Run());
  base::HistogramTester histogram_tester;

  SimulateTabWillDetach(tabs::TabInterface::DetachReason::kDelete);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.UserClosedTab",
      PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm, 1);
}

TEST_P(PasswordChangeDelegateImplTest, OnTabWillDetach_OtherReason_Ignored) {
  CreateDelegate();

  EXPECT_CALL(*actuator(), Cancel()).Times(0);
  EXPECT_CALL(completion_callback(), Run).Times(0);

  SimulateTabWillDetach(
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);
}

TEST_P(PasswordChangeDelegateImplTest, PrivateInferenceLoginCheck_Success) {
  if (!IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Private Inference login check is disabled.";
  }
  SetOptimizationFeatureEnabled(true);

  CreateDelegate();

  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kNoState);
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
            PasswordChangeUIDelegate::State::kWaitingForAgreement);
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
            PasswordChangeUIDelegate::State::kOfferingPasswordChange);
}

TEST_P(PasswordChangeDelegateImplTest, PrivateInferenceLoginCheck_Failure) {
  if (!IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Private Inference login check is disabled.";
  }

  CreateDelegate();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kNoState);
  ASSERT_TRUE(delegate()->login_checker());

  EXPECT_CALL(*mock_ui_controller(), OnLoginCheckFailedWithServerError)
      .Times(0);
  EXPECT_CALL(completion_callback(), Run());
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kError, /*logging_data=*/nullptr,
      LoginCheckResult::LoginCheckError::kTimeout);
  EXPECT_FALSE(delegate()->logs_uploader());
}

TEST_P(PasswordChangeDelegateImplTest,
       PrivateInferenceLoginCheck_ServerError_ShowsLeakWarning) {
  if (!IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Private Inference login check is disabled.";
  }

  CreateDelegate();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kNoState);
  ASSERT_TRUE(delegate()->login_checker());

  EXPECT_CALL(*mock_ui_controller(),
              OnLoginCheckFailedWithServerError(CreateLeakedPasswordDetails()));
  EXPECT_CALL(completion_callback(), Run());
  delegate()->login_checker()->RespondWithLoginStatus(
      LoginCheckResult::Status::kError, /*logging_data=*/nullptr,
      LoginCheckResult::LoginCheckError::kServerError);
  EXPECT_FALSE(delegate()->logs_uploader());
}

// Verifies there is no crash and no UI notification (and hence no
// credential leak dialog) if the originator tab is closed while the login
// check is still in flight.
TEST_P(PasswordChangeDelegateImplTest,
       PrivateInferenceLoginCheck_OriginatorDestroyed_NoNotification) {
  if (!IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "Private Inference login check is disabled.";
  }

  CreateDelegate();
  ASSERT_TRUE(delegate()->login_checker());

  // Closing the tab stops the flow and destroys the login checker, so a late
  // server error can no longer reach the delegate.
  EXPECT_CALL(*mock_ui_controller(), OnLoginCheckFailedWithServerError)
      .Times(0);
  EXPECT_CALL(completion_callback(), Run());
  SimulateTabWillDetach(tabs::TabInterface::DetachReason::kDelete);

  EXPECT_FALSE(delegate()->login_checker());
}

TEST_P(PasswordChangeDelegateImplTest, IsPasswordChangeOngoing) {
  CreateDelegate();

  std::unique_ptr<content::WebContents> test_executor =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  EXPECT_CALL(*actuator(), GetExecutorWebContents())
      .WillRepeatedly(Return(test_executor.get()));

  EXPECT_TRUE(delegate()->IsPasswordChangeOngoing(web_contents()));
  EXPECT_TRUE(delegate()->IsPasswordChangeOngoing(test_executor.get()));
  EXPECT_FALSE(delegate()->IsPasswordChangeOngoing(nullptr));
}

TEST_P(PasswordChangeDelegateImplTest, OpenPasswordChangeTab) {
  CreateDelegate();

  EXPECT_CALL(*actuator(), OpenPasswordChangeTab(web_contents()));
  EXPECT_CALL(completion_callback(), Run());
  delegate()->OpenPasswordChangeTab();
}

TEST_P(PasswordChangeDelegateImplTest, UIDelegateCredentialAccessors) {
  CreateDelegate();

  const std::u16string kNewPassword = u"NewSecurePassword";
  EXPECT_CALL(*actuator(), GetGeneratedPassword())
      .WillOnce(Return(
          password_manager::PasswordString(std::u16string(kNewPassword))));

  EXPECT_EQ(delegate()->GetChangePasswordUrl(), GURL(kChangePasswordURL));
  EXPECT_EQ(delegate()->GetUsername(), kTestEmail);
  EXPECT_EQ(delegate()->GetGeneratedPassword().value(), kNewPassword);
}

TEST_P(PasswordChangeDelegateImplTest, OnUiDismissedStopsFlow) {
  CreateDelegate();

  EXPECT_CALL(completion_callback(), Run());
  delegate()->OnUiDismissed();
}

TEST_P(PasswordChangeDelegateImplTest, LoginPasswordFormIsLogged) {
  CreateDelegateAndStartFlow();
  optimization_guide::proto::PasswordChangeQuality quality =
      delegate()
          ->logs_uploader()
          ->GetFinalLog()
          .password_change_submission()
          .quality();
  EXPECT_TRUE(quality.has_login_form_data());
}

TEST_P(PasswordChangeDelegateImplTest,
       RecordsOtpPresentInMainTabHistogram_NoOtpPresent) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "OTP check is skipped when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  base::HistogramTester histogram_tester;
  CreateDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kOtpPresentInMainTabHistogram, false, 1);
}

TEST_P(PasswordChangeDelegateImplTest,
       RecordsOtpPresentInMainTabHistogram_OtpPresent) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "OTP check is skipped when "
                    "PasswordChangeWithPrivateInferenceLoginCheck is enabled.";
  }
  base::HistogramTester histogram_tester;
  const autofill::FormGlobalId form_id = autofill::test::MakeFormGlobalId();
  autofill_client()->GetOtpFieldDetector()->AddFormAndNotifyIfNecessary(
      form_id);

  CreateDelegate();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kOtpPresentInMainTabHistogram, true, 1);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kNoState);

  autofill_client()->GetOtpFieldDetector()->RemoveFormAndNotifyIfNecessary(
      form_id);
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForAgreement);
}
TEST_P(PasswordChangeDelegateImplTest,
       StartPasswordChangeFlow_ReentrantTabDetachDuringUpdateState) {
  if (IsPrivateInferenceLoginCheckEnabled()) {
    GTEST_SKIP() << "StartPasswordChangeFlow() only updates UI state directly "
                    "when PasswordChangeWithPrivateInferenceLoginCheck is "
                    "disabled.";
  }
  CreateDelegate();

  EXPECT_CALL(completion_callback(), Run());
  EXPECT_CALL(
      *mock_ui_controller(),
      UpdateState(
          PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm))
      .WillOnce([this] {
        SimulateTabWillDetach(tabs::TabInterface::DetachReason::kDelete);
      });

  delegate()->StartPasswordChangeFlow();
  EXPECT_FALSE(delegate()->ui_controller());
  EXPECT_FALSE(delegate()->login_checker());

  // A subsequent state transition while `this` awaits `DeleteSoon()` should
  // safely no-op when `ui_controller_` is null.
  delegate()->CancelPasswordChangeFlow();
  EXPECT_EQ(delegate()->GetCurrentState(),
            PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordChangeDelegateImplTest);
