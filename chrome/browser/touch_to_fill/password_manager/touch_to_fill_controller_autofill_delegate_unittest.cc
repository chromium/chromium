// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_autofill_delegate.h"

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/access_loss/mock_password_access_loss_warning_bridge.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_credential_filler.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webauthn/android/mock_webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ToShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ToShowVirtualKeyboard;
using autofill::mojom::SubmissionReadinessState;
using base::test::RunOnceCallback;
using device_reauth::MockDeviceAuthenticator;
using password_manager::MockPasswordCredentialFiller;
using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;
using ::testing::WithArg;
using webauthn::MockWebAuthnCredManDelegate;
using webauthn::WebAuthnCredManDelegate;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

constexpr char kExampleCom[] = "https://example.com/";

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(void,
              StartSubmissionTrackingAfterTouchToFill,
              (const std::u16string& filled_username),
              (override));
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (password_manager::ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(password_manager::WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (password_manager::PasswordManagerDriver*),
              (override));
  MOCK_METHOD(void, MarkSharedCredentialsAsNotified, (const GURL&), (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD(bool,
              Show,
              (const GURL&,
               IsOriginSecure,
               base::span<const UiCredential>,
               base::span<const PasskeyCredential>,
               int),
              (override));
  MOCK_METHOD(void, OnCredentialSelected, (const UiCredential&));
  MOCK_METHOD(void, OnDismiss, ());
};

struct MakeUiCredentialParams {
  std::string_view username;
  std::string_view password;
  std::string_view origin = kExampleCom;
  password_manager_util::GetLoginMatchType match_type =
      password_manager_util::GetLoginMatchType::kExact;
  base::TimeDelta time_since_last_use;
};

UiCredential MakeUiCredential(MakeUiCredentialParams params) {
  return UiCredential(
      base::UTF8ToUTF16(params.username), base::UTF8ToUTF16(params.password),
      url::Origin::Create(GURL(params.origin)), params.match_type,
      base::Time::Now() - params.time_since_last_use);
}

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class TouchToFillControllerAutofillTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  using UkmBuilder = ukm::builders::TouchToFill_Shown;

  TouchToFillControllerAutofillTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    password_manager_launcher::
        OverrideManagePasswordWhenPasskeysPresentForTesting(false);

    // By default, disable biometric authentication.
    ON_CALL(client(), IsReauthBeforeFillingRequired)
        .WillByDefault(Return(false));

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kBiometricTouchToFill);
  }

  std::unique_ptr<MockPasswordCredentialFiller> CreateMockFiller() {
    std::unique_ptr<MockPasswordCredentialFiller> filler =
        std::make_unique<MockPasswordCredentialFiller>();
    // the Mock filler will be passed to TouchToFillControllerAutofillDelegate.
    // cache the raw pointer here to interact with the mock after passing.
    weak_filler_ = filler.get();
    ON_CALL(*filler, GetFrameUrl()).WillByDefault(Return(GURL(kExampleCom)));
    return filler;
  }

  MockPasswordManagerClient& client() { return client_; }

  MockTouchToFillView& view() { return *mock_view_; }

  MockPasswordCredentialFiller* last_mock_filler() {
    EXPECT_NE(weak_filler_, nullptr) << "Call CreateMockFiller first!";
    return weak_filler_;
  }

  MockDeviceAuthenticator* authenticator() { return authenticator_.get(); }

  ukm::TestAutoSetUkmRecorder& test_recorder() { return test_recorder_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TouchToFillController& touch_to_fill_controller() {
    return *touch_to_fill_controller_;
  }

  base::MockCallback<base::RepeatingCallback<
      void(gfx::NativeWindow,
           Profile*,
           password_manager::metrics_util::PasswordMigrationWarningTriggers)>>&
  show_password_migration_warning() {
    return show_password_migration_warning_;
  }

  std::unique_ptr<TouchToFillControllerAutofillDelegate>
  MakeTouchToFillControllerDelegate(
      autofill::mojom::SubmissionReadinessState submission_readiness,
      std::unique_ptr<MockPasswordCredentialFiller> filler,
      const password_manager::PasswordForm* form_to_fill,
      autofill::FieldRendererId focused_field_renderer_id,
      TouchToFillControllerAutofillDelegate::ShowHybridOption
          should_show_hybrid_option) {
    ON_CALL(*filler, GetSubmissionReadinessState())
        .WillByDefault(Return(submission_readiness));
    std::unique_ptr<MockPasswordAccessLossWarningBridge> mock_bridge =
        std::make_unique<MockPasswordAccessLossWarningBridge>();
    mock_access_loss_warning_bridge_ = mock_bridge.get();
    return std::make_unique<TouchToFillControllerAutofillDelegate>(
        base::PassKey<TouchToFillControllerAutofillTest>(), &client_,
        web_contents(), std::move(authenticator_),
        webauthn_credentials_delegate_.AsWeakPtr(), std::move(filler),
        form_to_fill, focused_field_renderer_id, should_show_hybrid_option,
        show_password_migration_warning().Get(), std::move(mock_bridge));
  }

  password_manager::MockWebAuthnCredentialsDelegate&
  webauthn_credentials_delegate() {
    return webauthn_credentials_delegate_;
  }

  password_manager::MockKeyboardReplacingSurfaceVisibilityController&
  visibility_controller() {
    return *visibility_controller_;
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  const password_manager::PasswordForm* form_to_fill() {
    return &form_to_fill_;
  }

  MockPasswordAccessLossWarningBridge* mock_access_loss_warning_bridge() {
    return mock_access_loss_warning_bridge_;
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    visibility_controller_ = std::make_unique<
        password_manager::MockKeyboardReplacingSurfaceVisibilityController>();
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        profile(), visibility_controller_->AsWeakPtr());
    touch_to_fill_controller().set_view(std::move(mock_view));
    form_to_fill_.password_element_renderer_id = autofill::FieldRendererId(1);
    form_to_fill_.username_element_renderer_id = autofill::FieldRendererId(2);
  }

 private:
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
  std::unique_ptr<MockDeviceAuthenticator> authenticator_ =
      std::make_unique<MockDeviceAuthenticator>();
  MockPasswordManagerClient client_;
  password_manager::MockWebAuthnCredentialsDelegate
      webauthn_credentials_delegate_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  std::unique_ptr<
      password_manager::MockKeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockCallback<base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      show_password_migration_warning_;
  raw_ptr<MockPasswordAccessLossWarningBridge> mock_access_loss_warning_bridge_;
  raw_ptr<MockPasswordCredentialFiller> weak_filler_;
  password_manager::PasswordForm form_to_fill_;
};

TEST_F(TouchToFillControllerAutofillTest, Show_Fill_And_Submit) {
  auto filler_to_pass = CreateMockFiller();
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(true));

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kTriggerSubmission));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));

  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(Eq(u"alice")));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Show_Fill_And_Dont_Submit) {
  auto filler_to_pass = CreateMockFiller();
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(false));

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));

  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest,
       ShowFillAndShowPasswordMigrationWarning) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsMigrationWarning},
      {});
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  auto filler_to_pass = CreateMockFiller();

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(false));
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);
  EXPECT_CALL(show_password_migration_warning(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kTouchToFill));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, FillingShowsAccessLossWarning) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  auto filler_to_pass = CreateMockFiller();

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(false));
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet(
                  profile()->GetPrefs(), _, profile(),
                  /*called_at_startup=*/false,
                  password_manager_android_util::
                      PasswordAccessLossWarningTriggers::kTouchToFill));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Dont_Submit_With_Empty_Username) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "", .password = "p4ssw0rd"}),
      MakeUiCredential({.username = "username", .password = "p4ssw0rd"})};
  auto filler_to_pass = CreateMockFiller();
  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(true));

  // As we don't know which credential will be selected, don't disable
  // submission for now.
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kTriggerSubmission));
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(true));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(false));
  // The user picks the credential with an empty username, submission should not
  // be triggered.
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(false));
  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u""),
                                      std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest,
       Single_Credential_With_Empty_Username) {
  auto filler_to_pass = CreateMockFiller();
  UiCredential credentials[] = {
      MakeUiCredential({.username = "", .password = "p4ssw0rd"})};
  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(false));

  // Only one credential with empty username - submission is impossible.
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u""),
                                      std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(*last_mock_filler(), UpdateTriggerSubmission(false));
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Show_And_Fill_No_Auth_Available) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));
  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(false));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillControllerAutofillDelegate::UserAction::
                               kSelectedCredential));
}

TEST_F(TouchToFillControllerAutofillTest,
       Show_And_Fill_Auth_Available_Success) {
  auto filler_to_pass = CreateMockFiller();
  auto* authenticator_ptr = authenticator();
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(true));

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kTriggerSubmission));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  ON_CALL(*last_mock_filler(), ShouldTriggerSubmission())
      .WillByDefault(Return(true));
  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"alice"),
                                      std::u16string(u"p4ssw0rd")));

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(Eq(u"alice")));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest,
       Show_And_Fill_Auth_Available_Failure) {
  auto* authenticator_ptr = authenticator();
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(), FillUsernameAndPassword(_, _)).Times(0);

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kReauthenticationFailed,
      1);
}

TEST_F(TouchToFillControllerAutofillTest, Show_Empty) {
  EXPECT_CALL(view(), Show).Times(0);
  touch_to_fill_controller().Show(
      {}, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest, Show_Insecure_Origin) {
  auto filler_to_pass = CreateMockFiller();
  EXPECT_CALL(*last_mock_filler(), GetFrameUrl())
      .WillOnce(Return(GURL("http://example.com")));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL("http://example.com")),
                           IsOriginSecure(false), ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          std::move(filler_to_pass), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  UiCredential credentials[] = {
      MakeUiCredential({
          .username = "alice",
          .password = "p4ssw0rd",
          .time_since_last_use = base::Minutes(2),
      }),
      MakeUiCredential({
          .username = "bob",
          .password = "s3cr3t",
          .origin = "",
          .match_type = password_manager_util::GetLoginMatchType::kAffiliated,
          .time_since_last_use = base::Minutes(3),
      }),
  };

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  // Test that we correctly log the presence of an Android credential.
  EXPECT_CALL(*last_mock_filler(),
              FillUsernameAndPassword(std::u16string(u"bob"),
                                      std::u16string(u"s3cr3t")));
  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(false));
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillControllerAutofillDelegate::UserAction::
                               kSelectedCredential));
}

// Verify that the credentials are ordered by their PSL match bit and last
// time used before being passed to the view.
TEST_F(TouchToFillControllerAutofillTest, Show_Orders_Credentials) {
  auto alice = MakeUiCredential({
      .username = "alice",
      .password = "p4ssw0rd",
      .time_since_last_use = base::Minutes(3),
  });
  auto bob = MakeUiCredential({
      .username = "bob",
      .password = "s3cr3t",
      .match_type = password_manager_util::GetLoginMatchType::kPSL,
      .time_since_last_use = base::Minutes(1),
  });
  auto charlie = MakeUiCredential({
      .username = "charlie",
      .password = "very_s3cr3t",
      .time_since_last_use = base::Minutes(2),
  });
  auto david = MakeUiCredential({
      .username = "david",
      .password = "even_more_s3cr3t",
      .match_type = password_manager_util::GetLoginMatchType::kPSL,
      .time_since_last_use = base::Minutes(4),
  });

  UiCredential credentials[] = {alice, bob, charlie, david};
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           testing::ElementsAre(charlie, alice, bob, david),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->username_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest, Dismiss) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(client(), MarkSharedCredentialsAsNotified(GURL(kExampleCom)));
  EXPECT_CALL(*last_mock_filler(), Dismiss(ToShowVirtualKeyboard(true)));
  touch_to_fill_controller().OnDismiss();

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillControllerAutofillDelegate::UserAction::kDismissed));
  histogram_tester().ExpectUniqueSample("PasswordManager.TouchToFill.Outcome",
                                        TouchToFillControllerAutofillDelegate::
                                            TouchToFillOutcome::kSheetDismissed,
                                        1);
}

TEST_F(TouchToFillControllerAutofillTest, ManagePasswordsSelected) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(client(), MarkSharedCredentialsAsNotified(GURL(kExampleCom)));
  EXPECT_CALL(*last_mock_filler(), Dismiss(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(client(),
              NavigateToManagePasswordsPage(
                  password_manager::ManagePasswordsReferrer::kTouchToFill));

  touch_to_fill_controller().OnManagePasswordsSelected(
      /*passkeys_shown=*/false);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kManagePasswordsSelected,
      1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillControllerAutofillDelegate::UserAction::
                               kSelectedManagePasswords));
}

TEST_F(TouchToFillControllerAutofillTest, DestroyedWhileAuthRunning) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  auto* authenticator_ptr = authenticator();

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage);
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  EXPECT_CALL(*authenticator_ptr, Cancel());
}

TEST_F(TouchToFillControllerAutofillTest, ShowWebAuthnCredential) {
  PasskeyCredential credential(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("example.com"),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId({5, 6, 7, 8}),
      PasskeyCredential::Username("alice@example.com"));
  std::vector<PasskeyCredential> credentials({credential});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(std::vector<UiCredential>()),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(webauthn_credentials_delegate(),
              SelectPasskey(base::Base64Encode(credential.credential_id()), _));
  EXPECT_CALL(*last_mock_filler(), Dismiss(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(*last_mock_filler(), FillUsernameAndPassword(_, _)).Times(0);
  touch_to_fill_controller().OnPasskeyCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kPasskeyCredentialSelected,
      1);
}

TEST_F(TouchToFillControllerAutofillTest, ShowAndSelectHybrid) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kShouldShowHybridOption));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(true)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(webauthn_credentials_delegate(), LaunchSecurityKeyOrHybridFlow());
  touch_to_fill_controller().OnHybridSignInSelected();
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kHybridSignInSelected,
      1);
}

TEST_F(TouchToFillControllerAutofillTest, ShowCredManEntryIfThereArePasskeys) {
  const UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  MockWebAuthnCredManDelegate cred_man_delegate;
  EXPECT_CALL(cred_man_delegate, HasPasskeys())
      .Times(2)
      .WillRepeatedly(Return(WebAuthnCredManDelegate::State::kHasPasskeys));
  EXPECT_CALL(cred_man_delegate, SetRequestCompletionCallback(_));
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           TouchToFillView::kShouldShowCredManEntry));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      &cred_man_delegate, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest,
       ShowCredManImmediatelyIfNoGpmPasskeys) {
  MockWebAuthnCredManDelegate cred_man_delegate;
  ON_CALL(cred_man_delegate, HasPasskeys())
      .WillByDefault(Return(WebAuthnCredManDelegate::State::kHasPasskeys));
  EXPECT_CALL(
      cred_man_delegate,
      TriggerCredManUi(WebAuthnCredManDelegate::RequestPasswords(false)));
  EXPECT_CALL(view(), Show(_, _, _, _, _)).Times(0);
  touch_to_fill_controller().Show(
      {}, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      &cred_man_delegate, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest, NoCredManEntryIfNoPasskeys) {
  const UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  MockWebAuthnCredManDelegate cred_man_delegate;
  EXPECT_CALL(cred_man_delegate, HasPasskeys()).Times(2);
  EXPECT_CALL(cred_man_delegate, SetRequestCompletionCallback(_)).Times(0);
  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()), 0));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      &cred_man_delegate, /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerAutofillTest,
       ShouldNotShowTouchToFillForNewPasswordField) {
  base::test::ScopedFeatureList enabledFeatures(
      password_manager::features::kPasswordSuggestionBottomSheetV2);

  const UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};
  MockWebAuthnCredManDelegate cred_man_delegate;
  password_manager::PasswordForm change_password_form(*form_to_fill());
  change_password_form.new_password_element_renderer_id =
      autofill::FieldRendererId(3);
  bool is_shown = touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation,
          CreateMockFiller(), &change_password_form,
          change_password_form.new_password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      &cred_man_delegate, /*frame_driver=*/nullptr);
  EXPECT_FALSE(is_shown);
}

class TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest
    : public TouchToFillControllerAutofillTest,
      public testing::WithParamInterface<SubmissionReadinessState> {};

TEST_P(TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest,
       SubmissionReadinessMetrics) {
  SubmissionReadinessState submission_readiness = GetParam();
  base::HistogramTester uma_recorder;

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           ElementsAreArray(std::vector<PasskeyCredential>()),
                           /*flags=*/_));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          submission_readiness, CreateMockFiller(), form_to_fill(),
          form_to_fill()->password_element_renderer_id,
          TouchToFillControllerAutofillDelegate::ShowHybridOption(false)),
      /*cred_man_delegate=*/nullptr, /*frame_driver=*/nullptr);

  EXPECT_CALL(*last_mock_filler(), Dismiss(ToShowVirtualKeyboard(true)));
  touch_to_fill_controller().OnDismiss();

  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SubmissionReadiness", submission_readiness,
      1);

  auto entries = test_recorder().GetEntriesByName(
      ukm::builders::TouchToFill_SubmissionReadiness::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0],
      ukm::builders::TouchToFill_SubmissionReadiness::kSubmissionReadinessName,
      static_cast<int64_t>(submission_readiness));
}

INSTANTIATE_TEST_SUITE_P(
    SubmissionReadinessVariation,
    TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest,
    testing::Values(SubmissionReadinessState::kNoInformation,
                    SubmissionReadinessState::kError,
                    SubmissionReadinessState::kNoUsernameField,
                    SubmissionReadinessState::kFieldBetweenUsernameAndPassword,
                    SubmissionReadinessState::kFieldAfterPasswordField,
                    SubmissionReadinessState::kEmptyFields,
                    SubmissionReadinessState::kMoreThanTwoFields,
                    SubmissionReadinessState::kTwoFields,
                    SubmissionReadinessState::kNoPasswordField));
