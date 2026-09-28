// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface.h"

#include <memory>

#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::testing::Return;

class ActorSurfaceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
  }

 protected:
  tabs::MockTabInterface mock_tab_;
};

TEST_F(ActorSurfaceTest, TabBacked) {
  ActorSurfaceImpl surface(ActorSurfaceId(1), mock_tab_.GetHandle());

  EXPECT_EQ(surface.Id(), ActorSurfaceId(1));
  EXPECT_TRUE(surface.IsTab());
  EXPECT_EQ(surface.GetTabHandle(), mock_tab_.GetHandle());
  EXPECT_EQ(surface.GetWebContents(), web_contents());
}

TEST_F(ActorSurfaceTest, HeadlessBacked) {
  ActorSurfaceImpl surface(ActorSurfaceId(2), web_contents());

  EXPECT_EQ(surface.Id(), ActorSurfaceId(2));
  EXPECT_FALSE(surface.IsTab());
  EXPECT_FALSE(surface.GetTabHandle().has_value());
  EXPECT_EQ(surface.GetWebContents(), web_contents());
}

// A tab's WebContents can be swapped out, e.g. on discard. The surface must
// follow the tab rather than hold on to the original WebContents.
TEST_F(ActorSurfaceTest, WebContentsFollowsTabSwap) {
  ActorSurfaceImpl surface(ActorSurfaceId(3), mock_tab_.GetHandle());
  ASSERT_EQ(surface.GetWebContents(), web_contents());

  std::unique_ptr<content::WebContents> swapped = CreateTestWebContents();
  ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(swapped.get()));

  EXPECT_EQ(surface.GetWebContents(), swapped.get());
}

}  // namespace
}  // namespace actor
