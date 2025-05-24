// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
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

namespace performance_manager::policies {

using DiscardReason = DiscardEligibilityPolicy::DiscardReason;
using CanDiscardResult::kDisallowed;
using CanDiscardResult::kEligible;
using CanDiscardResult::kProtected;
using ::testing::Contains;
using ::testing::Return;

TEST(PageNodeSortProxyTest, Order) {
  auto absolute_time = [](int seconds) {
    return base::TimeTicks() + base::Seconds(seconds);
  };

  // Disabled tab is never discarded over focused tabs.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kProtected, true, true, absolute_time(10)) <
      PageNodeSortProxy(nullptr, kDisallowed, false, false, absolute_time(1)));
  // Focused tab is more important than visible & non-focused tab.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kProtected, true, false, absolute_time(10)) <
      PageNodeSortProxy(nullptr, kProtected, true, true, absolute_time(1)));
  // Visible tab is more important than protected & non-visible tab.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kProtected, false, false, absolute_time(10)) <
      PageNodeSortProxy(nullptr, kProtected, true, false, absolute_time(1)));
  // Protected tab is more important than non-protected tab.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kEligible, false, false, absolute_time(10)) <
      PageNodeSortProxy(nullptr, kProtected, false, false, absolute_time(1)));

  // Compare disabled tabs.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kDisallowed, false, false, absolute_time(1)) <
      PageNodeSortProxy(nullptr, kDisallowed, false, false, absolute_time(10)));
  // Sort visible tabs based on `last_visibility_change_time_`.
  // TODO(crbug.com/391243672): use focus status change instead of
  // last_visible_.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kProtected, true, false, absolute_time(1)) <
      PageNodeSortProxy(nullptr, kProtected, true, false, absolute_time(10)));
  // Sort protected tabs based on `last_visibility_change_time_`.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kProtected, false, false, absolute_time(1)) <
      PageNodeSortProxy(nullptr, kProtected, false, false, absolute_time(10)));
  // Sort non-protected tabs based on `last_visibility_change_time_`.
  EXPECT_TRUE(
      PageNodeSortProxy(nullptr, kEligible, false, false, absolute_time(1)) <
      PageNodeSortProxy(nullptr, kEligible, false, false, absolute_time(10)));
}

class DiscardEligibilityPolicyTest
    : public testing::GraphTestHarnessWithDiscardablePage {
 public:
  DiscardEligibilityPolicyTest() = default;
  ~DiscardEligibilityPolicyTest() override = default;
  DiscardEligibilityPolicyTest(const DiscardEligibilityPolicyTest& other) =
      delete;
  DiscardEligibilityPolicyTest& operator=(const DiscardEligibilityPolicyTest&) =
      delete;

  void SetUp() override {
    testing::GraphTestHarnessWithDiscardablePage::SetUp();
  }

  void TearDown() override {
    testing::GraphTestHarnessWithDiscardablePage::TearDown();
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

  // Convenience wrappers for DiscardEligibilityPolicy::CanDiscard().
  CanDiscardResult CanDiscard(
      const PageNode* page_node,
      DiscardReason discard_reason,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) {
    return DiscardEligibilityPolicy::GetFromGraph(graph())->CanDiscard(
        page_node, discard_reason, kNonVisiblePagesUrgentProtectionTime,
        cannot_discard_reasons);
  }

  CanDiscardResult CanDiscardWithMinimumTimeInBackground(
      const PageNode* page_node,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) {
    return DiscardEligibilityPolicy::GetFromGraph(graph())->CanDiscard(
        page_node, discard_reason, minimum_time_in_background,
        cannot_discard_reasons);
  }
};

TEST_F(DiscardEligibilityPolicyTest, TestCanDiscardMultipleCurrentMainFrames) {
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
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::EXTERNAL));

  SetPageAndFrameUrl(GURL(), page_node(), frame_node());

  ASSERT_TRUE(frame_node()->GetURL().is_empty());
  ASSERT_TRUE(frame_node()->IsCurrent());
  ASSERT_TRUE(other_frame_node->GetURL().is_empty());
  ASSERT_TRUE(other_frame_node->IsCurrent());

  EXPECT_EQ(kProtected, CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_EQ(kProtected, CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::EXTERNAL));

  SetPageAndFrameUrl(GURL("https://foo.com"), page_node(),
                     other_frame_node.get());

  ASSERT_TRUE(frame_node()->GetURL().is_empty());
  ASSERT_TRUE(frame_node()->IsCurrent());
  ASSERT_FALSE(other_frame_node->GetURL().is_empty());
  ASSERT_TRUE(other_frame_node->IsCurrent());

  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::URGENT));
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::PROACTIVE));
  EXPECT_EQ(kEligible, CanDiscard(page_node(), DiscardReason::EXTERNAL));
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardVisiblePage) {
  page_node()->SetIsVisible(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kVisible));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kVisible));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardAudiblePage) {
  page_node()->SetIsAudible(true);
  // Ensure that the discard is being blocked because audio is playing, not
  // because GetTimeSinceLastAudibleChange() is recent.
  task_env().FastForwardBy(kTabAudioProtectionTime * 2);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAudible));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAudible));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotDiscardPageWithDiscardAttemptMarker) {
  DiscardEligibilityPolicy::AddDiscardAttemptMarker(page_node());
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDiscardAttempted));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardRecentlyAudiblePage) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyAudible));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyAudible));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}
#endif

TEST_F(DiscardEligibilityPolicyTest, TestCanDiscardNeverAudiblePage) {
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
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           new_page_node.get(), DiscardReason::URGENT,
                           kMinTimeInBackground, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           new_page_node.get(), DiscardReason::PROACTIVE,
                           kMinTimeInBackground, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           new_page_node.get(), DiscardReason::EXTERNAL,
                           kMinTimeInBackground, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(DiscardEligibilityPolicyTest,
       TestCannotDiscardRecentlyVisiblePageUnlessExplicitlyRequested) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Seconds(1));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyVisible));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kRecentlyVisible));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           page_node(), DiscardReason::URGENT, base::Seconds(1),
                           &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           page_node(), DiscardReason::PROACTIVE,
                           base::Seconds(1), &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible, CanDiscardWithMinimumTimeInBackground(
                           page_node(), DiscardReason::EXTERNAL,
                           base::Seconds(1), &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}
#endif

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPdf) {
  SetPageAndFrameUrlWithMimeType(GURL("https://foo.com/doc.pdf"),
                                 "application/pdf");
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPdf));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPdf));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageWithoutMainFrame) {
  ResetFrameNode();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNoMainFrame));
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageAlreadyDiscarded) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsDiscardedForTesting(true);

  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAlreadyDiscarded));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAlreadyDiscarded));

  reasons_vec.clear();
  EXPECT_EQ(kDisallowed,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kAlreadyDiscarded));
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardExtension) {
  SetPageAndFrameUrl(GURL("chrome-extension://foo"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNotWebOrInternal));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kNotWebOrInternal));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageWithInvalidURL) {
  SetPageAndFrameUrl(GURL("foo42"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kInvalidURL));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kInvalidURL));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotDiscardPageProtectedByExtension) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsAutoDiscardableForTesting(false);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kExtensionProtected));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kExtensionProtected));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingVideo));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingVideo));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingAudio));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingAudio));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBeingMirrored));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBeingMirrored));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingWindow));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingWindow));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingDisplay));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kCapturingDisplay));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotDiscardPageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kConnectedToBluetooth));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kConnectedToBluetooth));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kConnectedToUSB));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kConnectedToUSB));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotDiscardPageWithFormInteractions) {
  frame_node()->SetHadFormInteraction();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kFormInteractions));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kFormInteractions));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageWithUserEdits) {
  frame_node()->SetHadUserEdits();
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kUserEdits));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kUserEdits));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardActiveTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsActiveTabForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kActiveTab));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kActiveTab));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotProactivelyDiscardWithNotificationPermission) {
  // The page is discardable if notification permission is denied.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // The page is discardable if notification permission is granted.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);
  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec,
              Contains(CannotDiscardReason::kNotificationsEnabled));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPageOnNoDiscardList) {
  // static_cast page_node() because it's declared as a PageNodeImpl which hides
  // the members it overrides from PageNode.
  const auto* page = static_cast<const PageNode*>(page_node());
  DiscardEligibilityPolicy::GetFromGraph(graph())
      ->SetNoDiscardPatternsForProfile(page->GetBrowserContextID(),
                                       {"youtube.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  SetPageAndFrameUrl(GURL("https://www.example.com"));
  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // Changing the no discard list rebuilds the matcher
  DiscardEligibilityPolicy::GetFromGraph(graph())
      ->SetNoDiscardPatternsForProfile(page->GetBrowserContextID(),
                                       {"google.com"});
  SetPageAndFrameUrl(GURL("https://www.youtube.com"));
  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  SetPageAndFrameUrl(GURL("https://www.google.com"));
  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kOptedOut));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  // Setting the no discard list to empty makes all URLs discardable again.
  DiscardEligibilityPolicy::GetFromGraph(graph())
      ->SetNoDiscardPatternsForProfile(page->GetBrowserContextID(), {});
  SetPageAndFrameUrl(GURL("https://www.google.com"));
  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardPinnedTab) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsPinnedTabForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPinnedTab));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPinnedTab));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardWithDevToolsOpen) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsDevToolsOpenForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDevToolsOpen));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kDevToolsOpen));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest,
       TestCannotProactivelyDiscardAfterUpdatedTitleOrFaviconInBackground) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetUpdatedTitleOrFaviconInBackgroundForTesting(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kBackgroundActivity));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

TEST_F(DiscardEligibilityPolicyTest, TestCannotDiscardWithPictureInPicture) {
  page_node()->SetHasPictureInPicture(true);
  std::vector<CannotDiscardReason> reasons_vec;
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::URGENT, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPictureInPicture));

  reasons_vec.clear();
  EXPECT_EQ(kProtected,
            CanDiscard(page_node(), DiscardReason::PROACTIVE, &reasons_vec));
  EXPECT_THAT(reasons_vec, Contains(CannotDiscardReason::kPictureInPicture));

  reasons_vec.clear();
  EXPECT_EQ(kEligible,
            CanDiscard(page_node(), DiscardReason::EXTERNAL, &reasons_vec));
  EXPECT_TRUE(reasons_vec.empty());
}

}  // namespace performance_manager::policies
