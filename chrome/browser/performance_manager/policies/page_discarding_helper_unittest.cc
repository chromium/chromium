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
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {
namespace policies {

using DiscardReason = PageDiscardingHelper::DiscardReason;
using ::testing::Contains;
using ::testing::Return;

TEST(PageNodeSortProxyTest, Order) {
  // Disabled tab is never discarded over focused tabs.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                true, base::Seconds(1)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kDisallowed, false,
                                false, base::Seconds(10)));
  // Focused tab is more important than visible & non-focused tab.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                false, base::Seconds(1)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                true, base::Seconds(10)));
  // Visible tab is more important than protected & non-visible tab.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, false,
                                false, base::Seconds(1)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                false, base::Seconds(10)));
  // Protected tab is more important than non-protected tab.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kEligible, false,
                                false, base::Seconds(1)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, false,
                                false, base::Seconds(10)));

  // Compare disabled tabs.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kDisallowed, false,
                                false, base::Seconds(10)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kDisallowed, false,
                                false, base::Seconds(1)));
  // Sort visible tabs based on last_visible_.
  // TODO(crbug.com/391243672): use focus status change instead of
  // last_visible_.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                false, base::Seconds(10)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, true,
                                false, base::Seconds(1)));
  // Sort protected tabs based on last_visible_.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, false,
                                false, base::Seconds(10)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kProtected, false,
                                false, base::Seconds(1)));
  // Sort non-protected tabs based on last_visible_.
  EXPECT_TRUE(PageNodeSortProxy(nullptr, CanDiscardResult::kEligible, false,
                                false, base::Seconds(10)) <
              PageNodeSortProxy(nullptr, CanDiscardResult::kEligible, false,
                                false, base::Seconds(1)));
}

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
  bool CanDiscard(
      const PageNode* page_node,
      DiscardReason discard_reason,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) {
    return PageDiscardingHelper::GetFromGraph(graph())->CanDiscard(
               page_node, discard_reason, kNonVisiblePagesUrgentProtectionTime,
               cannot_discard_reasons) == CanDiscardResult::kEligible;
  }

  bool CanDiscardWithMinimumTimeInBackground(
      const PageNode* page_node,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) {
    return PageDiscardingHelper::GetFromGraph(graph())->CanDiscard(
               page_node, discard_reason, minimum_time_in_background,
               cannot_discard_reasons) == CanDiscardResult::kEligible;
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
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kVisible));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kVisible));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardAudiblePage) {
  page_node()->SetIsAudible(true);
  // Ensure that the discard is being blocked because audio is playing, not
  // because GetTimeSinceLastAudibleChange() is recent.
  task_env().FastForwardBy(kTabAudioProtectionTime * 2);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAudible));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAudible));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageWithDiscardAttemptMarker) {
  PageDiscardingHelper::GetFromGraph(graph())
      ->AddDiscardAttemptMarkerForTesting(page_node());
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardRecentlyAudiblePage) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyAudible));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyAudible));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCanDiscardNeverAudiblePage) {
  // Ensure that if a page node is created without ever becoming audible, it
  // isn't marked as "recently playing audio". MakePageNodeDiscardable() which
  // is run on the default page_node() overrides audio properties, so need to
  // create a new page node and make it discardable by hand.
  TestNodeWrapper<PageNodeImpl> new_page_node = CreateNode<PageNodeImpl>();
  new_page_node->SetType(PageType::kTab);
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
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::URGENT, kMinTimeInBackground,
      &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::PROACTIVE, kMinTimeInBackground,
      &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      new_page_node.get(), DiscardReason::EXTERNAL, kMinTimeInBackground,
      &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardRecentlyVisiblePageUnlessExplicitlyRequested) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Seconds(1));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyVisible));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyVisible));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::URGENT, base::Seconds(1), &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::PROACTIVE, base::Seconds(1), &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscardWithMinimumTimeInBackground(
      page_node(), DiscardReason::EXTERNAL, base::Seconds(1), &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPdf) {
  SetPageAndFrameUrlWithMimeType(GURL("https://foo.com/doc.pdf"),
                                 "application/pdf");
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPdf));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPdf));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithoutMainFrame) {
  ResetFrameNode();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardExtension) {
  SetPageAndFrameUrl(GURL("chrome-extension://foo"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNotWebOrInternal));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNotWebOrInternal));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithInvalidURL) {
  SetPageAndFrameUrl(GURL("foo42"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kInvalidURL));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kInvalidURL));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageProtectedByExtension) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsAutoDiscardableForTesting(false);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kExtensionProtected));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kExtensionProtected));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingVideo));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingVideo));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingAudio));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingAudio));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBeingMirrored));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBeingMirrored));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingWindow));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingWindow));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingDisplay));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingDisplay));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kConnectedToBluetooth));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kConnectedToBluetooth));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kConnectedToUSB));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kConnectedToUSB));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

#if !BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/391179510): Remove this test if the WasDiscarded() property is
// removed.
TEST_F(PageDiscardingHelperTest, DISABLED_TestCannotDiscardPageMultipleTimes) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetWasDiscardedForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kWasDiscarded));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kWasDiscarded));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithFormInteractions) {
  frame_node()->SetHadFormInteraction();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kFormInteractions));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kFormInteractions));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithUserEdits) {
  frame_node()->SetHadUserEdits();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kUserEdits));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kUserEdits));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardActiveTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsActiveTabForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kActiveTab));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kActiveTab));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest,
       TestCannotProactivelyDiscardWithNotificationPermission) {
  // The page is discardable if notification permission is denied.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // The page is discardable if notification permission is granted.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);
  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kNotificationsEnabled));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageOnNoDiscardList) {
  // static_cast page_node() because it's declared as a PageNodeImpl which hides
  // the members it overrides from PageNode.
  const auto* page = static_cast<const PageNode*>(page_node());
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {"youtube.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  SetPageAndFrameUrl(GURL("https://www.example.com"));
  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // Changing the no discard list rebuilds the matcher
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {"google.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  SetPageAndFrameUrl(GURL("https://www.google.com"));
  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // Setting the no discard list to empty makes all URLs discardable again.
  PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
      page->GetBrowserContextID(), {});
  SetPageAndFrameUrl(GURL("https://www.google.com"));
  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPinnedTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsPinnedTabForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPinnedTab));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPinnedTab));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardWithDevToolsOpen) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsDevToolsOpenForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDevToolsOpen));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDevToolsOpen));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest,
       TestCannotProactivelyDiscardAfterUpdatedTitleOrFaviconInBackground) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetUpdatedTitleOrFaviconInBackgroundForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBackgroundActivity));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardWithPictureInPicture) {
  page_node()->SetHasPictureInPicture(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPictureInPicture));

  reasons_vec.clear();
  EXPECT_FALSE(CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPictureInPicture));

  reasons_vec.clear();
  EXPECT_TRUE(CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

// Tests DiscardMultiplePages.

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesNoCandidate) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is false, protected page can not be discarded.
  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1024),
          /*discard_protected_tabs*/ false, DiscardReason::URGENT);
  EXPECT_FALSE(first_discarded_at.has_value());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesDiscardProtected) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is true, protected page can be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1024),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);

  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 2048),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1000000),
          /*discard_protected_tabs*/ false, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1500),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1500),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
}

TEST_F(PageDiscardingHelperTest, DiscardMultiplePagesNoDiscardable) {
  // DiscardMultiplePages should not retry indefinitely when all nodes
  // are not discardable.

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  page_node2->SetType(PageType::kTab);
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // Discarding failed on all nodes.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(false));

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 10240),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_FALSE(first_discarded_at.has_value());
}

// Tests DiscardAPage.

TEST_F(PageDiscardingHelperTest, DiscardAPageNoCandidate) {
  page_node()->SetIsVisible(true);
  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_FALSE(first_discarded_at.has_value());
}

TEST_F(PageDiscardingHelperTest, DiscardAPageSingleCandidate) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);
}

TEST_F(PageDiscardingHelperTest, DiscardAPageSingleCandidateFails) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_FALSE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());

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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardAPage(
          DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          /*reclaim_target*/ std::nullopt,
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());
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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());

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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());

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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());

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

  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(graph())->DiscardMultiplePages(
          memory_pressure::ReclaimTarget(/*reclaim_target_kb*/ 1),
          /*discard_protected_tabs*/ true, DiscardReason::URGENT);
  EXPECT_TRUE(first_discarded_at.has_value());

  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab", true,
                                        0);
  histogram_tester()->ExpectBucketCount("Discarding.DiscardingFocusedTab",
                                        false, 1);
}

}  // namespace policies
}  // namespace performance_manager
