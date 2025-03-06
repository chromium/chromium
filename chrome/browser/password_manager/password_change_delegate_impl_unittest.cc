// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::Return;

const char kChangePasswordURL[] = "https://example.com/password/";
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

}  // namespace

class PasswordChangeDelegateImplTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangeDelegateImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PasswordChangeDelegateImplTest() override = default;

  std::unique_ptr<PasswordChangeDelegateImpl> CreateDelegate() {
    auto delegate = std::make_unique<PasswordChangeDelegateImpl>(
        GURL(kChangePasswordURL), kTestEmail, kPassword, web_contents());
    delegate->SetNavigator(&navigator_);
    delegate->OfferPasswordChangeUi();
    return delegate;
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        web_contents()->GetBrowserContext(), nullptr);
    // `ChromePasswordManagerClient` observes `AutofillManager`s, so
    // `ChromeAutofillClient` needs to be set up, too.
    autofill::ChromeAutofillClient::CreateForWebContents(contents.get());
    ChromePasswordManagerClient::CreateForWebContents(contents.get());
    return contents;
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  PrefService* prefs() { return profile()->GetPrefs(); }
  MockPageNavigator& navigator() { return navigator_; }

 private:
  MockPageNavigator navigator_;
};

TEST_F(PasswordChangeDelegateImplTest, WaitingForAgreement) {
  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();

  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  delegate->StartPasswordChangeFlow();

  ASSERT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement));

  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForAgreement,
            delegate->GetCurrentState());

  delegate->OnPrivacyNoticeAccepted();
  // Both pref and state reflect acceptance.
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement));
  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());
}

TEST_F(PasswordChangeDelegateImplTest, PasswordChangeFormNotFound) {
  base::HistogramTester histogram_tester;
  prefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);

  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();
  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  delegate->StartPasswordChangeFlow();

  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());

  FastForwardBy(ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  EXPECT_EQ(PasswordChangeDelegate::State::kChangePasswordFormNotFound,
            delegate->GetCurrentState());
  delegate.reset();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kChangePasswordFormNotFound, 1);
}

TEST_F(PasswordChangeDelegateImplTest, RestartPasswordChange) {
  prefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);

  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();
  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  delegate->StartPasswordChangeFlow();

  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());

  FastForwardBy(ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);

  EXPECT_EQ(PasswordChangeDelegate::State::kChangePasswordFormNotFound,
            delegate->GetCurrentState());

  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  delegate->Restart();
  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());
}

TEST_F(PasswordChangeDelegateImplTest, MetricsReportedFlowOffered) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();
  delegate.reset();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kOfferingPasswordChange, 1);
}

TEST_F(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledInPrivacyNotice) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();
  delegate->StartPasswordChangeFlow();

  delegate.reset();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kWaitingForAgreement, 1);
}

TEST_F(PasswordChangeDelegateImplTest,
       MetricsReportedFlowCanceledDuringSignInCheck) {
  prefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegate> delegate = CreateDelegate();
  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  delegate->StartPasswordChangeFlow();
  delegate.reset();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kWaitingForChangePasswordForm, 1);
}

TEST_F(PasswordChangeDelegateImplTest,
       MetricsReportedWasPasswordChangeNewTabFocused) {
  prefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> test_web_contents = CreateWebContents();
  std::unique_ptr<PasswordChangeDelegateImpl> delegate = CreateDelegate();
  EXPECT_CALL(navigator(), OpenURL).WillOnce(Return(test_web_contents.get()));
  static_cast<PasswordChangeDelegate*>(delegate.get())
      ->StartPasswordChangeFlow();
  static_cast<content::WebContentsObserver*>(delegate.get())
      ->OnVisibilityChanged(content::Visibility::VISIBLE);
  delegate.reset();
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kWasPasswordChangeNewTabFocused, true, 1);
}
