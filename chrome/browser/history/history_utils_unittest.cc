// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_utils.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif

TEST(HistoryUtilsTest, CanAddURLToHistory) {
  EXPECT_TRUE(CanAddURLToHistory(GURL("https://www.google.com")));
  EXPECT_TRUE(CanAddURLToHistory(GURL("http://example.com/path?query=1")));

  EXPECT_FALSE(CanAddURLToHistory(GURL()));
  EXPECT_FALSE(CanAddURLToHistory(GURL("invalid-url")));
  EXPECT_FALSE(CanAddURLToHistory(GURL("javascript:alert(1)")));
  EXPECT_FALSE(CanAddURLToHistory(GURL("about:blank")));
  EXPECT_FALSE(CanAddURLToHistory(GURL("chrome://version")));
  EXPECT_FALSE(CanAddURLToHistory(GURL("chrome-untrusted://terminal")));
  EXPECT_FALSE(CanAddURLToHistory(GURL("view-source:https://www.google.com")));

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(CanAddURLToHistory(
      GURL("isolated-app://aerugq4wfqsayeaaxxxxxxxxxx/index.html")));
  EXPECT_FALSE(CanAddURLToHistory(
      GURL("isolated-app://aerugq4wfqsayeaaxxxxxxxxxx/secret/admin")));
#endif
}
