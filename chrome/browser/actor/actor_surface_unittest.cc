// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface.h"

#include <memory>

#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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

TEST_F(ActorSurfaceTest, NullAndUnknownHandleReturnNull) {
  EXPECT_TRUE(ActorSurfaceHandle().is_null());
  EXPECT_TRUE(ActorSurfaceHandle::Null().is_null());
  EXPECT_EQ(ActorSurfaceHandle::Null().raw_value(), 0);
  EXPECT_EQ(ActorSurfaceHandle::Null().Get(), nullptr);
  EXPECT_EQ(ActorSurfaceHandle::CreateForTesting().Get(), nullptr);
}

TEST_F(ActorSurfaceTest, TabBacked) {
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::CreateForTesting();
  auto surface = std::make_unique<ActorSurfaceImpl>(actor_surface_handle,
                                                    mock_tab_.GetHandle());

  EXPECT_EQ(surface->GetHandle(), actor_surface_handle);
  EXPECT_EQ(actor_surface_handle.Get(), surface.get());
  EXPECT_TRUE(surface->IsTab());
  EXPECT_EQ(surface->GetTabHandle(), mock_tab_.GetHandle());
  EXPECT_EQ(surface->GetWebContents(), web_contents());

  surface.reset();
  EXPECT_EQ(actor_surface_handle.Get(), nullptr);
}

TEST_F(ActorSurfaceTest, HeadlessBacked) {
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::CreateForTesting();
  auto surface =
      std::make_unique<ActorSurfaceImpl>(actor_surface_handle, web_contents());

  EXPECT_EQ(surface->GetHandle(), actor_surface_handle);
  EXPECT_EQ(actor_surface_handle.Get(), surface.get());
  EXPECT_FALSE(surface->IsTab());
  EXPECT_FALSE(surface->GetTabHandle().has_value());
  EXPECT_EQ(surface->GetWebContents(), web_contents());

  surface.reset();
  EXPECT_EQ(actor_surface_handle.Get(), nullptr);
}

// A tab's WebContents can be swapped out, e.g. on discard. The surface must
// follow the tab rather than hold on to the original WebContents.
TEST_F(ActorSurfaceTest, WebContentsFollowsTabSwap) {
  ActorSurfaceImpl surface(ActorSurfaceHandle::CreateForTesting(),
                           mock_tab_.GetHandle());
  ASSERT_EQ(surface.GetWebContents(), web_contents());

  std::unique_ptr<content::WebContents> swapped = CreateTestWebContents();
  ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(swapped.get()));

  EXPECT_EQ(surface.GetWebContents(), swapped.get());
}

TEST_F(ActorSurfaceTest, TabBackedOwnsActorTabData) {
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::CreateForTesting();
  auto surface = std::make_unique<ActorSurfaceImpl>(actor_surface_handle,
                                                    mock_tab_.GetHandle());

  EXPECT_EQ(ActorTabData::From(actor_surface_handle),
            surface->GetActorTabData());

  surface.reset();
  EXPECT_EQ(ActorTabData::From(actor_surface_handle), nullptr);
}

TEST_F(ActorSurfaceTest, HeadlessBackedOwnsActorTabData) {
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::CreateForTesting();
  ActorSurfaceImpl surface(actor_surface_handle, web_contents());

  ActorTabData* tab_data = surface.GetActorTabData();
  EXPECT_EQ(ActorTabData::From(actor_surface_handle), tab_data);
  EXPECT_EQ(tab_data->GetLastObservedPageContent(), nullptr);

  optimization_guide::proto::AnnotatedPageContent content;
  content.mutable_main_frame_data()->set_title("title");
  tab_data->DidObserveContent(content, ApcSource::kActor);

  const optimization_guide::proto::AnnotatedPageContent* observed =
      ActorTabData::From(actor_surface_handle)->GetLastObservedPageContent();
  ASSERT_TRUE(observed);
  EXPECT_EQ(observed->main_frame_data().title(), "title");
}

}  // namespace
}  // namespace actor
