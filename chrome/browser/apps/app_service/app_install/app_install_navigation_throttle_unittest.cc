// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

class AppInstallNavigationThrottleTest : public testing::Test {
 public:
  AppInstallNavigationThrottleTest() = default;

  absl::optional<PackageId> ExtractPackageId(std::string_view query) {
    return AppInstallNavigationThrottle::ExtractPackageId(query);
  }
};

TEST_F(AppInstallNavigationThrottleTest, ExtractPackageId) {
  EXPECT_EQ(ExtractPackageId(""), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("garbage"), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("package_id"), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("package_id="), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("package_id=garbage"), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("package_id=web:identifier"),
            PackageId(AppType::kWeb, "identifier"));

  EXPECT_EQ(ExtractPackageId("package_id=android:identifier"),
            PackageId(AppType::kArc, "identifier"));

  EXPECT_EQ(ExtractPackageId("package_id=garbage:identifier"), absl::nullopt);

  EXPECT_EQ(ExtractPackageId("ignore&package_id=web:identifier"),
            PackageId(AppType::kWeb, "identifier"));

  EXPECT_EQ(ExtractPackageId("ignore&package_id=web:identifier&ignore=as_well"),
            PackageId(AppType::kWeb, "identifier"));

  EXPECT_EQ(ExtractPackageId("package_id=web:first&package_id=web:second"),
            PackageId(AppType::kWeb, "first"));

  EXPECT_EQ(ExtractPackageId("package_id=web:https://website.com/"),
            PackageId(AppType::kWeb, "https://website.com/"));

  EXPECT_EQ(
      ExtractPackageId(
          "package_id=web:https://website.com/?param1=value&param2=value"),
      PackageId(AppType::kWeb, "https://website.com/?param1=value"));

  EXPECT_EQ(ExtractPackageId("package_id=web%3Ahttps%3A%2F%2Fwebsite.com%2F%"
                             "3Fparam1%3Dvalue%26param2%3Dvalue"),
            PackageId(AppType::kWeb,
                      "https://website.com/?param1=value&param2=value"));
}

}  // namespace apps
