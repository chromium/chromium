// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller_webauthn_delegate.h"

#include <memory>

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_webauthn_credential.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using password_manager::UiCredential;
using IsOriginSecure = TouchToFillView::IsOriginSecure;
using ::testing::ElementsAreArray;
using ::testing::Eq;

constexpr char kExampleCom[] = "https://example.com/";
constexpr uint8_t kUserId1[] = {'1', '2', '3', '4'};
constexpr uint8_t kUserId2[] = {'5', '6', '7', '8'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";

std::vector<uint8_t> UserId1AsVector() {
  return std::vector<uint8_t>(std::begin(kUserId1), std::end(kUserId1));
}
std::vector<uint8_t> UserId2AsVector() {
  return std::vector<uint8_t>(std::begin(kUserId2), std::end(kUserId2));
}
std::string UserId1AsString() {
  return base::Base64Encode(kUserId1);
}
std::string UserId2AsString() {
  return base::Base64Encode(kUserId2);
}
std::u16string UserName1() {
  return base::UTF8ToUTF16(std::string(kUserName1));
}
std::u16string UserName2() {
  return base::UTF8ToUTF16(std::string(kUserName2));
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
};

struct MockTouchToFillView : public TouchToFillView {
  MOCK_METHOD(void,
              Show,
              (const GURL&,
               IsOriginSecure,
               base::span<const UiCredential>,
               base::span<const TouchToFillWebAuthnCredential>,
               bool),
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
    return touch_to_fill_controller_;
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents_.get());
  }

  std::unique_ptr<TouchToFillControllerWebAuthnDelegate>
  MakeTouchToFillControllerDelegate() {
    return std::make_unique<TouchToFillControllerWebAuthnDelegate>(
        request_delegate_.get());
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MockWebAuthnRequestDelegateAndroid> request_delegate_;
  TouchToFillController touch_to_fill_controller_;
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
};

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectCredential) {
  TouchToFillWebAuthnCredential credential(
      (TouchToFillWebAuthnCredential::Username(UserName1())),
      TouchToFillWebAuthnCredential::BackendId(UserId1AsString()));
  std::vector<TouchToFillWebAuthnCredential> credentials({credential});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(std::vector<UiCredential>()),
                           ElementsAreArray(credentials),
                           /*trigger_submission=*/false));
  touch_to_fill_controller().Show({}, credentials,
                                  MakeTouchToFillControllerDelegate());

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(UserId1AsVector()));
  touch_to_fill_controller().OnWebAuthnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndSelectWithMultipleCredential) {
  TouchToFillWebAuthnCredential credential1(
      (TouchToFillWebAuthnCredential::Username(UserName1())),
      TouchToFillWebAuthnCredential::BackendId(UserId1AsString()));
  TouchToFillWebAuthnCredential credential2(
      (TouchToFillWebAuthnCredential::Username(UserName2())),
      TouchToFillWebAuthnCredential::BackendId(UserId2AsString()));
  std::vector<TouchToFillWebAuthnCredential> credentials(
      {credential1, credential2});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(std::vector<UiCredential>()),
                           ElementsAreArray(credentials),
                           /*trigger_submission=*/false));
  touch_to_fill_controller().Show({}, credentials,
                                  MakeTouchToFillControllerDelegate());

  EXPECT_CALL(request_delegate(), OnWebAuthnAccountSelected(UserId2AsVector()));
  touch_to_fill_controller().OnWebAuthnCredentialSelected(credentials[1]);
}

TEST_F(TouchToFillControllerWebAuthnTest, ShowAndCancel) {
  TouchToFillWebAuthnCredential credential(
      (TouchToFillWebAuthnCredential::Username(UserName1())),
      TouchToFillWebAuthnCredential::BackendId(UserId1AsString()));
  std::vector<TouchToFillWebAuthnCredential> credentials({credential});

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(std::vector<UiCredential>()),
                           ElementsAreArray(credentials),
                           /*trigger_submission=*/false));
  touch_to_fill_controller().Show({}, credentials,
                                  MakeTouchToFillControllerDelegate());

  EXPECT_CALL(request_delegate(),
              OnWebAuthnAccountSelected(std::vector<uint8_t>()));
  touch_to_fill_controller().Close();
}

}  // namespace
