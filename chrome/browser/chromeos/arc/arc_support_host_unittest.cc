// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_support_host.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/arc/extensions/fake_arc_support.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using arc::FakeArcSupport;
using testing::Eq;
using testing::StrictMock;
using testing::_;

namespace {

constexpr char kFakeActiveDirectoryPrefix[] = "fake-prefix";
constexpr char kFakeFederationUrl[] = "http://example.com/adfs/ls/awesome.aspx";

class MockAuthDelegateNonStrict : public ArcSupportHost::AuthDelegate {
 public:
  MOCK_METHOD0(OnAuthSucceeded, void());
  MOCK_METHOD1(OnAuthFailed, void(const std::string& error_msg));
  MOCK_METHOD0(OnAuthRetryClicked, void());
};

using MockAuthDelegate = StrictMock<MockAuthDelegateNonStrict>;

class MockTermsOfServiceDelegateNonStrict
    : public ArcSupportHost::TermsOfServiceDelegate {
 public:
  MOCK_METHOD3(OnTermsAgreed,
               void(bool is_metrics_enabled,
                    bool is_backup_and_restore_enabled,
                    bool is_location_service_enabled));
  MOCK_METHOD0(OnTermsRejected, void());
  MOCK_METHOD0(OnTermsRetryClicked, void());
};

using MockTermsOfServiceDelegate =
    StrictMock<MockTermsOfServiceDelegateNonStrict>;

class MockErrorDelegateNonStrict : public ArcSupportHost::ErrorDelegate {
 public:
  MOCK_METHOD0(OnWindowClosed, void());
  MOCK_METHOD0(OnRetryClicked, void());
  MOCK_METHOD0(OnSendFeedbackClicked, void());
};

using MockErrorDelegate = StrictMock<MockErrorDelegateNonStrict>;

}  // namespace

class ArcSupportHostTest : public BrowserWithTestWindowTest {
 public:
  ArcSupportHostTest() = default;
  ~ArcSupportHostTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()),
        "testing@account.com");

    support_host_ = std::make_unique<ArcSupportHost>(profile());
    fake_arc_support_ = std::make_unique<FakeArcSupport>(support_host_.get());
  }

  void TearDown() override {
    support_host_->SetAuthDelegate(nullptr);
    support_host_->SetTermsOfServiceDelegate(nullptr);
    support_host_->SetErrorDelegate(nullptr);

    fake_arc_support_.reset();
    support_host_.reset();
    user_manager_enabler_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  ArcSupportHost* support_host() { return support_host_.get(); }
  FakeArcSupport* fake_arc_support() { return fake_arc_support_.get(); }

  MockAuthDelegate* CreateMockAuthDelegate() {
    auth_delegate_ = std::make_unique<MockAuthDelegate>();
    support_host_->SetAuthDelegate(auth_delegate_.get());
    return auth_delegate_.get();
  }

  MockTermsOfServiceDelegate* CreateMockTermsOfServiceDelegate() {
    tos_delegate_ = std::make_unique<MockTermsOfServiceDelegate>();
    support_host_->SetTermsOfServiceDelegate(tos_delegate_.get());
    return tos_delegate_.get();
  }

  MockErrorDelegate* CreateMockErrorDelegate() {
    error_delegate_ = std::make_unique<MockErrorDelegate>();
    support_host_->SetErrorDelegate(error_delegate_.get());
    return error_delegate_.get();
  }

  void ShowAuthPage() {
    // Currently there is only Active Directory sign-in that provides an
    // authorization inside the OptIn flow.
    support_host()->ShowActiveDirectoryAuth(GURL(kFakeFederationUrl),
                                            kFakeActiveDirectoryPrefix);
  }

  consent_auditor::FakeConsentAuditor* consent_auditor() {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{ConsentAuditorFactory::GetInstance(),
             base::BindRepeating(&BuildFakeConsentAuditor)}};
  }

 private:
  std::unique_ptr<ArcSupportHost> support_host_;
  std::unique_ptr<FakeArcSupport> fake_arc_support_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  std::unique_ptr<MockAuthDelegate> auth_delegate_;
  std::unique_ptr<MockTermsOfServiceDelegate> tos_delegate_;
  std::unique_ptr<MockErrorDelegate> error_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHostTest);
};

namespace {

TEST_F(ArcSupportHostTest, AuthSucceeded) {
  MockAuthDelegate* auth_delegate = CreateMockAuthDelegate();
  support_host()->SetAuthDelegate(auth_delegate);

  ShowAuthPage();

  EXPECT_CALL(*auth_delegate, OnAuthSucceeded());
  fake_arc_support()->EmulateAuthSuccess();
}

TEST_F(ArcSupportHostTest, AuthFailed) {
  constexpr char kFakeError[] = "fake_error";
  MockAuthDelegate* auth_delegate = CreateMockAuthDelegate();
  support_host()->SetAuthDelegate(auth_delegate);

  ShowAuthPage();

  EXPECT_CALL(*auth_delegate, OnAuthFailed(kFakeError));
  fake_arc_support()->EmulateAuthFailure(kFakeError);
}

TEST_F(ArcSupportHostTest, AuthRetryOnError) {
  // Auth error can only happen after terms accepted.
  MockAuthDelegate* auth_delegate = CreateMockAuthDelegate();
  support_host()->SetAuthDelegate(auth_delegate);
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowError(ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR,
                            false /* should_show_send_feedback */);

  EXPECT_CALL(*auth_delegate, OnAuthRetryClicked());
  EXPECT_CALL(*error_delegate, OnWindowClosed()).Times(0);
  fake_arc_support()->ClickRetryButton();
}

TEST_F(ArcSupportHostTest, TermsOfServiceAccept) {
  consent_auditor::FakeConsentAuditor* ca = consent_auditor();
  EXPECT_CALL(*ca, RecordArcPlayConsent(_, _));
  EXPECT_CALL(*ca, RecordArcBackupAndRestoreConsent(_, _));
  EXPECT_CALL(*ca, RecordArcGoogleLocationServiceConsent(_, _));

  MockTermsOfServiceDelegate tos_delegate;
  support_host()->SetTermsOfServiceDelegate(&tos_delegate);

  support_host()->ShowTermsOfService();
  fake_arc_support()->set_metrics_mode(false);  // just to add some diversity
  fake_arc_support()->set_backup_and_restore_mode(true);
  fake_arc_support()->set_location_service_mode(true);

  EXPECT_CALL(tos_delegate, OnTermsAgreed(false, true, true));
  fake_arc_support()->ClickAgreeButton();
}

TEST_F(ArcSupportHostTest, TermsOfServiceRejectOrCloseWindow) {
  MockTermsOfServiceDelegate* tos_delegate = CreateMockTermsOfServiceDelegate();
  support_host()->SetTermsOfServiceDelegate(tos_delegate);
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowTermsOfService();

  EXPECT_CALL(*tos_delegate, OnTermsRejected());
  EXPECT_CALL(*error_delegate, OnWindowClosed()).Times(0);
  // Rejecting ToS and closing the window when ToS is showing is the same thing.
  fake_arc_support()->Close();
}

TEST_F(ArcSupportHostTest, TermsOfServiceRetryOnError) {
  MockTermsOfServiceDelegate* tos_delegate = CreateMockTermsOfServiceDelegate();
  support_host()->SetTermsOfServiceDelegate(tos_delegate);
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowError(ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR,
                            false /* should_show_send_feedback */);

  EXPECT_CALL(*tos_delegate, OnTermsRetryClicked());
  EXPECT_CALL(*error_delegate, OnWindowClosed()).Times(0);
  fake_arc_support()->ClickRetryButton();
}

TEST_F(ArcSupportHostTest, CloseWindowDuringManualAuth) {
  // No TermsOfServiceDelegate since it is not ongoing.
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);
  MockAuthDelegate* auth_delegate = CreateMockAuthDelegate();
  support_host()->SetAuthDelegate(auth_delegate);

  support_host()->ShowArcLoading();

  EXPECT_CALL(*error_delegate, OnWindowClosed());
  fake_arc_support()->Close();
}

TEST_F(ArcSupportHostTest, CloseWindowWithoutTermsOfServiceOrAuthOnging) {
  // No TermsOfServiceDelegate since it is not ongoing.
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowArcLoading();

  EXPECT_CALL(*error_delegate, OnWindowClosed());
  fake_arc_support()->Close();
}

TEST_F(ArcSupportHostTest, RetryOnGeneralError) {
  // No TermsOfServiceDelegate and AuthDelegate since it is not ongoing.
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowError(ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR,
                            false /* should_show_send_feedback */);

  EXPECT_CALL(*error_delegate, OnRetryClicked());
  fake_arc_support()->ClickRetryButton();
}

TEST_F(ArcSupportHostTest, SendFeedbackOnError) {
  MockErrorDelegate* error_delegate = CreateMockErrorDelegate();
  support_host()->SetErrorDelegate(error_delegate);

  support_host()->ShowError(ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR,
                            true /* should_show_send_feedback */);

  EXPECT_CALL(*error_delegate, OnSendFeedbackClicked());
  fake_arc_support()->ClickSendFeedbackButton();
}

}  // namespace
