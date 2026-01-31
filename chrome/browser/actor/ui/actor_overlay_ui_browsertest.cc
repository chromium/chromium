// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_ui.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/ui/test_support/fake_actor_overlay_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

namespace {

class MockPageHandlerFactory {
 public:
  explicit MockPageHandlerFactory(ActorOverlayUI* ui) {
    ui->BindInterface(factory_remote_.BindNewPipeAndPassReceiver());
  }

  void CreatePageHandler(FakeActorOverlayPage* mock_page) {
    factory_remote_->CreatePageHandler(
        mock_page->BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver());
    factory_remote_.FlushForTesting();
  }

 private:
  mojo::Remote<mojom::ActorOverlayPageHandlerFactory> factory_remote_;
  mojo::Remote<mojom::ActorOverlayPageHandler> handler_remote_;
};

}  // namespace

class ActorOverlayUITest : public InProcessBrowserTest {
 public:
  ActorOverlayUITest() = default;

  std::unique_ptr<ActorOverlayUI> CreateController() {
    auto web_ui = std::make_unique<content::TestWebUI>();
    web_ui->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());
    test_web_ui_ = std::move(web_ui);
    return std::make_unique<ActorOverlayUI>(test_web_ui_.get());
  }

 private:
  std::unique_ptr<content::TestWebUI> test_web_ui_;
};

IN_PROC_BROWSER_TEST_F(ActorOverlayUITest,
                       SetOverlayBackgroundQueuesAndReplaysUpdates) {
  std::unique_ptr<ActorOverlayUI> controller = CreateController();

  controller->SetOverlayBackground(true);
  controller->SetOverlayBackground(false);
  controller->SetOverlayBackground(true);

  MockPageHandlerFactory factory_helper(controller.get());
  FakeActorOverlayPage fake_page;
  factory_helper.CreatePageHandler(&fake_page);
  fake_page.FlushForTesting();

  EXPECT_EQ(fake_page.scrim_background_call_count(), 3);
  EXPECT_TRUE(fake_page.is_scrim_background_visible());
}

IN_PROC_BROWSER_TEST_F(ActorOverlayUITest,
                       SetBorderGlowVisibilityCrashesIfHandlerNull) {
  std::unique_ptr<ActorOverlayUI> controller = CreateController();
  EXPECT_DCHECK_DEATH(controller->SetBorderGlowVisibility(true));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayUITest, MoveCursorToCrashesIfHandlerNull) {
  std::unique_ptr<ActorOverlayUI> controller = CreateController();
  base::test::TestFuture<void> future;
  EXPECT_DCHECK_DEATH(
      controller->MoveCursorTo(gfx::Point(10, 10), future.GetCallback()));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayUITest,
                       TriggerClickAnimationCrashesIfHandlerNull) {
  std::unique_ptr<ActorOverlayUI> controller = CreateController();
  base::test::TestFuture<void> future;
  EXPECT_DCHECK_DEATH(controller->TriggerClickAnimation(future.GetCallback()));
}

}  // namespace actor::ui
