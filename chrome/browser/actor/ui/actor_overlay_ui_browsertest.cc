// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_ui.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

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
                       SetOverlayBackgroundCrashesIfHandlerNull) {
  std::unique_ptr<ActorOverlayUI> controller = CreateController();
  EXPECT_DCHECK_DEATH(controller->SetOverlayBackground(true));
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

}  // namespace actor::ui
