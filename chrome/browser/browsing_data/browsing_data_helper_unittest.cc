// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_helper.h"

#include "base/macros.h"
#include "chrome/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "devtools://abcdefghijklmnopqrstuvw/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

class BrowsingDataHelperTest : public testing::Test {
 public:
  BrowsingDataHelperTest() {}
  ~BrowsingDataHelperTest() override {}

  bool IsWebScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasWebScheme(test) &&
            BrowsingDataHelper::IsWebScheme(scheme));
  }

  bool IsExtensionScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasExtensionScheme(test) &&
            BrowsingDataHelper::IsExtensionScheme(scheme));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHelperTest);
};

TEST_F(BrowsingDataHelperTest, WebStorageSchemesAreWebSchemes) {
  EXPECT_TRUE(IsWebScheme(url::kHttpScheme));
  EXPECT_TRUE(IsWebScheme(url::kHttpsScheme));
  EXPECT_TRUE(IsWebScheme(url::kFileScheme));
  EXPECT_TRUE(IsWebScheme(url::kFtpScheme));
  EXPECT_TRUE(IsWebScheme(url::kWsScheme));
  EXPECT_TRUE(IsWebScheme(url::kWssScheme));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotWebSchemes) {
  EXPECT_FALSE(IsWebScheme(extensions::kExtensionScheme));
  EXPECT_FALSE(IsWebScheme(url::kAboutScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsWebScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsWebScheme(url::kMailToScheme));
  EXPECT_FALSE(IsWebScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, WebStorageSchemesAreNotExtensions) {
  EXPECT_FALSE(IsExtensionScheme(url::kHttpScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kHttpsScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kFileScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kFtpScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kWsScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kWssScheme));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotAllExtension) {
  EXPECT_TRUE(IsExtensionScheme(extensions::kExtensionScheme));

  EXPECT_FALSE(IsExtensionScheme(url::kAboutScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kMailToScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, SchemesThatCantStoreDataDontMatchAnything) {
  EXPECT_FALSE(IsWebScheme(url::kDataScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kDataScheme));

  EXPECT_FALSE(IsWebScheme("feed"));
  EXPECT_FALSE(IsExtensionScheme("feed"));

  EXPECT_FALSE(IsWebScheme(url::kBlobScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kBlobScheme));

  EXPECT_FALSE(IsWebScheme(url::kFileSystemScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kFileSystemScheme));

  EXPECT_FALSE(IsWebScheme("invalid-scheme-i-just-made-up"));
  EXPECT_FALSE(IsExtensionScheme("invalid-scheme-i-just-made-up"));
}

}  // namespace
