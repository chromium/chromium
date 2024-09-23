// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"

#include <memory>
#include <string>

#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/android/no_passkeys_bottom_sheet_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
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

struct MockJniDelegate : public NoPasskeysBottomSheetBridge::JniDelegate {
  ~MockJniDelegate() override = default;
  MOCK_METHOD(void, Create, (ui::WindowAndroid*), (override));
  MOCK_METHOD(void, Show, (const std::string&), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

}  // namespace

class TouchToFillControllerWebAuthnTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TouchToFillControllerWebAuthnTest() = default;
  ~TouchToFillControllerWebAuthnTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    password_manager_launcher::
        OverrideManagePasswordWhenPasskeysPresentForTesting(false);
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);

    visibility_controller_ = std::make_unique<
        password_manager::MockKeyboardReplacingSurfaceVisibilityController>();
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        profile(), visibility_controller_->AsWeakPtr());
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller().set_view(std::move(mock_view));

    auto jni_delegate = std::make_unique<MockJniDelegate>();
    jni_delegate_ = jni_delegate.get();
    auto no_passkeys_bridge = std::make_unique<NoPasskeysBottomSheetBridge>(
        base::PassKey<class TouchToFillControllerWebAuthnTest>(),
        std::move(jni_delegate));
    touch_to_fill_controller().set_no_passkeys_bridge(
        std::move(no_passkeys_bridge));

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
    window_ = ui::WindowAndroid::CreateForTesting();
    window_.get()->get()->AddChild(web_contents_->GetNativeView());
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

  MockJniDelegate& jni_delegate() { return *jni_delegate_; }

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
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<MockWebAuthnRequestDelegateAndroid> request_delegate_;
  std::unique_ptr<
      password_manager::MockKeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
  raw_ptr<MockJniDelegate> jni_delegate_ = nullptr;
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
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);

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
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);

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
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);

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
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);
  EXPECT_CALL(request_delegate(), ShowHybridSignIn());
  touch_to_fill_controller().OnHybridSignInSelected();
}

TEST_F(TouchToFillControllerWebAuthnTest,
       ShowNoPasskeysSheetIfGpmNotInCredMan) {
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::PARALLEL_WITH_FIDO_2);

  EXPECT_CALL(view(), Show).Times(0);
  EXPECT_CALL(jni_delegate(), Show).Times(1);
  touch_to_fill_controller().Show(
      {}, {},
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/false),
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowNothingIfGpmInCredMan) {
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::FULL_UNLESS_INAPPLICABLE);

  EXPECT_CALL(view(), Show).Times(0);
  EXPECT_CALL(jni_delegate(), Show).Times(0);
  touch_to_fill_controller().Show(
      {}, {},
      MakeTouchToFillControllerDelegate(/*should_show_hybrid_option=*/false),
      /*cred_man_delegate=*/nullptr,
      /*frame_driver=*/nullptr);
}
