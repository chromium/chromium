// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/session_sync_service_factory.h"

#include "chrome/common/webui_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kInvalidUrl[] = "invalid.url";

TEST(SessionSyncServiceFactoryTest, ShouldSyncURL) {
  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL(kValidUrl)));
  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("other://anything")));
  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("chrome-other://anything")));

  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL(kInvalidUrl)));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("file://anything")));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL(chrome::kChromeUIVersionURL)));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("chrome-native://anything")));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("chrome-distiller://anything")));

  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
      GURL("chrome-untrusted://anything")));
}

}  // namespace
