// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
namespace apps {
namespace {

using AppInstallTypesTest = testing::Test;

TEST_F(AppInstallTypesTest, IsNotValidForInstallation_AndroidAppNoInstallUrl) {
  AppInstallData data(PackageId(PackageType::kArc, "com.android.app"));

  ASSERT_FALSE(data.IsValidForInstallation());
}

TEST_F(AppInstallTypesTest, IsValidForInstallation_AndroidAppWithInstallUrl) {
  AppInstallData data(PackageId(PackageType::kArc, "com.android.app"));
  data.install_url = GURL("https://play.google.com/apps");

  ASSERT_TRUE(data.IsValidForInstallation());
}

TEST_F(AppInstallTypesTest, IsNotValidForInstallation_WebAppNoInstallData) {
  AppInstallData data(PackageId(PackageType::kWeb, "https://www.app.com/"));

  ASSERT_FALSE(data.IsValidForInstallation());
}

TEST_F(AppInstallTypesTest, IsValidForInstallation_WebAppWithInstallData) {
  AppInstallData data(PackageId(PackageType::kWeb, "https://www.app.com/"));
  WebAppInstallData web_app_data;
  web_app_data.original_manifest_url =
      GURL("https://www.app.com/manifest.json");
  web_app_data.proxied_manifest_url =
      GURL("https://googleusercontent.com/manifest.json");
  web_app_data.document_url = GURL("https://www.app.com/");
  data.app_type_data.emplace<WebAppInstallData>(web_app_data);

  ASSERT_TRUE(data.IsValidForInstallation());
}

TEST_F(AppInstallTypesTest, IsValidForInstallation_WebsiteWithInstallData) {
  AppInstallData data(PackageId(PackageType::kWebsite, "https://www.app.com/"));
  WebAppInstallData web_app_data;
  web_app_data.original_manifest_url =
      GURL("https://www.app.com/manifest.json");
  web_app_data.proxied_manifest_url =
      GURL("https://googleusercontent.com/manifest.json");
  web_app_data.document_url = GURL("https://www.app.com/");
  data.app_type_data.emplace<WebAppInstallData>(web_app_data);

  ASSERT_TRUE(data.IsValidForInstallation());
}

}  // namespace
}  // namespace apps
