// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_webauthn_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using IsOriginSecure = TouchToFillView::IsOriginSecure;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;

constexpr char kRpId[] = "example.com";
constexpr char kExampleCom[] = "https://example.com/";
const std::vector<uint8_t> kCredentialId1 = {'a', 'b', 'c', 'd'};
const std::vector<uint8_t> kCredentialId2 = {'e', 'f', 'g', 'h'};
const std::vector<uint8_t> kUserId1 = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "John.Doe@example.com";

PasskeyCredential CreatePasskey(
    std::vector<uint8_t> credential_id = kCredentialId1,
    std::vector<uint8_t> user_id = kUserId1,
    std::string username = kUserName1) {
  return PasskeyCredential(
      PasskeyCredential::Source::kAndroidPhone, PasskeyCredential::RpId(kRpId),
      PasskeyCredential::CredentialId(std::move(credential_id)),
      PasskeyCredential::UserId(std::move(user_id)),
      PasskeyCredential::Username(std::move(username)));
}

class MockWebAuthnRequestDelegateAndroid
    : public WebAuthnRequestDelegateAndroid {
 public:
  explicit MockWebAuthnRequestDelegateAndroid(
      content::WebContents* web_contents)
      : WebAuthnRequestDelegateAndroid(web_contents) {}
  ~MockWebAuthnRequestDelegateAndroid() override = default;

  MOCK_METHOD(void,
              OnWebAuthnAccountSelected,
              (const std::vector<uint8_t>& id),
              (override));
  MOCK_METHOD(void, ShowHybridSignIn, (), (override));
};

struct MockTouchToFillView : public TouchToFillView {
  MOCK_METHOD(void,
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

class TouchToFillControllerWebAuthnTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillControllerWebAuthnTest() = default;
  ~TouchToFillControllerWebAuthnTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    password_manager_launcher::
        OverrideManagePasswordWhenPasskeysPresentForTesting(false);

    visibility_controller_ = std::make_unique<
        password_manager::MockKeyboardReplacingSurfaceVisibilityController>();
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        visibility_controller_->AsWeakPtr());
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller().set_view(std::move(mock_view));

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
    web_contents_tester()->NavigateAndCommit(GURL(kExampleCom));

    request_delegate_ = std::make_unique<MockWebAuthnRequestDelegateAndroid>(
        web_contents_.get());
  }

  void TearDown() override {
    request_delegate_.reset();
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockWebAuthnRequestDelegateAndroid& request_delegate() {
    return *request_delegate_;
  }

  MockTouchToFillView& view() { return *mock_view_; }

  TouchToFillController& touch_to_fill_controller() {
    return *touch_to_fill_controller_;
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents_.get());
  }

  std::unique_ptr<TouchToFillControllerWebAuthnDelegate>
  MakeTouchToFillControllerDelegate(bool should_show_hybrid_option) {
    return std::make_unique<TouchToFillControllerWebAuthnDelegate>(
        request_delegate_.get(), should_show_hybrid_option);
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MockWebAuthnRequestDelegateAndroid> request_delegate_;
  std::unique_ptr<
      password_manager::MockKeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
};

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectCredential) {
  std::vector<PasskeyCredential> credentials{CreatePasskey()};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(std::vector<UiCredential>()),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/false),
      /*render_widget_host=*/nullptr);

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(kCredentialId1));
  touch_to_fill_controller().OnPasskeyCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectWithMultipleCredential) {
  std::vector<PasskeyCredential> credentials(
      {CreatePasskey(kCredentialId1), CreatePasskey(kCredentialId2)});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(std::vector<UiCredential>()),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/false),
      /*render_widget_host=*/nullptr);

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(kCredentialId2));
  touch_to_fill_controller().OnPasskeyCredentialSelected(credentials[1]);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndCancel) {
  std::vector<PasskeyCredential> credentials({CreatePasskey()});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(std::vector<UiCredential>()),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/false),
      /*render_widget_host=*/nullptr);

  EXPECT_CALL(request_delegate(),
              OnWebAuthnAccountSelected(std::vector<uint8_t>()));
  touch_to_fill_controller().Close();
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectHybrid) {
  std::vector<PasskeyCredential> credentials({CreatePasskey()});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(std::vector<UiCredential>()),
                           ElementsAreArray(credentials),
                           TouchToFillView::kShouldShowHybridOption));
  touch_to_fill_controller().Show(
      {}, credentials,
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/true),
      /*render_widget_host=*/nullptr);
  EXPECT_CALL(request_delegate(), ShowHybridSignIn());
  touch_to_fill_controller().OnHybridSignInSelected();
}

}  // namespace
