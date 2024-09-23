// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr const char kUrl1[] =
    "http://www.example.com/path1/index.html?q=test&id=5";
constexpr const char kSanitizedUrl1[] = "http://www.example.com/path1/";
constexpr const char kUrl2[] =
    "http://www.example.com/path1/path2/index.html?q=test&id=5";
constexpr const char kSanitizedUrl2[] = "http://www.example.com/path1/path2/";

TEST(DeclarativeNetRequestActionSignalTest, SanitizesUrls) {
  std::unique_ptr<DeclarativeNetRequestActionSignal> signal =
      DeclarativeNetRequestActionSignal::
          CreateDeclarativeNetRequestRedirectActionSignal(
              /*extension_id=*/"ext-0",
              /*request_url=*/GURL(kUrl1),
              /*redirect_url=*/GURL(kUrl2));

  EXPECT_EQ(signal->action_details().request_url(), kSanitizedUrl1);
  EXPECT_EQ(signal->action_details().redirect_url(), kSanitizedUrl2);
}

TEST(DeclarativeNetRequestActionSignalTest, GeneratesIdWithAllArgsPresent) {
  std::unique_ptr<DeclarativeNetRequestActionSignal> signal =
      DeclarativeNetRequestActionSignal::
          CreateDeclarativeNetRequestRedirectActionSignal(
              /*extension_id=*/"ext-0",
              /*request_url=*/GURL(kUrl1),
              /*redirect_url=*/GURL(kUrl2));

  std::string expected_id =
      "1," + std::string(kSanitizedUrl1) + "," + std::string(kSanitizedUrl2);
  EXPECT_EQ(signal->GetUniqueActionDetailsId(), expected_id);
}

TEST(DeclarativeNetRequestActionSignalTest, GeneratesIdWithDefaultArgs) {
  std::unique_ptr<DeclarativeNetRequestActionSignal> signal =
      DeclarativeNetRequestActionSignal::
          CreateDeclarativeNetRequestRedirectActionSignal(
              /*extension_id=*/"ext-0",
              /*request_url=*/GURL(),
              /*redirect_url=*/GURL(kUrl2));

  std::string expected_id = "1,," + std::string(kSanitizedUrl2);
  EXPECT_EQ(signal->GetUniqueActionDetailsId(), expected_id);
}

}  // namespace

}  // namespace safe_browsing
