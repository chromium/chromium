// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_android.h"

#include <optional>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace signin_ui_util {

namespace {

const signin_metrics::AccessPoint kTestAccessPoint =
    signin_metrics::AccessPoint::kExtensions;

const signin_metrics::PromoAction kTestPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

const char kTestEmail[] = "test@gmail.com";

const char kTestExtensionName[] = "Test Extension";

GURL GetTestUrl() {
  return GURL("http://example.com");
}

class MockSigninBridge : public SigninBridge {
 public:
  MockSigninBridge() = default;

  MOCK_METHOD(void,
              OpenAccountPickerBottomSheetForExtensions,
              (content::WebContents * web_contents,
               const GURL& continue_url,
               const std::optional<CoreAccountId>& account_id,
               const std::string& extension_name),
              (override));

  MOCK_METHOD(void,
              StartAddAccountFlow,
              (TabAndroid * window,
               const std::string& prefilled_email,
               const GURL& continue_url,
               const std::string& extension_name),
              (override));

  MOCK_METHOD(void,
              StartUpdateCredentialsFlow,
              (TabAndroid * tab,
               const GURL& continue_url,
               const CoreAccountId& account_id),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSigninBridgeForTesting(
    content::BrowserContext* context) {
  return std::make_unique<MockSigninBridge>();
}

}  // namespace

using ::testing::_;
using ::testing::Eq;

class SigninUiDelegateImplAndroidTest : public ChromeRenderViewHostTestHarness {
 protected:
  SigninUiDelegateImplAndroidTest() = default;
  ~SigninUiDelegateImplAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_signin_bridge_ = static_cast<MockSigninBridge*>(
        SigninBridgeFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSigninBridgeForTesting)));
    CHECK(profile());
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

  MockSigninBridge* signin_bridge() { return mock_signin_bridge_; }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  raw_ptr<MockSigninBridge> mock_signin_bridge_ = nullptr;
};

TEST_F(SigninUiDelegateImplAndroidTest, ShowSigninUI) {
  CoreAccountId account_id = identity_test_env()
                                 ->MakePrimaryAccountAvailable(
                                     kTestEmail, signin::ConsentLevel::kSignin)
                                 .account_id;

  NavigateAndCommit(GetTestUrl());
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  tab_model.SetWebContentsList({web_contents()});
  tab_model.SetIsActiveModel(true);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      window_android = ui::WindowAndroid::CreateForTesting();
  window_android->get()->AddChild(web_contents()->GetNativeView());

  signin_ui_util::SigninUiDelegateImplAndroid delegate;

  EXPECT_CALL(
      *signin_bridge(),
      OpenAccountPickerBottomSheetForExtensions(
          _, GetTestUrl(), std::make_optional(account_id), kTestExtensionName));

  delegate.ShowSigninUI(profile(), /*enable_sync=*/true, kTestAccessPoint,
                        kTestPromoAction, kTestExtensionName);

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SigninUiDelegateImplAndroidTest, ShowSigninUINoAccountsOnDevice) {
  NavigateAndCommit(GetTestUrl());
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  tab_model.SetWebContentsList({web_contents()});
  tab_model.SetIsActiveModel(true);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      window_android = ui::WindowAndroid::CreateForTesting();
  window_android->get()->AddChild(web_contents()->GetNativeView());

  signin_ui_util::SigninUiDelegateImplAndroid delegate;

  EXPECT_CALL(
      *signin_bridge(),
      StartAddAccountFlow(_, std::string(), GetTestUrl(), kTestExtensionName));

  delegate.ShowSigninUI(profile(), /*enable_sync=*/true, kTestAccessPoint,
                        kTestPromoAction, kTestExtensionName);

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(SigninUiDelegateImplAndroidTest, ShowReauthUI) {
  CoreAccountId account_id = identity_test_env()
                                 ->MakePrimaryAccountAvailable(
                                     kTestEmail, signin::ConsentLevel::kSignin)
                                 .account_id;
  identity_test_env()->SetInvalidRefreshTokenForAccount(account_id);

  NavigateAndCommit(GetTestUrl());
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  tab_model.SetWebContentsList({web_contents()});
  tab_model.SetIsActiveModel(true);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      window_android = ui::WindowAndroid::CreateForTesting();
  window_android->get()->AddChild(web_contents()->GetNativeView());

  signin_ui_util::SigninUiDelegateImplAndroid delegate;

  EXPECT_CALL(*signin_bridge(),
              StartUpdateCredentialsFlow(_, GetTestUrl(), account_id));

  delegate.ShowReauthUI(profile(), kTestEmail, /*enable_sync=*/true,
                        kTestAccessPoint, kTestPromoAction);

  TabModelList::RemoveTabModel(&tab_model);
}

}  // namespace signin_ui_util
