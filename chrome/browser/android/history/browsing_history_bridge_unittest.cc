// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/browsing_history_bridge.h"

#include <string>

#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class BrowsingHistoryBridgeTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowsingHistoryBridgeTest,
       GetPageTitleForUrl_ReturnsStoredHistoryTitleAndEmptyForUnvisitedUrl) {
  // Setup.
  auto* history_service = static_cast<history::HistoryService*>(
      HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_, HistoryServiceFactory::GetDefaultFactory()));
  ASSERT_TRUE(history_service);

  const GURL kVisitedUrl("https://espn.com");
  const GURL kUnvisitedUrl("https://unvisited.example.com");
  history_service->AddPage(kVisitedUrl, base::Time::Now(),
                           history::SOURCE_BROWSED);
  history_service->SetPageTitle(kVisitedUrl, u"ESPN - Serving Sports Fans");

  // Act.
  base::test::TestFuture<std::string> stored_future;
  BrowsingHistoryBridge::GetPageTitleForUrl(
      &profile_, kVisitedUrl, stored_future.GetCallback<const std::string&>());

  base::test::TestFuture<std::string> unvisited_future;
  BrowsingHistoryBridge::GetPageTitleForUrl(
      &profile_, kUnvisitedUrl,
      unvisited_future.GetCallback<const std::string&>());

  // Verify.
  EXPECT_EQ("ESPN - Serving Sports Fans", stored_future.Get());
  EXPECT_EQ("", unvisited_future.Get());
}

TEST_F(BrowsingHistoryBridgeTest,
       GetPageTitleForUrl_ReturnsEmptyForNullProfileOrInvalidUrl) {
  // Act & Verify: null profile returns empty string immediately.
  base::test::TestFuture<std::string> null_profile_future;
  BrowsingHistoryBridge::GetPageTitleForUrl(
      /*profile=*/nullptr, GURL("https://example.com"),
      null_profile_future.GetCallback<const std::string&>());
  EXPECT_EQ("", null_profile_future.Get());

  // Act & Verify: invalid GURL returns empty string immediately.
  base::test::TestFuture<std::string> invalid_url_future;
  BrowsingHistoryBridge::GetPageTitleForUrl(
      &profile_, GURL("not a valid url"),
      invalid_url_future.GetCallback<const std::string&>());
  EXPECT_EQ("", invalid_url_future.Get());
}

}  // namespace
