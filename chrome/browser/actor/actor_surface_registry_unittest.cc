// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_registry.h"

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/headless_web_contents_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::testing::_;
using ::testing::Return;

class ActorSurfaceRegistryTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    headless_manager_ = std::make_unique<HeadlessWebContentsManager>(profile());
    registry_ = std::make_unique<ActorSurfaceRegistry>(headless_manager_.get());
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, RegisterWillDetach(_))
        .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
          return will_detach_callbacks_.Add(std::move(callback));
        });
  }

  void TearDown() override {
    registry_.reset();
    headless_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  tabs::TabHandle tab_handle() { return mock_tab_.GetHandle(); }

  std::unique_ptr<HeadlessWebContentsManager> headless_manager_;
  std::unique_ptr<ActorSurfaceRegistry> registry_;
  tabs::MockTabInterface mock_tab_;
  base::RepeatingCallbackList<void(tabs::TabInterface*,
                                   tabs::TabInterface::DetachReason)>
      will_detach_callbacks_;
};

TEST_F(ActorSurfaceRegistryTest, GetOrCreateForTabIsIdempotent) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  ASSERT_TRUE(surface);
  EXPECT_TRUE(surface->IsTab());
  EXPECT_EQ(registry_->GetOrCreateForTab(tab_handle()), surface);
  EXPECT_EQ(registry_->size(), 1u);

  EXPECT_EQ(registry_->Get(surface->Id()), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), surface);
}

TEST_F(ActorSurfaceRegistryTest, CreateForHeadlessIsIdempotent) {
  content::WebContents* contents = headless_manager_->Create();

  ActorSurface* surface = registry_->CreateForHeadless(contents);
  ASSERT_TRUE(surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents(), contents);
  EXPECT_EQ(registry_->CreateForHeadless(contents), surface);
  EXPECT_EQ(registry_->size(), 1u);

  EXPECT_EQ(registry_->GetForHeadless(contents), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), nullptr);
}

TEST_F(ActorSurfaceRegistryTest,
       CreateHeadlessWebContentsUsesManagerOwnedContents) {
  ActorSurface* surface = registry_->CreateHeadlessWebContents();
  ASSERT_TRUE(surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents()->GetDelegate(), headless_manager_.get());
  EXPECT_EQ(registry_->GetForHeadless(surface->GetWebContents()), surface);
  EXPECT_EQ(registry_->size(), 1u);
}

TEST_F(ActorSurfaceRegistryTest, DestroyHeadlessSurfaceDestroysContents) {
  ActorSurface* surface = registry_->CreateHeadlessWebContents();
  const ActorSurfaceId id = surface->Id();
  content::WebContentsDestroyedWatcher watcher(surface->GetWebContents());

  registry_->DestroySurface(id);

  EXPECT_EQ(registry_->Get(id), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(ActorSurfaceRegistryTest, PromotionKeepsIdAndRetargetsLookups) {
  ActorSurface* surface = registry_->CreateHeadlessWebContents();
  content::WebContents* contents = surface->GetWebContents();
  const ActorSurfaceId id = surface->Id();

  // Promotion parents the same WebContents into a tab.
  ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(contents));
  tabs::TabLookupFromWebContents::CreateForWebContents(contents, &mock_tab_);
  registry_->OnSurfacePromoted(id);

  EXPECT_EQ(surface->Id(), id);
  EXPECT_TRUE(surface->IsTab());
  EXPECT_EQ(surface->GetTabHandle(), tab_handle());
  EXPECT_EQ(surface->GetWebContents(), contents);

  EXPECT_EQ(registry_->GetForTab(tab_handle()), surface);
  EXPECT_EQ(registry_->GetForHeadless(contents), nullptr);
}

TEST_F(ActorSurfaceRegistryTest, DemotionKeepsIdAndRetargetsLookups) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceId id = surface->Id();

  registry_->OnSurfaceWillBeDemoted(id);

  EXPECT_EQ(surface->Id(), id);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents(), web_contents());

  EXPECT_EQ(registry_->GetForHeadless(web_contents()), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), nullptr);
}

TEST_F(ActorSurfaceRegistryTest, TabDeletionDestroysSurface) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceId id = surface->Id();

  will_detach_callbacks_.Notify(&mock_tab_,
                                tabs::TabInterface::DetachReason::kDelete);

  EXPECT_EQ(registry_->Get(id), nullptr);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
}

TEST_F(ActorSurfaceRegistryTest, TabMoveBetweenWindowsKeepsSurface) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceId id = surface->Id();

  will_detach_callbacks_.Notify(
      &mock_tab_, tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);

  EXPECT_EQ(registry_->Get(id), surface);
}

}  // namespace
}  // namespace actor
