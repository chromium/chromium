// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_anchor.h"

namespace contextual_tasks {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnchorElementId);

constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);

}  // namespace

class ContextualTasksExtensionsContainerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

  void TearDown() override {
    browser_window_.reset();
    profile_.reset();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  std::unique_ptr<ContextualTasksExtensionsContainer> CreateContainer(
      content::WebContents* web_contents,
      ContextualTasksExtensionsContainer::AnchorProvider anchor_provider =
          base::NullCallback()) {
    return std::make_unique<ContextualTasksExtensionsContainer>(
        browser_window_.get(), web_contents, std::move(anchor_provider));
  }

  // Returns an `AnchorProvider` that resolves `kTestAnchorElementId` the same
  // way production code resolves the side panel's super G button, and records
  // how many times it ran in `run_count`.
  ContextualTasksExtensionsContainer::AnchorProvider CreateAnchorProvider(
      int* run_count) {
    return base::BindLambdaForTesting([run_count]() {
      ++*run_count;
      ui::TrackedElement* element =
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              kTestAnchorElementId, kTestContext);
      return element ? views::BubbleAnchor(element) : views::BubbleAnchor();
    });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_window_;
};

TEST_F(ContextualTasksExtensionsContainerTest, GetAndSetActiveWebContents) {
  auto web_contents1 = CreateTestWebContents();
  auto web_contents2 = CreateTestWebContents();

  auto container = CreateContainer(web_contents1.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents1.get());

  container->SetWebContents(web_contents2.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents2.get());

  container->SetWebContents(nullptr);
  EXPECT_EQ(container->GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, WebContentsWeakPtrLifecycle) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents.get());

  // Destroying the WebContents should safely clear the weak pointer.
  web_contents.reset();
  EXPECT_EQ(container->GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, ContainerDefaults) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  EXPECT_TRUE(container->IsVisible());
  EXPECT_FALSE(container->HasAnyExtensions());
  EXPECT_EQ(container->GetActionForId("test_extension"), nullptr);
  EXPECT_FALSE(container->IsActionVisibleOnToolbar("test_extension"));
  EXPECT_EQ(container->GetPoppedOutActionId(), std::nullopt);
  EXPECT_FALSE(container->ShowToolbarActionPopupForAPICall("test_extension",
                                                           base::DoNothing()));
}

TEST_F(ContextualTasksExtensionsContainerTest, PopOutActionRunsCallback) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  bool called = false;
  container->PopOutAction("test_extension",
                          base::BindLambdaForTesting([&]() { called = true; }));
  EXPECT_TRUE(called);
}

TEST_F(ContextualTasksExtensionsContainerTest, GetPopupArrow) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());
  EXPECT_EQ(container->GetPopupArrow(), views::BubbleBorder::TOP_LEFT);
}

TEST_F(ContextualTasksExtensionsContainerTest, NullAnchorProvider) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  EXPECT_TRUE(container->GetExtensionsButtonAnchor().IsNull());
  EXPECT_TRUE(container->GetReferenceButtonForPopup("test_extension").IsNull());
}

TEST_F(ContextualTasksExtensionsContainerTest, AnchorIsResolvedOnEveryUse) {
  auto web_contents = CreateTestWebContents();
  int run_count = 0;
  auto container =
      CreateContainer(web_contents.get(), CreateAnchorProvider(&run_count));

  ui::test::TestElement element(kTestAnchorElementId, kTestContext);
  element.Show();

  EXPECT_EQ(container->GetExtensionsButtonAnchor().GetIfElement(), &element);
  EXPECT_EQ(run_count, 1);

  // The anchor must be re-resolved rather than served from a cached copy.
  EXPECT_EQ(
      container->GetReferenceButtonForPopup("test_extension").GetIfElement(),
      &element);
  EXPECT_EQ(run_count, 2);
}

// Regression test for a dangling `raw_ptr`: the container used to cache the
// `views::BubbleAnchor` it was shown with, which outlived the WebUI
// `ui::TrackedElement` backing it when the side panel document went away.
TEST_F(ContextualTasksExtensionsContainerTest,
       AnchorIsNotCachedAcrossElementDestruction) {
  auto web_contents = CreateTestWebContents();
  int run_count = 0;
  auto container =
      CreateContainer(web_contents.get(), CreateAnchorProvider(&run_count));

  {
    ui::test::TestElement element(kTestAnchorElementId, kTestContext);
    element.Show();
    EXPECT_FALSE(container->GetExtensionsButtonAnchor().IsNull());
  }

  // The element is gone, as it would be after the side panel WebUI document is
  // destroyed. The container must report a null anchor instead of handing back
  // a pointer to freed memory.
  EXPECT_TRUE(container->GetExtensionsButtonAnchor().IsNull());
  EXPECT_TRUE(container->GetReferenceButtonForPopup("test_extension").IsNull());

  // Showing the menu with no valid anchor must be a no-op rather than a crash.
  container->ShowExtensionsMenu();
  EXPECT_FALSE(container->IsExtensionsMenuShowing());
}

}  // namespace contextual_tasks
