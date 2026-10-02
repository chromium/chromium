// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface.h"

#include <memory>

#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace actor {
namespace {

using ::testing::Return;
using ::testing::ReturnRef;

class ActorSurfaceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
  }

 protected:
  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
};

TEST_F(ActorSurfaceTest, NullAndUnknownHandleReturnNull) {
  EXPECT_TRUE(ActorSurfaceHandle().is_null());
  EXPECT_TRUE(ActorSurfaceHandle::Null().is_null());
  EXPECT_EQ(ActorSurfaceHandle::Null().raw_value(), 0);
  EXPECT_EQ(ActorSurfaceHandle::Null().Get(), nullptr);
  EXPECT_EQ(ActorSurfaceHandle(42).Get(), nullptr);
}

TEST_F(ActorSurfaceTest, TabBacked) {
  const ActorSurfaceHandle handle(1);
  auto surface =
      std::make_unique<ActorSurfaceImpl>(handle, mock_tab_.GetHandle());

  EXPECT_EQ(surface->GetHandle(), handle);
  EXPECT_EQ(handle.Get(), surface.get());
  EXPECT_TRUE(surface->IsTab());
  EXPECT_EQ(surface->GetTabHandle(), mock_tab_.GetHandle());
  EXPECT_EQ(surface->GetWebContents(), web_contents());

  surface.reset();
  EXPECT_EQ(handle.Get(), nullptr);
}

TEST_F(ActorSurfaceTest, HeadlessBacked) {
  const ActorSurfaceHandle handle(2);
  auto surface = std::make_unique<ActorSurfaceImpl>(handle, web_contents());

  EXPECT_EQ(surface->GetHandle(), handle);
  EXPECT_EQ(handle.Get(), surface.get());
  EXPECT_FALSE(surface->IsTab());
  EXPECT_FALSE(surface->GetTabHandle().has_value());
  EXPECT_EQ(surface->GetWebContents(), web_contents());

  surface.reset();
  EXPECT_EQ(handle.Get(), nullptr);
}

// A tab's WebContents can be swapped out, e.g. on discard. The surface must
// follow the tab rather than hold on to the original WebContents.
TEST_F(ActorSurfaceTest, WebContentsFollowsTabSwap) {
  ActorSurfaceImpl surface(ActorSurfaceHandle(3), mock_tab_.GetHandle());
  ASSERT_EQ(surface.GetWebContents(), web_contents());

  std::unique_ptr<content::WebContents> swapped = CreateTestWebContents();
  ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(swapped.get()));

  EXPECT_EQ(surface.GetWebContents(), swapped.get());
}

TEST_F(ActorSurfaceTest, TabBackedReturnsTabActorTabData) {
  ActorSurfaceImpl surface(ActorSurfaceHandle(4), mock_tab_.GetHandle());
  auto tab_data = std::make_unique<ActorTabData>(&mock_tab_);

  EXPECT_EQ(surface.GetActorTabData(), tab_data.get());
  EXPECT_EQ(surface.GetActorTabData(), ActorTabData::From(&mock_tab_));
}

TEST_F(ActorSurfaceTest, TabBackedWithoutActorTabDataReturnsNull) {
  ActorSurfaceImpl surface(ActorSurfaceHandle(5), mock_tab_.GetHandle());

  EXPECT_EQ(surface.GetActorTabData(), nullptr);
}

TEST_F(ActorSurfaceTest, HeadlessReturnsNullActorTabData) {
  ActorSurfaceImpl surface(ActorSurfaceHandle(6), web_contents());

  EXPECT_EQ(surface.GetActorTabData(), nullptr);
}

}  // namespace
}  // namespace actor
