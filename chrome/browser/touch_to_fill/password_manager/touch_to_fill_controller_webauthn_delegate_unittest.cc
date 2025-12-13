// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/android/no_passkeys_bottom_sheet_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "chrome/browser/webauthn/android/credential_sorter_android.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/browser/webauthn/shared_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using Credential = TouchToFillView::Credential;
using IsOriginSecure = TouchToFillView::IsOriginSecure;
using SortingCallback = TouchToFillControllerWebAuthnDelegate::SortingCallback;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;

constexpr char kRpId[] = "example.com";
constexpr char kExampleCom[] = "https://example.com/";
const std::vector<uint8_t> kCredentialId1 = {'a', 'b', 'c', 'd'};
const std::vector<uint8_t> kCredentialId2 = {'e', 'f', 'g', 'h'};
const std::vector<uint8_t> kUserId1 = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "John.Doe@example.com";
const std::u16string kUserName2 = u"Jane.Doe@example.com";
const std::u16string kPassword = u"hunter2";

PasskeyCredential CreatePasskey(
    std::vector<uint8_t> credential_id = kCredentialId1,
    std::vector<uint8_t> user_id = kUserId1,
    std::string username = kUserName1,
    std::optional<base::Time> last_used = std::nullopt) {
  return PasskeyCredential(
      PasskeyCredential::Source::kAndroidPhone, PasskeyCredential::RpId(kRpId),
      PasskeyCredential::CredentialId(std::move(credential_id)),
      PasskeyCredential::UserId(std::move(user_id)),
      PasskeyCredential::Username(std::move(username)),
      PasskeyCredential::DisplayName(""),
      /* creation_time= */ std::nullopt, last_used);
}

UiCredential CreatePasswordCredential(
    std::u16string username = kUserName2,
    std::u16string password = kPassword,
    std::string origin = kExampleCom,
    std::optional<base::Time> last_used = std::nullopt) {
  return UiCredential(username, password, url::Origin::Create(GURL(origin)),
                      std::string(origin),
                      password_manager_util::GetLoginMatchType::kExact,
                      last_used.value_or(base::Time::Now() - base::Minutes(2)),
                      UiCredential::IsBackupCredential(false));
}

class MockCredentialReceiver
    : public TouchToFillControllerWebAuthnDelegate::CredentialReceiver {
 public:
  explicit MockCredentialReceiver(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  virtual ~MockCredentialReceiver() = default;

  MOCK_METHOD(void,
              OnWebAuthnAccountSelected,
              (const std::vector<uint8_t>& id),
              (override));
  MOCK_METHOD(void,
              OnPasswordCredentialSelected,
              (const PasswordCredentialPair& password_credential),
              (override));
  MOCK_METHOD(void, OnCredentialSelectionDeclined, (), (override));
  MOCK_METHOD(void, OnHybridSignInSelected, (), (override));

  content::WebContents* web_contents() override { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

struct MockTouchToFillView : public TouchToFillView {
  MOCK_METHOD(bool,
              Show,
              (const GURL&, IsOriginSecure, base::span<const Credential>, int),
              (override));
  MOCK_METHOD(void, OnCredentialSelected, (const UiCredential&));
  MOCK_METHOD(void, OnDismiss, ());
};

struct MockJniDelegate : public NoPasskeysBottomSheetBridge::JniDelegate {
  ~MockJniDelegate() override = default;
  MOCK_METHOD(void, Create, (ui::WindowAndroid&), (override));
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

    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);

    visibility_controller_ = std::make_unique<
        password_manager::MockKeyboardReplacingSurfaceVisibilityController>();
    touch_to_fill_controller_ = std::make_unique<TouchToFillController>(
        profile(), visibility_controller_->AsWeakPtr(),
        /*grouped_credential_sheet_controller=*/nullptr);
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

    request_delegate_ =
        std::make_unique<MockCredentialReceiver>(web_contents_.get());
  }

  bool Show(std::vector<Credential> credentials,
            std::unique_ptr<TouchToFillControllerWebAuthnDelegate>
                touch_to_fill_delegate) {
    touch_to_fill_controller_->InitData(std::move(credentials),
                                        /*frame_driver=*/nullptr);
    return touch_to_fill_controller_->Show(std::move(touch_to_fill_delegate),
                                           /*cred_man_delegate=*/nullptr);
  }

  void TearDown() override {
    request_delegate_.reset();
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockCredentialReceiver& request_delegate() { return *request_delegate_; }

  MockTouchToFillView& view() { return *mock_view_; }

  MockJniDelegate& jni_delegate() { return *jni_delegate_; }

  TouchToFillController& touch_to_fill_controller() {
    return *touch_to_fill_controller_;
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents_.get());
  }

  std::unique_ptr<TouchToFillControllerWebAuthnDelegate>
  MakeTouchToFillControllerDelegate(bool should_show_hybrid_option,
                                    bool is_immediate,
                                    SortingCallback sorting_callback) {
    return std::make_unique<TouchToFillControllerWebAuthnDelegate>(
        request_delegate_.get(), std::move(sorting_callback),
        should_show_hybrid_option, is_immediate);
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<MockCredentialReceiver> request_delegate_;
  std::unique_ptr<
      password_manager::MockKeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
  raw_ptr<MockJniDelegate> jni_delegate_ = nullptr;
};

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectCredential) {
  auto passkey_credential = CreatePasskey();
  std::vector<Credential> credentials{passkey_credential};

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/false,
                        /*is_immediate=*/false, SortingCallback()));

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(kCredentialId1));
  touch_to_fill_controller().OnPasskeyCredentialSelected(passkey_credential);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectWithMultipleCredential) {
  auto passkey_credential2 = CreatePasskey(kCredentialId2);
  std::vector<Credential> credentials(
      {CreatePasskey(kCredentialId1), passkey_credential2});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/false,
                        /*is_immediate=*/false, SortingCallback()));

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(kCredentialId2));
  touch_to_fill_controller().OnPasskeyCredentialSelected(passkey_credential2);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndCancel) {
  std::vector<Credential> credentials({CreatePasskey()});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/false,
                        /*is_immediate=*/false, SortingCallback()));

  EXPECT_CALL(request_delegate(), OnCredentialSelectionDeclined());
  touch_to_fill_controller().Close();
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndCancelImmediate) {
  std::vector<Credential> credentials({CreatePasskey()});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/false,
                        /*is_immediate=*/true, SortingCallback()));

  EXPECT_CALL(request_delegate(), OnCredentialSelectionDeclined());
  touch_to_fill_controller().Close();
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectHybrid) {
  std::vector<Credential> credentials({CreatePasskey()});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials),
                           TouchToFillView::kShouldShowHybridOption));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/true,
                        /*is_immediate=*/false, SortingCallback()));
  EXPECT_CALL(request_delegate(), OnHybridSignInSelected());
  touch_to_fill_controller().OnHybridSignInSelected();
}

TEST_F(TouchToFillControllerWebAuthnTest,
       ShowNoPasskeysSheetIfGpmNotInCredMan) {
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::PARALLEL_WITH_FIDO_2);

  EXPECT_CALL(view(), Show).Times(0);
  EXPECT_CALL(jni_delegate(), Show).Times(1);
  Show({}, MakeTouchToFillControllerDelegate(
               /*should_show_hybrid_option=*/false,
               /*is_immediate=*/false, SortingCallback()));
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowNothingIfGpmInCredMan) {
  webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      webauthn::CredManSupport::FULL_UNLESS_INAPPLICABLE);

  EXPECT_CALL(view(), Show).Times(0);
  EXPECT_CALL(jni_delegate(), Show).Times(0);
  Show({}, MakeTouchToFillControllerDelegate(
               /*should_show_hybrid_option=*/false,
               /*is_immediate=*/false, SortingCallback()));
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowPasswordForImmediate) {
  auto password_credential = CreatePasswordCredential();
  std::vector<Credential> credentials({CreatePasskey(), password_credential});

  EXPECT_CALL(view(),
              Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                   ElementsAreArray(credentials), TouchToFillView::kNone));
  Show(credentials, MakeTouchToFillControllerDelegate(
                        /*should_show_hybrid_option=*/false,
                        /*is_immediate=*/true, SortingCallback()));

  PasswordCredentialPair expected = {kUserName2, kPassword};
  EXPECT_CALL(request_delegate(), OnPasswordCredentialSelected(expected));
  touch_to_fill_controller().OnCredentialSelected(password_credential);
}

TEST_F(TouchToFillControllerWebAuthnTest, SortCredentialsForImmediate) {
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  auto alice_password = CreatePasswordCredential(u"alice");
  auto alice_passkey =
      CreatePasskey(kCredentialId1, kUserId1, "alice", time_older);
  auto bob_password =
      CreatePasswordCredential(u"bob", kPassword, kExampleCom, time_now);
  auto charlie_passkey =
      CreatePasskey(kCredentialId1, kUserId1, "charlie", time_now);

  std::vector<Credential> credentials(
      {alice_password, alice_passkey, charlie_passkey, bob_password});

  // This test relies on the behaviour of
  // `webauthn::sorting::SortTouchToFillCredentials`, so changes to the
  // credential sorting order might require these expectations to be updated as
  // well.
  // Currently:
  // - `alice_password` should be removed due to duplicate username with
  //   `alice_passkey`
  // - `alice_passkey` should be last in the list due to being least recently
  //   used
  // - `bob_password` should appear ahead of `charlie_passkey` due to equal
  //   last used time and then being alphabetized
  std::vector<Credential> sorted_credentials(
      {bob_password, charlie_passkey, alice_passkey});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(sorted_credentials),
                           TouchToFillView::kNone));
  Show(credentials,
       MakeTouchToFillControllerDelegate(
           /*should_show_hybrid_option=*/false,
           /*is_immediate=*/true,
           base::BindRepeating<std::vector<TouchToFillView::Credential>(
               std::vector<TouchToFillView::Credential>, bool)>(
               webauthn::sorting::SortTouchToFillCredentials)));
}
