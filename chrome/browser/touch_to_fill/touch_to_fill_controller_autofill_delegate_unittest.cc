// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_autofill_delegate.h"

#include <memory>
#include <tuple>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using autofill::mojom::SubmissionReadinessState;
using base::test::RunOnceCallback;
using device_reauth::DeviceAuthRequester;
using device_reauth::MockDeviceAuthenticator;
using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

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
};

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, TouchToFillClosed, (ShowVirtualKeyboard), (override));
  MOCK_METHOD(void, TriggerFormSubmission, (), (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const override));
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD(void,
              Show,
              (const GURL&,
               IsOriginSecure,
               base::span<const UiCredential>,
               base::span<const PasskeyCredential>,
               bool,
               bool),
              (override));
  MOCK_METHOD(void, OnCredentialSelected, (const UiCredential&));
  MOCK_METHOD(void, OnDismiss, ());
};

struct MakeUiCredentialParams {
  base::StringPiece username;
  base::StringPiece password;
  base::StringPiece origin = kExampleCom;
  bool is_public_suffix_match = false;
  bool is_affiliation_based_match = false;
  base::TimeDelta time_since_last_use;
};

UiCredential MakeUiCredential(MakeUiCredentialParams params) {
  return UiCredential(
      base::UTF8ToUTF16(params.username), base::UTF8ToUTF16(params.password),
      url::Origin::Create(GURL(params.origin)),
      IsPublicSuffixMatch(params.is_public_suffix_match),
      IsAffiliationBasedMatch(params.is_affiliation_based_match),
      base::Time::Now() - params.time_since_last_use);
}

}  // namespace

class TouchToFillControllerAutofillTest : public testing::Test {
 protected:
  using UkmBuilder = ukm::builders::TouchToFill_Shown;

  TouchToFillControllerAutofillTest() {
    password_manager_launcher::
        OverrideManagePasswordWhenPasskeysPresentForTesting(false);

    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller().set_view(std::move(mock_view));

    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
    // By default, disable biometric authentication.
    ON_CALL(*authenticator(), CanAuthenticateWithBiometrics)
        .WillByDefault(Return(false));

    // By default, don't trigger a form submission.
    EXPECT_CALL(driver_, TriggerFormSubmission()).Times(0);

    webauthn_credentials_delegate_ =
        std::make_unique<password_manager::MockWebAuthnCredentialsDelegate>();
    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(webauthn_credentials_delegate_.get()));

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kBiometricTouchToFill);
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  MockPasswordManagerClient& client() { return client_; }

  MockTouchToFillView& view() { return *mock_view_; }

  MockDeviceAuthenticator* authenticator() { return authenticator_.get(); }

  ukm::TestAutoSetUkmRecorder& test_recorder() { return test_recorder_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

  std::unique_ptr<TouchToFillControllerAutofillDelegate>
  MakeTouchToFillControllerDelegate(
      autofill::mojom::SubmissionReadinessState submission_readiness) {
    return std::make_unique<TouchToFillControllerAutofillDelegate>(
        base::PassKey<TouchToFillControllerAutofillTest>(), &client_,
        authenticator_, driver().AsWeakPtr(), submission_readiness);
  }

  password_manager::MockWebAuthnCredentialsDelegate*
  webauthn_credentials_delegate() {
    return webauthn_credentials_delegate_.get();
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
  scoped_refptr<MockDeviceAuthenticator> authenticator_ =
      base::MakeRefCounted<MockDeviceAuthenticator>();
  MockPasswordManagerDriver driver_;
  MockPasswordManagerClient client_;
  std::unique_ptr<password_manager::MockWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  TouchToFillController touch_to_fill_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchToFillControllerAutofillTest, Show_And_Fill_No_Auth) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", false, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kCredentialFilled,
      1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillControllerAutofillDelegate::UserAction::
                               kSelectedCredential));
}

TEST_F(TouchToFillControllerAutofillTest, Show_Fill_And_Submit) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/true,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields));

  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), TriggerFormSubmission());
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(Eq(u"alice")));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Show_Fill_And_Dont_Submit) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  EXPECT_CALL(driver(), TriggerFormSubmission()).Times(0);
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(_)).Times(0);

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Dont_Submit_With_Empty_Username) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "", .password = "p4ssw0rd"}),
      MakeUiCredential({.username = "username", .password = "p4ssw0rd"})};

  // As we don't know which credential will be selected, don't disable
  // submission for now.
  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/true,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields));

  // The user picks the credential with an empty username, submission should not
  // be triggered.
  EXPECT_CALL(driver(), TriggerFormSubmission()).Times(0);
  EXPECT_CALL(driver(),
              FillSuggestion(std::u16string(u""), std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest,
       Single_Credential_With_Empty_Username) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "", .password = "p4ssw0rd"})};

  // Only one credential with empty username - submission is impossible.
  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields));

  // Filling doesn't trigger submission.
  EXPECT_CALL(driver(), TriggerFormSubmission()).Times(0);
  EXPECT_CALL(driver(),
              FillSuggestion(std::u16string(u""), std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest, Show_And_Fill_No_Auth_Available) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  EXPECT_CALL(*authenticator(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", false, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillControllerAutofillDelegate::UserAction::
                               kSelectedCredential));
}

TEST_F(TouchToFillControllerAutofillTest,
       Show_And_Fill_Auth_Available_Success) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/true,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kTwoFields));

  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  EXPECT_CALL(*authenticator(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator(),
              Authenticate(DeviceAuthRequester::kTouchToFill, _,
                           /*use_last_valid_auth=*/true))
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(driver(), TriggerFormSubmission());
  EXPECT_CALL(client(), StartSubmissionTrackingAfterTouchToFill(Eq(u"alice")));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerAutofillTest,
       Show_And_Fill_Auth_Available_Failure) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));

  EXPECT_CALL(*authenticator(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator(),
              Authenticate(DeviceAuthRequester::kTouchToFill, _,
                           /*use_last_valid_auth=*/true))
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
          autofill::mojom::SubmissionReadinessState::kNoInformation));
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 0, 1);
}

TEST_F(TouchToFillControllerAutofillTest, Show_Insecure_Origin) {
  EXPECT_CALL(driver(), GetLastCommittedURL())
      .WillOnce(ReturnRefOfCopy(GURL("http://example.com")));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL("http://example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));
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
          .is_affiliation_based_match = true,
          .time_since_last_use = base::Minutes(3),
      }),
  };

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  // Test that we correctly log the presence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"bob"),
                                       std::u16string(u"s3cr3t")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  EXPECT_CALL(*authenticator(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", true, 1);

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
      .is_public_suffix_match = true,
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
      .is_public_suffix_match = true,
      .time_since_last_use = base::Minutes(4),
  });

  UiCredential credentials[] = {alice, bob, charlie, david};
  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   testing::ElementsAre(charlie, alice, bob, david),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));
}

TEST_F(TouchToFillControllerAutofillTest, Dismiss) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));
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
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
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

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {},
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(*authenticator(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator(),
              Authenticate(DeviceAuthRequester::kTouchToFill, _,
                           /*use_last_valid_auth=*/true));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  EXPECT_CALL(*authenticator(), Cancel(DeviceAuthRequester::kTouchToFill));
}

TEST_F(TouchToFillControllerAutofillTest, ShowWebAuthnCredential) {
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  touch_to_fill_controller().set_view(std::move(mock_view));

  PasskeyCredential credential(PasskeyCredential::Source::kAndroidPhone,
                               "example.com", {1, 2, 3, 4}, {5, 6, 7, 8},
                               "alice@example.com");
  std::vector<PasskeyCredential> credentials({credential});

  EXPECT_CALL(*weak_view,
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(std::vector<UiCredential>()),
                   ElementsAreArray(credentials),
                   /*trigger_submission=*/false,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(
          autofill::mojom::SubmissionReadinessState::kNoInformation));

  EXPECT_CALL(*webauthn_credentials_delegate(),
              SelectPasskey(base::Base64Encode(credential.credential_id())));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnPasskeyCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillControllerAutofillDelegate::TouchToFillOutcome::
          kPasskeyCredentialSelected,
      1);
}

class TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest
    : public TouchToFillControllerAutofillTest,
      public testing::WithParamInterface<SubmissionReadinessState> {};

TEST_P(TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest,
       SubmissionReadiness) {
  SubmissionReadinessState submission_readiness = GetParam();

  const UiCredential credential =
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"});
  const UiCredential credentials[] = {credential};

  // If there is no field after the password and both username and password
  // fields are there, then submit the form.
  bool submission_expected =
      submission_readiness == SubmissionReadinessState::kEmptyFields ||
      submission_readiness == SubmissionReadinessState::kMoreThanTwoFields ||
      submission_readiness == SubmissionReadinessState::kTwoFields;
  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/submission_expected,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {}, MakeTouchToFillControllerDelegate(submission_readiness));

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(),
              FillSuggestion(credential.username(), credential.password()));
  EXPECT_CALL(driver(), TriggerFormSubmission())
      .Times(submission_expected ? 1 : 0);

  touch_to_fill_controller().OnCredentialSelected(credential);
}

TEST_P(TouchToFillControllerAutofillTestWithSubmissionReadinessVariationTest,
       SubmissionReadinessMetrics) {
  SubmissionReadinessState submission_readiness = GetParam();

  base::HistogramTester uma_recorder;

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials),
                   ElementsAreArray(std::vector<PasskeyCredential>()),
                   /*trigger_submission=*/_,
                   /*can_manage_passwords_when_passkeys_present*/ false));
  touch_to_fill_controller().Show(
      credentials, {}, MakeTouchToFillControllerDelegate(submission_readiness));

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));
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
