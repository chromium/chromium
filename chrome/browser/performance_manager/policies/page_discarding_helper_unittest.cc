// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {
namespace policies {

using CanDiscardResult = PageDiscardingHelper::CanDiscardResult;
using DiscardReason = PageDiscardingHelper::DiscardReason;
using ::testing::Return;

class PageDiscardingHelperTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  PageDiscardingHelperTest() = default;
  ~PageDiscardingHelperTest() override = default;
  PageDiscardingHelperTest(const PageDiscardingHelperTest& other) = delete;
  PageDiscardingHelperTest& operator=(const PageDiscardingHelperTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  // Helper to update the url of `page` and `frame` (or the default page_node()
  // and frame_node() on nullptr). In production a real navigation will notify
  // both.
  void SetPageAndFrameUrl(const GURL& url,
                          PageNodeImpl* page = nullptr,
                          FrameNodeImpl* frame = nullptr) {
    SetPageAndFrameUrlWithMimeType(url, "text/html", page, frame);
  }

  // Helper to update the url and Content-Type of `page` and `frame` (or the
  // default page_node() and frame_node() on nullptr). In production a real
  // navigation will notify both.
  void SetPageAndFrameUrlWithMimeType(const GURL& url,
                                      const std::string& mime_type,
                                      PageNodeImpl* page = nullptr,
                                      FrameNodeImpl* frame = nullptr) {
    page = page ? page : page_node();
    frame = frame ? frame : frame_node();
    page->OnMainFrameNavigationCommitted(
        false, base::TimeTicks::Now(), page->GetNavigationID() + 1, url,
        mime_type, /* notification_permission_status=*/
        blink::mojom::PermissionStatus::ASK);
    frame->OnNavigationCommitted(url, url::Origin::Create(url),
                                 /*same_document=*/false,
                                 /*is_served_from_back_forward_cache=*/false);
  }

  // Convenience wrappers for PageNodeHelper::CanDiscard().
  bool CanDiscard(const PageNode* page_node, DiscardReason discard_reason) {
    return PageDiscardingHelper::GetFromGraph(graph())->CanDiscard(
               page_node, discard_reason) == CanDiscardResult::kEligible;
  }

  bool CanDiscardWithMinimumTimeInBackground(
      const PageNode* page_node,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background) {
    return PageDiscardingHelper::GetFromGraph(graph())->CanDiscard(
               page_node, discard_reason, minimum_time_in_background) ==
           CanDiscardResult::kEligible;
  }

 protected:
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PageDiscardingHelperTest, TestCanDiscardMultipleCurrentMainFrames) {
  // TODO(crbug.com/40910297): It shouldn't be possible to have two main frames
  // both marked "current", but due to a state tracking bug this sometimes
  // occurs. Until the bug is fixed, make sure CanDiscard works around it. (See
  // comment at
  // https://source.chromium.org/chromium/chromium/src/+/main:components/performance_manager/graph/frame_node_impl.cc;l=272;drc=6d331b84c048659c6a9a89bd81e92dfdddd6bae7.)
  TestNodeWrapper<FrameNodeImpl> other_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node());

  // frame_node() is created with a URL. `other_frame_node` starts without.
  ASSERT_FALSE(frame_node()->GetURL().is_empty());
  ASSERT_TRUE(frame_node()->IsCurrent());
  ASSERT_TRUE(other_frame_node->GetURL().is_empty());
  ASSERT_TRUE(other_frame_node->IsCurrent());

  // An arbitrary "current" frame will be returned by GetMainFrameNode(). Make
  // sure the page can be discarded even if the one without a url is returned.
  // Discarding is only blocked if neither have a url.
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  SetPageAndFrameUrl(GURL(), page_node(), frame_node());

  ASSERT_TRUE(frame_node()->GetURL().is_empty());
  ASSERT_TRUE(frame_node()->IsCurrent());
  ASSERT_TRUE(other_frame_node->GetURL().is_empty());
  ASSERT_TRUE(other_frame_node->IsCurrent());

  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  SetPageAndFrameUrl(GURL("https://foo.com"), page_node(),
                     other_frame_node.get());

  ASSERT_TRUE(frame_node()->GetURL().is_empty());
  ASSERT_TRUE(frame_node()->IsCurrent());
  ASSERT_FALSE(other_frame_node->GetURL().is_empty());
  ASSERT_TRUE(other_frame_node->IsCurrent());

  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardVisiblePage) {
  page_node()->SetIsVisible(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardAudiblePage) {
  page_node()->SetIsAudible(true);
  // Ensure that the discard is being blocked because audio is playing, not
  // because GetTimeSinceLastAudibleChange() is recent.
  task_env().FastForwardBy(kTabAudioProtectionTime * 2);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageWithDiscardAttemptMarker) {
  PageDiscardingHelper::GetFromGraph(graph())
      ->AddDiscardAttemptMarkerForTesting(page_node());
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardRecentlyAudiblePage) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCanDiscardNeverAudiblePage) {
  // Ensure that if a page node is created without ever becoming audible, it
  // isn't marked as "recently playing audio". MakePageNodeDiscardable() which
  // is run on the default page_node() overrides audio properties, so need to
  // create a new page node and make it discardable by hand.
  TestNodeWrapper<PageNodeImpl> new_page_node = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> new_frame_node =
      CreateFrameNodeAutoId(process_node(), new_page_node.get());
  new_page_node->SetIsVisible(false);
  const GURL kUrl("https://example.com");
  new_page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), 42, kUrl, "text/html",
      /* notification_permission_status=*/blink::mojom::PermissionStatus::ASK);
  new_frame_node->OnNavigationCommitted(
      kUrl, url::Origin::Create(kUrl), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);

  EXPECT_FALSE(new_page_node->IsAudible());

  // Use a short `minimum_time_in_background` so that the page is discardable
  // but still created inside kTabAudioProtectionTime. It should NOT be blocked
  // from discarding due to kTabAudioProtectionTime.
  constexpr base::TimeDelta kMinTimeInBackground = kTabAudioProtectionTime / 2;
  task_env().FastForwardBy(kMinTimeInBackground);
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::URGENT, kMinTimeInBackground));
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::PROACTIVE, kMinTimeInBackground));
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::EXTERNAL, kMinTimeInBackground));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardRecentlyVisiblePageUnlessExplicitlyRequested) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Seconds(1));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::URGENT, base::Seconds(1)));
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::PROACTIVE, base::Seconds(1)));
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::EXTERNAL, base::Seconds(1)));
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPdf) {
  SetPageAndFrameUrlWithMimeType(GURL("https://foo.com/doc.pdf"),
                                 "application/pdf");
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithoutMainFrame) {
  ResetFrameNode();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardExtension) {
  SetPageAndFrameUrl(GURL("chrome-extension://foo"));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithInvalidURL) {
  SetPageAndFrameUrl(GURL("foo42"));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageProtectedByExtension) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsAutoDiscardableForTesting(false);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageMultipleTimes) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetWasDiscardedForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithFormInteractions) {
  frame_node()->SetHadFormInteraction();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithUserEdits) {
  frame_node()->SetHadUserEdits();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardActiveTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsActiveTabForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotProactivelyDiscardWithNotificationPermission) {
  // The page is discardable if notification permission is denied.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  // The page is discardable if notification permission is granted.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageOnNoDiscardList) {
  // static_cast page_node() because it's declared as a PageNodeImpl which hides
  // the members it overrides from PageNode.
  const auto* page = static_cast<const PageNode*>(page_node());
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {"youtube.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  SetPageAndFrameUrl(GURL("https://www.example.com"));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  // Changing the no discard list rebuilds the matcher
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {"google.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
  SetPageAndFrameUrl(GURL("https://www.google.com"));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));

  // Setting the no discard list to empty makes all URLs discardable again.
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {});
  SetPageAndFrameUrl(GURL("https://www.google.com"));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPinnedTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsPinnedTabForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardWithDevToolsOpen) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsDevToolsOpenForTesting(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotProactivelyDiscardAfterUpdatedTitleOrFaviconInBackground) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetUpdatedTitleOrFaviconInBackgroundForTesting(true);
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardWithPictureInPicture) {
  page_node()->SetHasPictureInPicture(true);
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

// Tests DiscardMultiplePages.

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesNoCandidate) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is false, protected page can not be discarded.
  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1024),
      /*discard_protected_tabs*/ false,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_FALSE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesDiscardProtected) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is true, protected page can be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1024),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesTwoCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  EXPECT_TRUE(CanDiscard(page_node2.get(), DiscardReason::URGENT));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // 2 candidates should both be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 2048),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesTwoCandidatesProtected) {
  // page_node() is audible and should not be discarded.
  page_node()->SetIsAudible(true);

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  EXPECT_TRUE(CanDiscard(page_node2.get(), DiscardReason::URGENT));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // When discard_protected_tabs is false, it should not discard protected page
  // even with large reclaim_target_kb.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1000000),
      /*discard_protected_tabs*/ false,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesThreeCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  auto process_node3 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());

  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  // |page_node3| is the most recently visible page.
  page_node3->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node3->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);
  process_node3->set_resident_set_kb(1024);

  // The 2 candidates with earlier last visible time should be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1500),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 3,
                                        1);
}

TEST_F(PageDiscardingHelperTest,
       DiscardMultiplePagesThreeCandidatesWithPriority) {
  // page_node() is audible and should have lower discard priority.
  page_node()->SetIsAudible(true);

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  auto process_node3 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());

  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  // |page_node3| is the most recently visible page.
  page_node3->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node3->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);
  process_node3->set_resident_set_kb(1024);

  // Protected pages should have lower discard priority.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node3.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1500),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesNoDiscardable) {
  // DiscardMultiplePages should not retry indefinitely when all nodes
  // are not discardable.

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // Discarding failed on all nodes.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(false));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 10240),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_FALSE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

// Tests DiscardAPage.

TEST_F(PageDiscardingHelperTest, DiscardAPageNoCandidate) {
  page_node()->SetIsVisible(true);
  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_FALSE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardAPageSingleCandidate) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);
}

TEST_F(PageDiscardingHelperTest, DiscardAPageSingleCandidateFails) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_FALSE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // There should be 2 discard attempts, during the first one an attempt will be
  // made to discard |page_node()|, on the second attempt no discard candidate
  // should be found.
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);

  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 0,
                                        1);
}

TEST_F(PageDiscardingHelperTest, DiscardAPageTwoCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node2| is the most recently visible page.
  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(CanDiscard(page_node2.get(), DiscardReason::URGENT));
  EXPECT_GT(page_node()->GetTimeSinceLastVisibilityChange(),
            page_node2->GetTimeSinceLastVisibilityChange());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 2,
                                        1);
}

TEST_F(PageDiscardingHelperTest, DiscardAPageTwoCandidatesFirstFails) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // Pretends that the first discardable page hasn't been discarded
  // successfully, the other one should be discarded in this case.
  ::testing::InSequence in_sequence;
  // The first candidate is the least recently used tab.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardAPageTwoCandidatesMultipleFrames) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  // Adds a second frame to |page_node()| and host it in |process_node2|.
  auto page_node1_extra_frame =
      CreateFrameNodeAutoId(process_node2.get(), page_node(), frame_node());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // The total RSS of |page_node()| should be 1024 + 2048 / 2 = 2048 and the
  // RSS of |page_node2| should be 2048 / 2 = 1024, so |page_node()| will get
  // discarded despite having spent less time in background than |page_node2|.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardAPageTwoCandidatesNoRSSData) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node()| is the most recently visible page.
  page_node()->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_GT(page_node2->GetTimeSinceLastVisibilityChange(),
            page_node()->GetTimeSinceLastVisibilityChange());

  // |page_node2| should be discarded as there's no RSS data for any of the
  // pages and it's the least recently visible page.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

// Tests DiscardMultiplePages with reclaim_target_kb == nullopt.

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesTwoCandidatesNoRSSData) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node()| is the most recently visible page.
  page_node()->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_GT(page_node2->GetTimeSinceLastVisibilityChange(),
            page_node()->GetTimeSinceLastVisibilityChange());

  // |page_node2| should be discarded as there's no RSS data for any of the
  // pages and it's the least recently visible page.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      /*reclaim_target*/ std::nullopt,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, DiscardingProtectedTabReported) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Page node 2 is still audible but has not been visible for 30 minutes. It
  // should be protected but the lower priority tab and should be discarded.
  page_node2->SetIsVisible(true);
  page_node2->SetIsAudible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  // Set the primary page node to visible so it is higher priority than
  // page_node2.
  page_node()->SetIsVisible(true);

  process_node2->set_resident_set_kb(1024);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardingProtectedTab",
                                        true, 1);
  histogram_tester()->ExpectBucketCount("Discarding.DiscardingProtectedTab",
                                        false, 0);
}

TEST_F(PageDiscardingHelperTest, DiscardingUnprotectedTabReported) {
  // By default the primary page node is not protected.

  process_node()->set_resident_set_kb(1024);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardingProtectedTab",
                                        true, 0);
  histogram_tester()->ExpectBucketCount("Discarding.DiscardingProtectedTab",
                                        false, 1);
}

TEST_F(PageDiscardingHelperTest, DiscardingFocusedTabReported) {
  process_node()->set_resident_set_kb(1024);
  page_node()->SetIsVisible(true);
  page_node()->SetIsFocused(true);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab", true,
                                        1);
  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab",
                                        false, 0);
}

TEST_F(PageDiscardingHelperTest, DiscardingUnfocusedTabReported) {
  // Main process node is not focused by default.
  process_node()->set_resident_set_kb(1024);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
      memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](std::optional<base::TimeTicks> first_discarded_at) {
        EXPECT_TRUE(first_discarded_at.has_value());
      }),
      DiscardReason::URGENT);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab", true,
                                        0);
  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab",
                                        false, 1);
}

}  // namespace policies
}  // namespace performance_manager
