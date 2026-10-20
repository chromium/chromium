// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_android.h"

#include <string>

#include "base/android/apk_info.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// The test APK registers the providers declared in
// base/test/android/java/AndroidManifest_file_provider.xml:
// "<package>.fileprovider" and "<package>" (android:exported="false"), and
// "<package>.docprov" (android:exported="true").
class ChromeContentBrowserClientAndroidContentUriTest : public testing::Test {
 protected:
  void SetUp() override {
    package_name_ = base::android::apk_info::package_name();
    ASSERT_NE(package_name_, "");
  }

  const std::string& package_name() const { return package_name_; }

 private:
  std::string package_name_;
};

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProvider) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content://", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithPackageNameAuthority) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(
      GURL(base::StrCat({"content://", package_name(), "/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithUserInfo) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content://0@", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithMultipleUserInfo) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(base::StrCat(
      {"content://0@1@", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithPercentEncodedUserInfo) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(base::StrCat(
      {"content://0%40", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithExportedProviderAsUserInfo) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(
      GURL(base::StrCat({"content://", package_name(), ".docprov@",
                         package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithPercentEncodedAuthority) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content://", package_name(), "%2Efileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksUnexportedProviderWithQueryAndRef) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(base::StrCat(
      {"content://", package_name(), ".fileprovider/secret?query=1#ref"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       AllowsUnexportedProviderWhenBlockingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kBlockNonExportedContentProviderNavigation);
  EXPECT_TRUE(IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content://", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       AllowsExportedProvider) {
  EXPECT_TRUE(IsContentSchemeURLNavigationAllowed(
      GURL(base::StrCat({"content://", package_name(), ".docprov/path"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       AllowsExternalMediaStoreContentUri) {
  EXPECT_TRUE(IsContentSchemeURLNavigationAllowed(
      GURL("content://media/external/downloads/1")));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       AllowsUnresolvedProvider) {
  EXPECT_TRUE(IsContentSchemeURLNavigationAllowed(
      GURL("content://com.example.otherapp.provider/path")));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksEmptyHostContentUri) {
  EXPECT_FALSE(
      IsContentSchemeURLNavigationAllowed(GURL("content:///local/path")));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksOpaqueContentUri) {
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content:", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       BlocksInvalidContentUri) {
  const GURL url("content://<");
  ASSERT_FALSE(url.is_valid());
  EXPECT_FALSE(IsContentSchemeURLNavigationAllowed(url));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       ChromeContentBrowserClientBlocksUnexportedProvider) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.IsContentSchemeURLNavigationAllowed(GURL(
      base::StrCat({"content://", package_name(), ".fileprovider/secret"}))));
}

TEST_F(ChromeContentBrowserClientAndroidContentUriTest,
       ChromeContentBrowserClientAllowsExternalUri) {
  ChromeContentBrowserClient client;
  EXPECT_TRUE(client.IsContentSchemeURLNavigationAllowed(
      GURL("content://media/external/downloads/1")));
}

}  // namespace
