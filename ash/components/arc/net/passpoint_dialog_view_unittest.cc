// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/passpoint_dialog_view.h"

#include <memory>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/net/browser_url_opener.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace arc {
namespace {

class TestBrowserUrlOpener : public BrowserUrlOpener {
 public:
  ~TestBrowserUrlOpener() override = default;

  // BrowserUrlOpener:
  void OpenUrl(GURL url) override { url_open_count_ += 1; }

  int url_open_count() { return url_open_count_; }

 private:
  int url_open_count_ = 0;
};

class PasspointDialogViewTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    browser_delegate_ = std::make_unique<TestBrowserUrlOpener>();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->SetBounds(gfx::Rect(800, 800));

    mojom::PasspointApprovalRequestPtr request =
        mojom::PasspointApprovalRequest::New(
            /*package_name=*/std::string(), /*app_name=*/std::string(),
            /*friendly_name=*/std::string(),
            /*subscription_expiration_time_ms=*/0);
    dialog_view_ =
        widget_->SetContentsView(std::make_unique<PasspointDialogView>(
            std::move(request),
            base::BindOnce(&PasspointDialogViewTest::OnClicked,
                           base::Unretained(this))));
    widget_->Show();
  }

  void TearDown() override {
    widget_->Close();
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void LeftClickOnView(const views::Widget* widget,
                       const views::View* view) const {
    ui::test::EventGenerator event_generator(GetRootWindow(widget));
    event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

  void ClickDialogButton(bool allow) {
    PasspointDialogView::TestApi dialog_view_test(dialog_view_);
    auto* target_button = allow ? dialog_view_test.allow_button()
                                : dialog_view_test.dont_allow_button();
    LeftClickOnView(widget_.get(), target_button);
  }

  void ClickLearnMoreLink() {
    PasspointDialogView::TestApi dialog_view_test(dialog_view_);
    auto* learn_more_link =
        dialog_view_test.body_text()->GetFirstLinkForTesting();
    LeftClickOnView(widget_.get(), learn_more_link);
  }

  bool callback_called() { return callback_called_; }
  bool callback_allowed() { return callback_allowed_; }

  int url_open_count() { return browser_delegate_->url_open_count(); }

 private:
  void OnClicked(mojom::PasspointApprovalResponsePtr response) {
    callback_called_ = true;
    callback_allowed_ = response->allowed;
  }

  // For callback checks.
  bool callback_called_{false};
  bool callback_allowed_{false};

  // A LayoutProvider must exist in scope in order to set up views.
  views::LayoutProvider layout_provider;

  // Handles URL open.
  std::unique_ptr<TestBrowserUrlOpener> browser_delegate_;

  raw_ptr<PasspointDialogView, DanglingUntriaged> dialog_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PasspointDialogViewTest, ClickAllow) {
  ClickDialogButton(/*allow=*/true);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(callback_allowed());
}

TEST_F(PasspointDialogViewTest, ClickDontAllow) {
  ClickDialogButton(/*allow=*/false);
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_allowed());
}

TEST_F(PasspointDialogViewTest, ClickLearnMore) {
  ClickLearnMoreLink();
  EXPECT_EQ(url_open_count(), 1);
  ClickLearnMoreLink();
  EXPECT_EQ(url_open_count(), 2);
}

}  // namespace
}  // namespace arc
