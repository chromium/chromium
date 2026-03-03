// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/ui/actor_overlay_ui.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockActorOverlayUI : public ActorOverlayUI {
 public:
  explicit MockActorOverlayUI(content::WebUI* web_ui)
      : ActorOverlayUI(web_ui) {}
  MOCK_METHOD(void, SetOverlayBackground, (bool), (override));
  MOCK_METHOD(void, SetBorderGlowVisibility, (bool), (override));
  MOCK_METHOD(void,
              MoveCursorTo,
              (const gfx::Point&, base::OnceClosure),
              (override));
  MOCK_METHOD(void, TriggerClickAnimation, (base::OnceClosure), (override));
};

class TestActorOverlayWebView : public ActorOverlayWebView {
 public:
  explicit TestActorOverlayWebView(BrowserWindowInterface* browser)
      : ActorOverlayWebView(browser) {}

  void set_web_ui(ActorOverlayUI* web_ui) { web_ui_ = web_ui; }

 protected:
  ActorOverlayUI* GetWebUi() override { return web_ui_; }

 private:
  raw_ptr<ActorOverlayUI> web_ui_ = nullptr;
};

class ActorOverlayWebViewTestBase : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    rvh_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));

    web_view_ =
        std::make_unique<TestActorOverlayWebView>(&mock_browser_window_);
    web_view_->LoadInitialURL(GURL(chrome::kChromeUIActorOverlayURL));
    test_web_ui_.set_web_contents(web_view_->web_contents());
    mock_web_ui_ =
        std::make_unique<NiceMock<MockActorOverlayUI>>(&test_web_ui_);
  }

  void TearDown() override {
    if (web_view_) {
      web_view_->set_web_ui(nullptr);
    }
    mock_web_ui_.reset();
    web_view_.reset();
    profile_.reset();
    rvh_enabler_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI test_web_ui_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<TestActorOverlayWebView> web_view_;
  std::unique_ptr<MockActorOverlayUI> mock_web_ui_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ActorOverlayWebViewTestBase, SetOverlayBackgroundForwardsToWebUI) {
  web_view_->set_web_ui(mock_web_ui_.get());
  EXPECT_CALL(*mock_web_ui_, SetOverlayBackground(true));
  web_view_->SetOverlayBackground(true);
  EXPECT_CALL(*mock_web_ui_, SetOverlayBackground(false));
  web_view_->SetOverlayBackground(false);
}

TEST_F(ActorOverlayWebViewTestBase, SetOverlayBackgroundHandlesNullWebUi) {
  web_view_->set_web_ui(nullptr);
  // Verifies that the null check prevents a crash.
  web_view_->SetOverlayBackground(true);
  web_view_->SetOverlayBackground(false);
}

TEST_F(ActorOverlayWebViewTestBase, SetBorderGlowVisibilityForwardsToWebUI) {
  web_view_->set_web_ui(mock_web_ui_.get());
  EXPECT_CALL(*mock_web_ui_, SetBorderGlowVisibility(true));
  web_view_->SetBorderGlowVisibility(true);
  EXPECT_CALL(*mock_web_ui_, SetBorderGlowVisibility(false));
  web_view_->SetBorderGlowVisibility(false);
}

TEST_F(ActorOverlayWebViewTestBase, SetBorderGlowVisibilityHandlesNullWebUi) {
  web_view_->set_web_ui(nullptr);
  // Verifies that the null check prevents a crash.
  web_view_->SetBorderGlowVisibility(true);
  web_view_->SetBorderGlowVisibility(false);
}

class ActorOverlayWebViewMagicCursorEnabledTest
    : public ActorOverlayWebViewTestBase {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kGlicActorUiMagicCursor);
    ActorOverlayWebViewTestBase::SetUp();
  }
};

TEST_F(ActorOverlayWebViewMagicCursorEnabledTest, MoveCursorForwardsToWebUI) {
  web_view_->set_web_ui(mock_web_ui_.get());

  gfx::Point target(50, 50);
  EXPECT_CALL(*mock_web_ui_, MoveCursorTo(target, _))
      .WillOnce(
          [](const gfx::Point&, base::OnceClosure cb) { std::move(cb).Run(); });

  base::test::TestFuture<void> future;
  web_view_->MoveCursorTo(target, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ActorOverlayWebViewMagicCursorEnabledTest, MoveCursorHandlesNullWebUi) {
  web_view_->set_web_ui(nullptr);

  base::test::TestFuture<void> future;
  web_view_->MoveCursorTo(gfx::Point(50, 50), future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ActorOverlayWebViewMagicCursorEnabledTest, TriggerClickForwardsToWebUI) {
  web_view_->set_web_ui(mock_web_ui_.get());

  EXPECT_CALL(*mock_web_ui_, TriggerClickAnimation(_))
      .WillOnce([](base::OnceClosure cb) { std::move(cb).Run(); });

  base::test::TestFuture<void> future;
  web_view_->TriggerClickAnimation(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ActorOverlayWebViewMagicCursorEnabledTest,
       TriggerClickHandlesNullWebUi) {
  web_view_->set_web_ui(nullptr);

  base::test::TestFuture<void> future;
  web_view_->TriggerClickAnimation(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

class ActorOverlayWebViewMagicCursorDisabledTest
    : public ActorOverlayWebViewTestBase {
 public:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kGlicActorUiMagicCursor);
    ActorOverlayWebViewTestBase::SetUp();
  }
};

TEST_F(ActorOverlayWebViewMagicCursorDisabledTest, MoveCursorIgnored) {
  web_view_->set_web_ui(mock_web_ui_.get());
  EXPECT_CALL(*mock_web_ui_, MoveCursorTo(_, _)).Times(0);

  base::test::TestFuture<void> future;
  web_view_->MoveCursorTo(gfx::Point(10, 10), future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ActorOverlayWebViewMagicCursorDisabledTest, TriggerClickIgnored) {
  web_view_->set_web_ui(mock_web_ui_.get());
  EXPECT_CALL(*mock_web_ui_, TriggerClickAnimation(_)).Times(0);

  base::test::TestFuture<void> future;
  web_view_->TriggerClickAnimation(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

}  // namespace
}  // namespace actor::ui
