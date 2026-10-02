// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_registry.h"

#include <memory>

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/headless_web_contents_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::testing::_;
using ::testing::Return;

class ActorSurfaceRegistryTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents1_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    headless_manager_ = std::make_unique<HeadlessWebContentsManager>(&profile_);
    registry_ = std::make_unique<ActorSurfaceRegistry>(headless_manager_.get());
    ON_CALL(mock_tab1_, GetContents())
        .WillByDefault(Return(web_contents1_.get()));
    ON_CALL(mock_tab1_, RegisterWillDetach(_))
        .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
          return will_detach_callbacks_.Add(std::move(callback));
        });
    ON_CALL(mock_tab2_, GetContents())
        .WillByDefault(Return(web_contents1_.get()));
    ON_CALL(mock_tab2_, RegisterWillDetach(_))
        .WillByDefault([this](tabs::TabInterface::WillDetach callback) {
          return will_detach_callbacks_.Add(std::move(callback));
        });
  }

  void TearDown() override {
    registry_.reset();
    headless_manager_.reset();
    web_contents1_.reset();
  }

 protected:
  tabs::TabHandle tab_handle() { return mock_tab1_.GetHandle(); }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents1_;
  std::unique_ptr<HeadlessWebContentsManager> headless_manager_;
  std::unique_ptr<ActorSurfaceRegistry> registry_;
  tabs::MockTabInterface mock_tab1_;
  tabs::MockTabInterface mock_tab2_;
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

  EXPECT_EQ(surface->GetHandle().Get(), surface);
  EXPECT_EQ(registry_->Get(surface->GetHandle()), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), surface);
}

TEST_F(ActorSurfaceRegistryTest, GetOrCreateForStaleTabReturnsNull) {
  tabs::TabHandle stale_handle;
  {
    tabs::MockTabInterface transient_tab;
    stale_handle = transient_tab.GetHandle();
  }
  ASSERT_EQ(stale_handle.Get(), nullptr);

  EXPECT_EQ(registry_->GetOrCreateForTab(stale_handle), nullptr);
  EXPECT_EQ(registry_->GetOrCreateForTab(tabs::TabHandle::Null()), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
}

TEST_F(ActorSurfaceRegistryTest, GetOrCreateForCrossProfileTabReturnsNull) {
  TestingProfile other_profile;
  std::unique_ptr<content::WebContents> other_contents =
      content::WebContentsTester::CreateTestWebContents(&other_profile,
                                                        nullptr);
  tabs::MockTabInterface other_tab;
  ON_CALL(other_tab, GetContents()).WillByDefault(Return(other_contents.get()));

  EXPECT_EQ(registry_->GetOrCreateForTab(other_tab.GetHandle()), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
}

TEST_F(ActorSurfaceRegistryTest, CreateForHeadlessIsIdempotent) {
  content::WebContents* contents = headless_manager_->Create();

  ActorSurface* surface = registry_->CreateForHeadless(contents);
  ASSERT_TRUE(surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents(), contents);
  EXPECT_EQ(surface->GetHandle().Get(), surface);
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
  const ActorSurfaceHandle handle = surface->GetHandle();
  content::WebContentsDestroyedWatcher watcher(surface->GetWebContents());

  registry_->DestroySurface(handle);

  EXPECT_EQ(handle.Get(), nullptr);
  EXPECT_EQ(registry_->Get(handle), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(ActorSurfaceRegistryTest, PromotionKeepsHandleAndRetargetsLookups) {
  ActorSurface* surface = registry_->CreateHeadlessWebContents();
  content::WebContents* contents = surface->GetWebContents();
  const ActorSurfaceHandle handle = surface->GetHandle();

  // Promotion parents the same WebContents into a tab.
  ON_CALL(mock_tab1_, GetContents()).WillByDefault(Return(contents));
  tabs::TabLookupFromWebContents::CreateForWebContents(contents, &mock_tab1_);
  registry_->OnSurfacePromoted(handle);

  EXPECT_EQ(surface->GetHandle(), handle);
  EXPECT_EQ(handle.Get(), surface);
  EXPECT_TRUE(surface->IsTab());
  EXPECT_EQ(surface->GetTabHandle(), tab_handle());
  EXPECT_EQ(surface->GetWebContents(), contents);

  EXPECT_EQ(registry_->GetForTab(tab_handle()), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle())->GetHandle(), handle);
  EXPECT_EQ(registry_->GetForHeadless(contents), nullptr);
}

TEST_F(ActorSurfaceRegistryTest, DemotionKeepsHandleAndRetargetsLookups) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceHandle handle = surface->GetHandle();

  registry_->OnSurfaceWillBeDemoted(handle);

  EXPECT_EQ(surface->GetHandle(), handle);
  EXPECT_EQ(handle.Get(), surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents(), web_contents1_.get());

  EXPECT_EQ(registry_->GetForHeadless(web_contents1_.get()), surface);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), nullptr);
}

TEST_F(ActorSurfaceRegistryTest, TabDeletionDestroysSurface) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceHandle handle = surface->GetHandle();

  will_detach_callbacks_.Notify(&mock_tab1_,
                                tabs::TabInterface::DetachReason::kDelete);

  EXPECT_EQ(handle.Get(), nullptr);
  EXPECT_EQ(registry_->Get(handle), nullptr);
  EXPECT_EQ(registry_->GetForTab(tab_handle()), nullptr);
  EXPECT_EQ(registry_->size(), 0u);
}

TEST_F(ActorSurfaceRegistryTest, TabMoveBetweenWindowsKeepsSurface) {
  ActorSurface* surface = registry_->GetOrCreateForTab(tab_handle());
  const ActorSurfaceHandle handle = surface->GetHandle();

  will_detach_callbacks_.Notify(
      &mock_tab1_, tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);

  EXPECT_EQ(handle.Get(), surface);
  EXPECT_EQ(registry_->Get(handle), surface);
}

TEST_F(ActorSurfaceRegistryTest, TabSurfaceHandleIsTabHandleValue) {
  base::test::ScopedFeatureList feature_list(kUseTabHandleAsSurfaceHandle);
  const tabs::TabHandle tab1 = mock_tab1_.GetHandle();
  const tabs::TabHandle tab2 = mock_tab2_.GetHandle();

  const ActorSurfaceHandle handle1 =
      registry_->GetOrCreateForTab(tab1)->GetHandle();
  const ActorSurfaceHandle handle2 =
      registry_->GetOrCreateForTab(tab2)->GetHandle();

  EXPECT_EQ(handle1.raw_value(), tab1.raw_value());
  EXPECT_EQ(handle2.raw_value(), tab2.raw_value());
  EXPECT_NE(handle1, handle2);
}

TEST_F(ActorSurfaceRegistryTest, HeadlessHandlesAreNegativeAndDistinct) {
  base::test::ScopedFeatureList feature_list(kUseTabHandleAsSurfaceHandle);
  const ActorSurfaceHandle first =
      registry_->CreateHeadlessWebContents()->GetHandle();
  const ActorSurfaceHandle second =
      registry_->CreateHeadlessWebContents()->GetHandle();

  EXPECT_LT(first.raw_value(), 0);
  EXPECT_EQ(second.raw_value(), first.raw_value() - 1);
}

TEST_F(ActorSurfaceRegistryTest, IndependentHandlesShareOneCounter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kUseTabHandleAsSurfaceHandle);

  const ActorSurfaceHandle first =
      registry_->GetOrCreateForTab(tab_handle())->GetHandle();
  const ActorSurfaceHandle second =
      registry_->CreateHeadlessWebContents()->GetHandle();

  EXPECT_GT(first.raw_value(), 0);
  EXPECT_EQ(second.raw_value(), first.raw_value() + 1);
}

TEST_F(ActorSurfaceRegistryTest,
       CrossProfileHandleGetIsGlobalAndRegistryGetIsProfileScoped) {
  TestingProfile other_profile;
  HeadlessWebContentsManager other_headless_manager(&other_profile);
  ActorSurfaceRegistry other_registry(&other_headless_manager);

  ActorSurface* surface1 = registry_->CreateHeadlessWebContents();
  ActorSurface* surface2 = other_registry.CreateHeadlessWebContents();
  ASSERT_TRUE(surface1);
  ASSERT_TRUE(surface2);

  const ActorSurfaceHandle handle1 = surface1->GetHandle();
  const ActorSurfaceHandle handle2 = surface2->GetHandle();
  EXPECT_NE(handle1, handle2);

  // ActorSurfaceHandle::Get() is process-wide.
  EXPECT_EQ(handle1.Get(), surface1);
  EXPECT_EQ(handle2.Get(), surface2);

  // ActorSurfaceRegistry::Get() is scoped to its own profile's surfaces.
  EXPECT_EQ(registry_->Get(handle1), surface1);
  EXPECT_EQ(registry_->Get(handle2), nullptr);
  EXPECT_EQ(other_registry.Get(handle2), surface2);
  EXPECT_EQ(other_registry.Get(handle1), nullptr);
}

}  // namespace
}  // namespace actor
