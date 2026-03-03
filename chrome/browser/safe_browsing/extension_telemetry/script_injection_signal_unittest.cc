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
      /*arg_url=*/"http://evil.com/script.js",
      /*timestamp=*/now);

  EXPECT_EQ(signal.api_name(), "blinkSetAttribute");
  EXPECT_EQ(signal.url(), kSanitizedUrl);
  EXPECT_EQ(signal.args_list().size(), 4u);
  EXPECT_EQ(signal.args_list()[2], "old.js");
  EXPECT_EQ(signal.arg_url(), "http://evil.com/script.js");
  EXPECT_EQ(signal.timestamp(), now);
}

TEST(ScriptInjectionSignalTest, TruncatesLargeArguments) {
  std::string large_arg(2000, 'a');
  ScriptInjectionSignal signal = ScriptInjectionSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"scripting.executeScript",
      /*url=*/kUrl,
      /*args_list=*/{large_arg},
      /*arg_url=*/"",
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
      /*arg_url=*/"http://evil.com/js",
      /*timestamp=*/base::Time::Now());

  // Key format: api_name + "|" + url + "|" + join(args, "|") + "|" + arg_url
  std::string expected_key = "blinkSetAttribute|" + std::string(kSanitizedUrl) +
                             "|script|src|<arg_url>|http://evil.com/js";
  EXPECT_EQ(signal.GetAggregationKey(), expected_key);
}

}  // namespace

}  // namespace safe_browsing
