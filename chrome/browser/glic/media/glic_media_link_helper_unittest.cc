// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_link_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/mock_media_session.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

// Subclass for `GlicMediaLinkHelper` that lets us provide the `MediaSession`
// object directly, since trying to get MediaSession::FromWebContents() to
// return a mock is basically impossible without some fairly big changes.
class GlicMediaLinkHelperForTest : public GlicMediaLinkHelper {
 public:
  GlicMediaLinkHelperForTest(content::WebContents* web_contents,
                             content::MediaSession* media_session)
      : GlicMediaLinkHelper(web_contents), media_session_(media_session) {}

  content::MediaSession* GetMediaSessionIfExists() override {
    return media_session_;
  }

  void clear_media_session() { media_session_ = nullptr; }

 private:
  raw_ptr<content::MediaSession> media_session_ = nullptr;
};

class GlicMediaLinkHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicMediaLinkHelperTest() = default;
  ~GlicMediaLinkHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    media_link_helper_ = std::make_unique<GlicMediaLinkHelperForTest>(
        web_contents(), &mock_media_session_);
  }

  void TearDown() override {
    media_link_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::MockMediaSession& mock_media_session() {
    return mock_media_session_;
  }

  GlicMediaLinkHelperForTest* media_link_helper() {
    return media_link_helper_.get();
  }

  void CreateAndAttachRoutedSubframe(const GURL& subframe_url) {
    // Set up a subframe with an embed URL.
    content::RenderFrameHost* subframe =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            subframe_url, content::RenderFrameHostTester::For(
                              web_contents()->GetPrimaryMainFrame())
                              ->AppendChild("subframe"));

    ON_CALL(mock_media_session(), GetRoutedFrame)
        .WillByDefault(Return(subframe));
  }

 protected:
  content::MockMediaSession mock_media_session_;
  std::unique_ptr<GlicMediaLinkHelperForTest> media_link_helper_;
};

TEST_F(GlicMediaLinkHelperTest, DifferentHostsReturnsFalse) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());

  // Case 1: Committed is example.com, target is youtube.com
  web_contents_tester->NavigateAndCommit(GURL("https://www.example.com/page"));
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123")));

  // Case 2: Different subdomains of YouTube are considered different hosts.
  web_contents_tester->NavigateAndCommit(
      GURL("https://m.youtube.com/watch?v=video123"));
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, SameOriginNoHelperReturnsFalse) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL("https://www.example.com/page"));

  // example.com is not in g_origin_helpers
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.example.com/another_page")));
}

// --- YouTubeEmbedHelper Specific Tests (via MaybeReplaceNavigation) ---

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMediaLinkEmbedHelper);

  // Navigate main frame to something else.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  // The target is a regular YT link. Since the feature is off, this should
  // fall through to the normal helper, which will fail because the main
  // frame's host is example.com.
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_SuccessWithWatchTarget) {
  base::test::ScopedFeatureList feature_list(kMediaLinkEmbedHelper);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_SuccessWithEmbedTarget) {
  base::test::ScopedFeatureList feature_list(kMediaLinkEmbedHelper);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(45)))).Times(1);

  EXPECT_TRUE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/embed/video123?t=45")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_MismatchedVideoId) {
  base::test::ScopedFeatureList feature_list(kMediaLinkEmbedHelper);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=DIFFERENT&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_NoTimeParam) {
  base::test::ScopedFeatureList feature_list(kMediaLinkEmbedHelper);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeEmbedHelper_NoRoutedFrame) {
  base::test::ScopedFeatureList feature_list(kMediaLinkEmbedHelper);

  ON_CALL(mock_media_session(), GetRoutedFrame).WillByDefault(Return(nullptr));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_DifferentSchemeSucceeds) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("http://www.youtube.com/watch?v=video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_MobileYouTubeIsNotHandled) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://m.youtube.com/watch?v=video123"));

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://m.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_NoVideoIdInCommittedUrl) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  // Committed URL without 'v=' parameter
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/feed/subscriptions"));

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_NoVideoIdInTargetUrl) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL without 'v=' parameter
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/feed/subscriptions?t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_DifferentVideoIds) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=different&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_EmptyVideoId) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());

  // Committed URL with empty 'v='
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v="));
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));

  // Target URL with empty 'v='
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_NoTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL without 't=' parameter
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&q=test")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_EmptyTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL with empty 't=' parameter
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_NonIntegerTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL with non-integer 't=' parameter
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=abc")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_NoMediaSession) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // No MediaSession attached, so MediaSession::GetIfExists will return nullptr.
  media_link_helper()->clear_media_session();
  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=60")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_WithMediaSession) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Attach a mock media session and expect SeekTo to be called.
  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30")));
}

TEST_F(GlicMediaLinkHelperTest, YouTubeHelper_WithMediaSession_ZeroTime) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(0)))).Times(1);

  EXPECT_TRUE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=0")));
}

TEST_F(GlicMediaLinkHelperTest,
       YouTubeHelper_WithMediaSession_NegativeTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Expect SeekTo to NOT be called because base::StringToUint fails for
  // negative numbers.
  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=-10")));
}

TEST_F(GlicMediaLinkHelperTest,
       YouTubeHelper_WithMediaSession_FractionalTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Expect SeekTo to NOT be called because base::StringToUint fails for
  // non-integers, which also don't work for `t=` parameters.
  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(media_link_helper()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=10.5")));
}

// Test fixture for static methods on GlicMediaLinkHelper.
class GlicMediaLinkHelperStaticTest : public testing::Test {};

TEST_F(GlicMediaLinkHelperStaticTest, ExtractVideoNameIfExists) {
  // Test case for a standard YouTube "watch" URL.
  GURL watch_url("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  std::optional<std::string> video_name =
      GlicMediaLinkHelper::ExtractVideoNameIfExists(watch_url);
  ASSERT_TRUE(video_name.has_value());
  EXPECT_EQ(*video_name, "dQw4w9WgXcQ");

  // Test case for a YouTube "embed" URL.
  GURL embed_url("https://www.youtube.com/embed/dQw4w9WgXcQ");
  video_name = GlicMediaLinkHelper::ExtractVideoNameIfExists(embed_url);
  ASSERT_TRUE(video_name.has_value());
  EXPECT_EQ(*video_name, "dQw4w9WgXcQ");

  // Test case with other query parameters.
  GURL watch_url_with_time("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42s");
  video_name =
      GlicMediaLinkHelper::ExtractVideoNameIfExists(watch_url_with_time);
  ASSERT_TRUE(video_name.has_value());
  EXPECT_EQ(*video_name, "dQw4w9WgXcQ");

  // Test case with no 'v' parameter.
  GURL no_v_param("https://www.youtube.com/watch?feature=youtu.be");
  video_name = GlicMediaLinkHelper::ExtractVideoNameIfExists(no_v_param);
  EXPECT_FALSE(video_name.has_value());

  // Test case with an empty 'v' parameter.
  GURL empty_v_param("https://www.youtube.com/watch?v=");
  video_name = GlicMediaLinkHelper::ExtractVideoNameIfExists(empty_v_param);
  EXPECT_FALSE(video_name.has_value());

  // Test case with an invalid path.
  GURL invalid_path("https://www.youtube.com/invalid/dQw4w9WgXcQ");
  video_name = GlicMediaLinkHelper::ExtractVideoNameIfExists(invalid_path);
  EXPECT_FALSE(video_name.has_value());
}

TEST_F(GlicMediaLinkHelperStaticTest, ExtractTimeFromQueryIfExists) {
  // Test case with a valid time parameter.
  GURL url_with_time("https://www.youtube.com/watch?v=123&t=42");
  std::optional<base::TimeDelta> time =
      GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_with_time);
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(*time, base::Seconds(42));

  // Test case with "t" as the first parameter.
  GURL url_with_time_first("https://www.youtube.com/watch?t=123&v=456");
  time = GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_with_time_first);
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(*time, base::Seconds(123));

  // Test case with no "t" parameter.
  GURL url_no_time("https://www.youtube.com/watch?v=123");
  time = GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_no_time);
  EXPECT_FALSE(time.has_value());

  // Test case with an empty "t" parameter.
  GURL url_empty_time("https://www.youtube.com/watch?v=123&t=");
  time = GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_empty_time);
  EXPECT_FALSE(time.has_value());

  // Test case with a non-numeric "t" parameter.
  GURL url_non_numeric_time("https://www.youtube.com/watch?v=123&t=abc");
  time =
      GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_non_numeric_time);
  EXPECT_FALSE(time.has_value());

  // Test case with a negative "t" parameter.
  GURL url_negative_time("https://www.youtube.com/watch?v=123&t=-30");
  time = GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_negative_time);
  EXPECT_FALSE(time.has_value());

  // Test case with a fractional "t" parameter.
  GURL url_fractional_time("https://www.youtube.com/watch?v=123&t=30.5");
  time = GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(url_fractional_time);
  EXPECT_FALSE(time.has_value());
}

}  // namespace
}  // namespace glic
