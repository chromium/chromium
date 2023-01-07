// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/request_system_proxy_credentials_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {
constexpr char kProxy[] = "http://localserver";
constexpr char16_t kUsername[] = u"testuser";
constexpr char16_t kPassword[] = u"testpwd";
}  // namespace

namespace ash {

class RequestSystemProxyCredentialsViewTest : public BrowserWithTestWindowTest {
 public:
  RequestSystemProxyCredentialsViewTest()
      : BrowserWithTestWindowTest(Browser::TYPE_NORMAL) {}
  RequestSystemProxyCredentialsViewTest(
      const RequestSystemProxyCredentialsViewTest&) = delete;
  RequestSystemProxyCredentialsViewTest& operator=(
      const RequestSystemProxyCredentialsViewTest&) = delete;
  ~RequestSystemProxyCredentialsViewTest() override = default;

  void TearDown() override {
    active_widget_->CloseNow();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void CreateDialog(bool show_error) {
    system_proxy_dialog_ = new RequestSystemProxyCredentialsView(
        kProxy, show_error, base::DoNothing());

    system_proxy_dialog_->SetAcceptCallback(
        base::BindRepeating(&RequestSystemProxyCredentialsViewTest::OnAccept,
                            base::Unretained(this)));
    system_proxy_dialog_->SetCancelCallback(
        base::BindRepeating(&RequestSystemProxyCredentialsViewTest::OnCancel,
                            base::Unretained(this)));

    active_widget_ = views::DialogDelegate::CreateDialogWidget(
        system_proxy_dialog_, GetContext(), /*parent=*/nullptr);
    active_widget_->Show();
  }

  void OnAccept() { accepted_ = true; }

  void OnCancel() { canceled_ = true; }

  bool accepted_ = false;
  bool canceled_ = false;
  // Owned by |active_widget_|.
  RequestSystemProxyCredentialsView* system_proxy_dialog_ = nullptr;

 private:
  // Owned by the UI code (NativeWidget).
  views::Widget* active_widget_ = nullptr;
};

// Tests that clicking "OK" in the UI will result in calling the
// |system_proxy_dialog_.accept_callback_| with the user entered
// credentials as arguments.
TEST_F(RequestSystemProxyCredentialsViewTest, AcceptCallback) {
  CreateDialog(/*show_error=*/false);
  system_proxy_dialog_->username_textfield_for_testing()->SetText(kUsername);
  system_proxy_dialog_->password_textfield_for_testing()->SetText(kPassword);

  // Simulate pressing the "OK" button.
  system_proxy_dialog_->Accept();

  EXPECT_TRUE(accepted_);
  EXPECT_EQ(system_proxy_dialog_->GetUsername(), kUsername);
  EXPECT_EQ(system_proxy_dialog_->GetPassword(), kPassword);
}

TEST_F(RequestSystemProxyCredentialsViewTest, CancelCallback) {
  CreateDialog(/*show_error=*/false);
  system_proxy_dialog_->Cancel();

  EXPECT_TRUE(canceled_);
}

TEST_F(RequestSystemProxyCredentialsViewTest, GetProxyServer) {
  CreateDialog(/*show_error=*/false);
  EXPECT_EQ(system_proxy_dialog_->GetProxyServer(), kProxy);
}

TEST_F(RequestSystemProxyCredentialsViewTest, GetWindowTitle) {
  CreateDialog(/*show_error=*/false);
  EXPECT_EQ(system_proxy_dialog_->GetWindowTitle(), u"Sign in");
}

TEST_F(RequestSystemProxyCredentialsViewTest, ErrorLabelHidden) {
  CreateDialog(/*show_error=*/false);
  EXPECT_FALSE(system_proxy_dialog_->error_label_for_testing()->GetVisible());
}

TEST_F(RequestSystemProxyCredentialsViewTest, ErrorLabelVisible) {
  CreateDialog(/*show_error=*/true);
  EXPECT_TRUE(system_proxy_dialog_->error_label_for_testing()->GetVisible());
}

}  // namespace ash
