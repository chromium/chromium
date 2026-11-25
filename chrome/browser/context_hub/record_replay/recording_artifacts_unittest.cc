// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

using ::browser::context_hub::mojom::SkillRecordingActionType;

TEST(RecordingArtifactsTest, StoresRecordingArtifacts) {
  RecordingArtifacts artifacts;
  EXPECT_TRUE(artifacts.actions.empty());

  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked on 'Submit'",
                                 base::Milliseconds(150));
  artifacts.actions[0].current_url = GURL("https://example.com/page");
  artifacts.actions[0].screenshot_jpeg_bytes =
      std::string("\x01\x02\x03\x04", 4);
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked on 'Next'", base::Milliseconds(350));

  ASSERT_EQ(artifacts.actions.size(), 2u);
  EXPECT_EQ(artifacts.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(artifacts.actions[0].description, "Clicked on 'Submit'");
  EXPECT_EQ(artifacts.actions[0].timestamp, base::Milliseconds(150));
  EXPECT_EQ(artifacts.actions[0].current_url, GURL("https://example.com/page"));
  EXPECT_EQ(artifacts.actions[0].screenshot_jpeg_bytes,
            std::string("\x01\x02\x03\x04", 4));

  EXPECT_EQ(artifacts.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(artifacts.actions[1].description, "Clicked on 'Next'");
  EXPECT_EQ(artifacts.actions[1].timestamp, base::Milliseconds(350));
  EXPECT_TRUE(artifacts.actions[1].current_url.is_empty());
  EXPECT_TRUE(artifacts.actions[1].screenshot_jpeg_bytes.empty());

  EXPECT_TRUE(artifacts.transcripts.empty());
  artifacts.transcripts.emplace_back("First click the button",
                                     base::Milliseconds(200));
  ASSERT_EQ(artifacts.transcripts.size(), 1u);
  EXPECT_EQ(artifacts.transcripts[0].text, "First click the button");
  EXPECT_EQ(artifacts.transcripts[0].timestamp, base::Milliseconds(200));
}

TEST(RecordingArtifactsTest, SanitizesRecordedUrl) {
  EXPECT_TRUE(SanitizeRecordedUrl(GURL()).is_empty());
  EXPECT_TRUE(SanitizeRecordedUrl(GURL("chrome://settings")).is_empty());
  EXPECT_TRUE(SanitizeRecordedUrl(GURL("file:///tmp/a.html")).is_empty());

  EXPECT_EQ(SanitizeRecordedUrl(
                GURL("https://user:pass@example.com/path?token=1#frag")),
            GURL("https://example.com/path"));

  // Paths pushing the spec over `kMaxRecordedUrlBytes` are cleared while
  // preserving origin.
  EXPECT_EQ(
      SanitizeRecordedUrl(GURL(base::StrCat(
          {"https://example.com/", std::string(kMaxRecordedUrlBytes, 'a')}))),
      GURL("https://example.com/"));

  // Origins exceeding `kMaxRecordedUrlBytes` are rejected completely.
  EXPECT_TRUE(
      SanitizeRecordedUrl(
          GURL(base::StrCat(
              {"https://", std::string(kMaxRecordedUrlBytes, 'a'), ".com/"})))
          .is_empty());
}

TEST(RecordingArtifactsTest,
     SanitizesRecordedTextAndStripsInvisibleAndBidiCharacters) {
  // Strips zero-width (`U+200B`, `U+FEFF`), bidi override (`U+202E`), Unicode
  // Tag (`U+E0041`), variation selectors (`U+FE0F`, `U+E0100`), and C0 control
  // (`\x07`) characters while trimming leading and trailing whitespace.
  EXPECT_EQ(SanitizeRecordedText(
                "\xEF\xBB\xBF  Submit\x07\xE2\x80\x8B\xE2\x80\xAE\xEF\xB8\x8F"
                "\xF3\xA0\x81\x81\xF3\xA0\x84\x80\nOrder  "),
            "Submit Order");

  // Collapses `\v`, `\f`, and `U+0085` (NEL) to spaces instead of gluing words.
  EXPECT_EQ(SanitizeRecordedText("First\vSecond\fThird\xC2\x85"
                                 "Fourth"),
            "First Second Third Fourth");

  // Strings consisting solely of zero-width characters sanitize to empty so
  // callers fall back to role/URL labels.
  EXPECT_EQ(SanitizeRecordedText("\xE2\x80\x8B\xEF\xBB\xBF"), "");

  // `max_bytes` edge cases (`0`, `<= 3`, exact boundary with trailing
  // whitespace, multi-byte UTF-8 boundaries, and large input streaming
  // truncation).
  EXPECT_EQ(SanitizeRecordedText("abcdef", /*max_bytes=*/0), "");
  EXPECT_EQ(SanitizeRecordedText("abcdef", /*max_bytes=*/3), "abc");
  EXPECT_EQ(SanitizeRecordedText("ab c", /*max_bytes=*/3), "ab");
  EXPECT_EQ(SanitizeRecordedText("a \xE2\x82\xAC", /*max_bytes=*/3), "a");
  EXPECT_EQ(SanitizeRecordedText("abcdef", /*max_bytes=*/5), "ab...");
  EXPECT_EQ(SanitizeRecordedText("ab cdef", /*max_bytes=*/6), "ab...");
  EXPECT_EQ(SanitizeRecordedText("abcde   \n  ", /*max_bytes=*/5), "abcde");
  EXPECT_EQ(SanitizeRecordedText("a\xE2\x82\xAC"
                                 "bcd",
                                 /*max_bytes=*/6),
            "a...");
  EXPECT_EQ(SanitizeRecordedText("a\xE2\x82\xAC"
                                 "bcde",
                                 /*max_bytes=*/7),
            "a\xE2\x82\xAC...");
  EXPECT_EQ(SanitizeRecordedText(std::string(10000, 'x')),
            base::StrCat({std::string(kMaxRecordedTextBytes - 3, 'x'), "..."}));

  // Invalid UTF-8 sequences produce valid UTF-8 output.
  EXPECT_EQ(SanitizeRecordedText("Bad\xFFSequence"), "Bad\xEF\xBF\xBDSequence");
}

}  // namespace
}  // namespace context_hub
