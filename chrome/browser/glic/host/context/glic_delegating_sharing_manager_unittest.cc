// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class GlicDelegatingSharingManagerTest : public testing::Test {
 public:
  GlicDelegatingSharingManagerTest() = default;
  ~GlicDelegatingSharingManagerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(GlicDelegatingSharingManagerTest, EmptyDelegateBehavior) {
  GlicDelegatingSharingManager manager;

  // Verify GetFocusedTabData returns error/no focus
  FocusedTabData focused_tab_data = manager.GetFocusedTabData();
  EXPECT_FALSE(focused_tab_data.is_focus());
  EXPECT_FALSE(focused_tab_data.GetFocus().has_value());

  // Verify PinTabs/UnpinTabs return false/fail safely
  EXPECT_FALSE(manager.PinTabs({}, GlicPinTrigger::kUnknown));
  EXPECT_FALSE(manager.UnpinTabs({}, GlicUnpinTrigger::kUnknown));

  // Verify UnpinAllTabs doesn't crash
  manager.UnpinAllTabs(GlicUnpinTrigger::kUnknown);

  // Verify GetPinnedTabUsage returns nullopt
  EXPECT_EQ(manager.GetPinnedTabUsage(tabs::TabHandle()), std::nullopt);

  // Verify getters return 0/false/empty
  EXPECT_EQ(manager.GetMaxPinnedTabs(), 0);
  EXPECT_EQ(manager.GetNumPinnedTabs(), 0);
  EXPECT_FALSE(manager.IsTabPinned(tabs::TabHandle()));
  EXPECT_TRUE(manager.GetPinnedTabs().empty());

  // Verify SetMaxPinnedTabs returns 0
  EXPECT_EQ(manager.SetMaxPinnedTabs(5), 0);

  // Verify GetContextFromTab returns error
  base::test::TestFuture<GlicGetContextResult> get_context_future;
  manager.GetContextFromTab(tabs::TabHandle(), {},
                            get_context_future.GetCallback());
  GlicGetContextResult context_result = get_context_future.Take();
  EXPECT_FALSE(context_result.has_value());
  EXPECT_EQ(context_result.error().error_code,
            GlicGetContextFromTabError::kPageContextNotEligible);

  // Verify GetContextForActorFromTab returns error
  base::test::TestFuture<GlicGetContextResult> get_actor_context_future;
  manager.GetContextForActorFromTab(tabs::TabHandle(), {},
                                    get_actor_context_future.GetCallback());
  GlicGetContextResult actor_context_result = get_actor_context_future.Take();
  EXPECT_FALSE(actor_context_result.has_value());
  EXPECT_EQ(actor_context_result.error().error_code,
            GlicGetContextFromTabError::kPageContextNotEligible);

  // Verify GetFocusedBrowser returns nullptr
  EXPECT_EQ(manager.GetFocusedBrowser(), nullptr);
}

}  // namespace

}  // namespace glic
