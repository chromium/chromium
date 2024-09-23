// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include <optional>

#include "base/strings/to_string.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using QueryParams = AppInstallNavigationThrottle::QueryParams;

void PrintTo(const QueryParams& query_params, std::ostream* out) {
  *out << "QueryParams("
       << base::ToString(query_params.serialized_package_id.value_or("nullopt"))
       << ", " << query_params.source << ")";
}

class AppInstallNavigationThrottleTest : public testing::Test {
 public:
  AppInstallNavigationThrottleTest() = default;

  AppInstallNavigationThrottle::QueryParams ExtractQueryParams(
      std::string_view query) {
    return AppInstallNavigationThrottle::ExtractQueryParams(query);
  }
};

TEST_F(AppInstallNavigationThrottleTest, ExtractQueryParams) {
  EXPECT_EQ(ExtractQueryParams(""), QueryParams());

  EXPECT_EQ(ExtractQueryParams("garbage"), QueryParams());

  EXPECT_EQ(ExtractQueryParams("package_id"), QueryParams());

  EXPECT_EQ(ExtractQueryParams("package_id="), QueryParams());

  EXPECT_EQ(ExtractQueryParams("package_id=garbage"),
            QueryParams("garbage", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams("package_id=android:identifier"),
            QueryParams("android:identifier",
                        AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams("package_id=garbage:identifier"),
            QueryParams("garbage:identifier",
                        AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams("ignore&package_id=web:identifier"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams("ignore&package_id=web:identifier&ignore=as_well"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:first&package_id=web:second"),
      QueryParams("web:second", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams("package_id=web:idenifier&package_id=garbage"),
            QueryParams("garbage", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams("package_id=web:https://website.com/"),
            QueryParams("web:https://website.com/",
                        AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams(
          "package_id=web:https://website.com/?source=showoff&param2=value"),
      QueryParams("web:https://website.com/?source=showoff",
                  AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams(
                "source=mall&package_id=web%3Ahttps%3A%2F%2Fwebsite.com%2F%"
                "3Fsource%3Dshowoff%26param2%3Dvalue"),
            QueryParams("web:https://website.com/?source=showoff&param2=value",
                        AppInstallSurface::kAppInstallUriMall));

  EXPECT_EQ(
      ExtractQueryParams("source=showoff"),
      QueryParams(std::nullopt, AppInstallSurface::kAppInstallUriShowoff));

  EXPECT_EQ(
      ExtractQueryParams("source=showoff&source=garbage"),
      QueryParams(std::nullopt, AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(ExtractQueryParams("package_id=invalid&source=showoff"),
            QueryParams("invalid", AppInstallSurface::kAppInstallUriShowoff));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=garbage"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriUnknown));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=showoff"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriShowoff));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=mall"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriMall));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=getit"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriGetit));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=launcher"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriLauncher));

  EXPECT_EQ(
      ExtractQueryParams("package_id=web:identifier&source=mallv2"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriMallV2));

  EXPECT_EQ(
      ExtractQueryParams("source=mall&package_id=web:identifier"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriMall));

  EXPECT_EQ(
      ExtractQueryParams("source=mall&package_id=web:identifier&source=getit"),
      QueryParams("web:identifier", AppInstallSurface::kAppInstallUriGetit));
}

}  // namespace apps
