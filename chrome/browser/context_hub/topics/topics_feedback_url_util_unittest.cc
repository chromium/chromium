// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topics_feedback_url_util.h"

#include <optional>
#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

using ::testing::Optional;

std::optional<GURL> Minimize(std::string_view url) {
  return MinimizeUrlForTopicsFeedback(GURL(url));
}

TEST(TopicsFeedbackUrlUtilTest, KeepsPlainWebUrls) {
  EXPECT_THAT(Minimize("https://example.com/path"),
              Optional(GURL("https://example.com/path")));
  EXPECT_THAT(Minimize("http://example.com:8080/a/b"),
              Optional(GURL("http://example.com:8080/a/b")));
}

TEST(TopicsFeedbackUrlUtilTest, DropsNonWebSchemes) {
  EXPECT_EQ(Minimize("chrome://settings"), std::nullopt);
  EXPECT_EQ(Minimize("chrome-extension://abcdefghijklmnop/page.html"),
            std::nullopt);
  EXPECT_EQ(Minimize("file:///home/user/secret.txt"), std::nullopt);
  EXPECT_EQ(Minimize("data:text/plain,hello"), std::nullopt);
  EXPECT_EQ(Minimize("about:blank"), std::nullopt);
  EXPECT_EQ(Minimize("ftp://example.com/file"), std::nullopt);
}

TEST(TopicsFeedbackUrlUtilTest, DropsInvalidUrls) {
  EXPECT_EQ(MinimizeUrlForTopicsFeedback(GURL()), std::nullopt);
  EXPECT_EQ(Minimize("not a url"), std::nullopt);
}

TEST(TopicsFeedbackUrlUtilTest, StripsCredentialsAndFragment) {
  EXPECT_THAT(Minimize("https://user:pass@example.com/path#section"),
              Optional(GURL("https://example.com/path")));
  EXPECT_THAT(Minimize("https://user@example.com/"),
              Optional(GURL("https://example.com/")));
}

TEST(TopicsFeedbackUrlUtilTest, RedactsSecretQueryValues) {
  EXPECT_THAT(Minimize("https://example.com/cb?code=abc123&state=xyz"),
              Optional(GURL("https://example.com/cb?code=REDACTED&state=xyz")));
  EXPECT_THAT(
      Minimize("https://example.com/?access_token=t&id_token=i&"
               "refresh_token=r&api_key=k&apikey=k2&sig=s&signature=s2"),
      Optional(GURL("https://example.com/?access_token=REDACTED&"
                    "id_token=REDACTED&refresh_token=REDACTED&"
                    "api_key=REDACTED&apikey=REDACTED&sig=REDACTED&"
                    "signature=REDACTED")));
  EXPECT_THAT(
      Minimize("https://example.com/?session=1&session_id=2&sessionid=3&"
               "auth=4&key=5&password=6&passwd=7&secret=8&token=9"),
      Optional(GURL("https://example.com/?session=REDACTED&"
                    "session_id=REDACTED&sessionid=REDACTED&auth=REDACTED&"
                    "key=REDACTED&password=REDACTED&passwd=REDACTED&"
                    "secret=REDACTED&token=REDACTED")));
}

TEST(TopicsFeedbackUrlUtilTest, MatchesSecretNamesCaseInsensitively) {
  EXPECT_THAT(Minimize("https://example.com/?Token=a&API_KEY=b"),
              Optional(GURL("https://example.com/?Token=REDACTED&"
                            "API_KEY=REDACTED")));
}

TEST(TopicsFeedbackUrlUtilTest, MatchesPercentEncodedSecretNames) {
  // "%74oken" decodes to "token".
  EXPECT_THAT(Minimize("https://example.com/?%74oken=a"),
              Optional(GURL("https://example.com/?%74oken=REDACTED")));
}

TEST(TopicsFeedbackUrlUtilTest, PreservesNonSecretQueryParams) {
  // Search queries and video ids are useful signal and must be kept verbatim.
  EXPECT_THAT(Minimize("https://www.google.com/search?q=hiking+boots&hl=en"),
              Optional(GURL("https://www.google.com/search?q=hiking+boots&"
                            "hl=en")));
  EXPECT_THAT(Minimize("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42"),
              Optional(GURL("https://www.youtube.com/watch?v=dQw4w9WgXcQ&"
                            "t=42")));
  // Only exact names match, not names that merely contain a secret word.
  EXPECT_THAT(Minimize("https://example.com/?monkey=1&tokens=2&keyword=3"),
              Optional(GURL("https://example.com/?monkey=1&tokens=2&"
                            "keyword=3")));
}

TEST(TopicsFeedbackUrlUtilTest, TreatsSemicolonAsSeparator) {
  // A secret after a ';' is redacted, and parameters after a redacted value
  // are kept. Each parameter keeps the separator that followed it.
  EXPECT_THAT(Minimize("https://example.com/?data=1;token=secret&q=2"),
              Optional(GURL("https://example.com/?data=1;token=REDACTED&q=2")));
  EXPECT_THAT(Minimize("https://example.com/?token=secret;other=1"),
              Optional(GURL("https://example.com/?token=REDACTED;other=1")));
}

TEST(TopicsFeedbackUrlUtilTest, MatchesSecretNamesWithEscapedSeparators) {
  // "%2F" and "%5C" are not separators, so these names are not secrets, but
  // every escape is decoded before matching.
  EXPECT_THAT(
      Minimize("https://example.com/?%2Ftoken=a&to%6Ben=b"),
      Optional(GURL("https://example.com/?%2Ftoken=a&to%6Ben=REDACTED")));
}

TEST(TopicsFeedbackUrlUtilTest, LeavesValuelessAndEmptyParamsAlone) {
  EXPECT_THAT(Minimize("https://example.com/?token&a=1&&b="),
              Optional(GURL("https://example.com/?token&a=1&&b=")));
}

TEST(TopicsFeedbackUrlUtilTest, RedactsAndStripsTogether) {
  EXPECT_THAT(Minimize("https://u:p@example.com/p?q=cats&token=abc#frag"),
              Optional(GURL("https://example.com/p?q=cats&token=REDACTED")));
}

TEST(TopicsFeedbackUrlUtilTest, ExcludesCorpHostsAndSubdomains) {
  EXPECT_TRUE(IsDefaultExcludedDomain("corp.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("buganizer.corp.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("a.b.corp.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("googleplex.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("sites.googleplex.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("goto.google.com"));
}

TEST(TopicsFeedbackUrlUtilTest, IgnoresTrailingDot) {
  EXPECT_TRUE(IsDefaultExcludedDomain("docs.google.com."));
  EXPECT_TRUE(IsDefaultExcludedDomain("buganizer.corp.google.com."));
  EXPECT_TRUE(IsDefaultExcludedDomain(
      GURL("https://docs.google.com./document").host()));
  EXPECT_FALSE(IsDefaultExcludedDomain("www.google.com."));
}

TEST(TopicsFeedbackUrlUtilTest, ExcludesWorkspaceHosts) {
  EXPECT_TRUE(IsDefaultExcludedDomain("docs.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("drive.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("mail.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("chat.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("meet.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("calendar.google.com"));
  EXPECT_TRUE(IsDefaultExcludedDomain("colab.research.google.com"));
}

TEST(TopicsFeedbackUrlUtilTest, DoesNotExcludeOtherHosts) {
  EXPECT_FALSE(IsDefaultExcludedDomain("www.google.com"));
  EXPECT_FALSE(IsDefaultExcludedDomain("google.com"));
  EXPECT_FALSE(IsDefaultExcludedDomain("research.google.com"));
  EXPECT_FALSE(IsDefaultExcludedDomain("example.com"));
  // Suffix matches require a label boundary.
  EXPECT_FALSE(IsDefaultExcludedDomain("notcorp.google.com"));
  EXPECT_FALSE(IsDefaultExcludedDomain("evilgoogleplex.com"));
  // Exact-match hosts do not extend to subdomains.
  EXPECT_FALSE(IsDefaultExcludedDomain("foo.docs.google.com"));
  // Lookalikes on other registrable domains are not excluded.
  EXPECT_FALSE(IsDefaultExcludedDomain("corp.google.com.evil.com"));
  EXPECT_FALSE(IsDefaultExcludedDomain(""));
}

}  // namespace

}  // namespace context_hub
