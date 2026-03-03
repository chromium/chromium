// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr const char kUrl[] = "http://www.example.com/path1/index.html?q=test";
constexpr const char kSanitizedUrl[] = "http://www.example.com/path1/";

TEST(DOMAccessSignalTest, ReadOperation) {
  base::Time now = base::Time::Now();
  DOMAccessSignal signal = DOMAccessSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"Document.cookie",
      /*url=*/kUrl,
      /*access_type=*/DOMAccessSignal::DOMAccess::READ,
      /*timestamp=*/now);

  EXPECT_EQ(signal.api_name(), "Document.cookie");
  EXPECT_EQ(signal.url(), kSanitizedUrl);
  EXPECT_EQ(signal.access_type(), DOMAccessSignal::DOMAccess::READ);
  EXPECT_EQ(signal.timestamp(), now);
}

TEST(DOMAccessSignalTest, GeneratesAggregationKey) {
  DOMAccessSignal signal = DOMAccessSignal(
      /*extension_id=*/"ext-0",
      /*api_name=*/"Document.cookie",
      /*url=*/kUrl,
      /*access_type=*/DOMAccessSignal::DOMAccess::READ,
      /*timestamp=*/base::Time::Now());

  // Key format: api_name + "|" + url + "|" + type
  std::string expected_key =
      "Document.cookie|" + std::string(kSanitizedUrl) + "|1";
  EXPECT_EQ(signal.GetAggregationKey(), expected_key);
}

}  // namespace

}  // namespace safe_browsing
