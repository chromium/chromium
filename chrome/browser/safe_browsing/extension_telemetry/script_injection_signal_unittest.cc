// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr const char kUrl[] = "http://www.example.com/path1/index.html?q=test";
constexpr const char kSanitizedUrl[] = "http://www.example.com/path1/";

TEST(ScriptInjectionSignalTest, RemoteScriptInjection) {
  base::Time now = base::Time::Now();
  ScriptInjectionSignal signal = ScriptInjectionSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"blinkSetAttribute",
      /*url=*/kUrl,
      /*args_list=*/{"script", "src", "old.js", "<arg_url>"},
      /*timestamp=*/now);

  EXPECT_EQ(signal.api_name(), "blinkSetAttribute");
  EXPECT_EQ(signal.url(), kSanitizedUrl);
  EXPECT_EQ(signal.args_list().size(), 4u);
  EXPECT_EQ(signal.args_list()[2], "old.js");
  EXPECT_EQ(signal.timestamp(), now);
}

TEST(ScriptInjectionSignalTest, SanitizesUrls) {
  base::Time now = base::Time::Now();
  // Create a very long URL to test truncation + sanitization.
  // The query parameter starts before 1024 and continues long after.
  std::string long_url =
      "https://evil.com/malware.js?q=" + std::string(2000, 'a');

  ScriptInjectionSignal signal = ScriptInjectionSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"blinkSetAttribute",
      /*url=*/"https://example.com/path/to/page.html?user=pii",
      /*args_list=*/
      {"script", "src", long_url, "javascript:alert(1)"},
      /*timestamp=*/now);

  // url should be sanitized to remove filename/query/ref.
  EXPECT_EQ(signal.url(), "https://example.com/path/to/");

  // args_list URLs should be sanitized to remove only query/ref (keeping
  // filename). Even though long_url was truncated first, GURL should still be
  // able to identify and strip the truncated query string.
  ASSERT_EQ(signal.args_list().size(), 4u);
  EXPECT_EQ(signal.args_list()[0], "script");
  EXPECT_EQ(signal.args_list()[1], "src");
  EXPECT_EQ(signal.args_list()[2], "https://evil.com/malware.js");
  EXPECT_EQ(signal.args_list()[3], "javascript:alert(1)");
}

TEST(ScriptInjectionSignalTest, TruncatesLargeArguments) {
  std::string large_arg(2000, 'a');
  ScriptInjectionSignal signal = ScriptInjectionSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"scripting.executeScript",
      /*url=*/kUrl,
      /*args_list=*/{large_arg},
      /*timestamp=*/base::Time::Now());

  ASSERT_EQ(signal.args_list().size(), 1u);
  EXPECT_EQ(signal.args_list()[0].length(), 1024u + 7u);  // 1024 + "[TRUNC]"
  EXPECT_EQ(signal.args_list()[0].substr(1024), "[TRUNC]");
}

TEST(ScriptInjectionSignalTest, GeneratesAggregationKey) {
  ScriptInjectionSignal signal = ScriptInjectionSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"blinkSetAttribute",
      /*url=*/kUrl,
      /*args_list=*/{"script", "src", "<arg_url>"},
      /*timestamp=*/base::Time::Now());

  // Key format: api_name + "|" + url + "|" + join(args, "|")
  std::string expected_key = "blinkSetAttribute|" + std::string(kSanitizedUrl) +
                             "|script|src|<arg_url>";
  EXPECT_EQ(signal.GetAggregationKey(), expected_key);
}

}  // namespace

}  // namespace safe_browsing
