// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
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

class MockGlicSharingManager : public GlicSharingManager {
 public:
  MockGlicSharingManager() = default;
  ~MockGlicSharingManager() override = default;

  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  FocusedTabData GetFocusedTabData() override {
    return FocusedTabData("mock", nullptr);
  }
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  MOCK_METHOD(BrowserWindowInterface*,
              GetFocusedBrowser,
              (),
              (const, override));
  MOCK_METHOD(GlicFocusedBrowserManager&,
              focused_browser_manager,
              (),
              (override));
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback) override {
    return base::CallbackListSubscription();
  }
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) override {
    return base::CallbackListSubscription();
  }

  MOCK_METHOD(bool,
              PinTabs,
              (base::span<const tabs::TabHandle>, GlicPinTrigger),
              (override));
  MOCK_METHOD(bool,
              UnpinTabs,
              (base::span<const tabs::TabHandle>, GlicUnpinTrigger),
              (override));
  MOCK_METHOD(void, UnpinAllTabs, (GlicUnpinTrigger), (override));
  MOCK_METHOD(int32_t, GetMaxPinnedTabs, (), (const, override));
  MOCK_METHOD(int32_t, GetNumPinnedTabs, (), (const, override));
  MOCK_METHOD(int32_t, SetMaxPinnedTabs, (uint32_t), (override));
  MOCK_METHOD(std::vector<content::WebContents*>,
              GetPinnedTabs,
              (),
              (const, override));
  MOCK_METHOD(bool, IsTabPinned, (tabs::TabHandle), (const, override));
  MOCK_METHOD(std::optional<GlicPinnedTabUsage>,
              GetPinnedTabUsage,
              (tabs::TabHandle),
              (override));
  MOCK_METHOD(void,
              GetContextFromTab,
              (tabs::TabHandle,
               const mojom::GetTabContextOptions&,
               base::OnceCallback<void(GlicGetContextResult)>),
              (override));
  MOCK_METHOD(void,
              GetContextForActorFromTab,
              (tabs::TabHandle,
               const mojom::GetTabContextOptions&,
               base::OnceCallback<void(GlicGetContextResult)>),
              (override));
  MOCK_METHOD(void,
              SubscribeToPinCandidates,
              (mojom::GetPinCandidatesOptionsPtr,
               mojo::PendingRemote<mojom::PinCandidatesObserver>),
              (override));
  MOCK_METHOD(void, OnConversationTurnSubmitted, (), (override));
  MOCK_METHOD(base::WeakPtr<GlicSharingManager>, GetWeakPtr, (), (override));
};

TEST_F(GlicDelegatingSharingManagerTest, DelegatedBehavior) {
  testing::NiceMock<MockGlicSharingManager> mock_delegate;
  GlicDelegatingSharingManager manager;

  manager.SetDelegate(&mock_delegate);

  // Verify GetFocusedTabData delegated
  FocusedTabData result = manager.GetFocusedTabData();
  EXPECT_FALSE(result.is_focus());
  EXPECT_EQ(result.GetFocus().error(), "mock");

  // Verify PinTabs delegated
  EXPECT_CALL(mock_delegate, PinTabs(testing::_, GlicPinTrigger::kUnknown))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(manager.PinTabs({}, GlicPinTrigger::kUnknown));

  // Verify UnpinTabs delegated
  EXPECT_CALL(mock_delegate, UnpinTabs(testing::_, GlicUnpinTrigger::kUnknown))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(manager.UnpinTabs({}, GlicUnpinTrigger::kUnknown));
}

}  // namespace

}  // namespace glic
