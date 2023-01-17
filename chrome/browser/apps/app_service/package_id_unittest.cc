// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/package_id.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using PackageIdTest = testing::Test;

TEST_F(PackageIdTest, FromStringValidWeb) {
  absl::optional<PackageId> id =
      PackageId::FromString("web:https://www.app.com/");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->app_type(), AppType::kWeb);
  ASSERT_EQ(id->identifier(), "https://www.app.com/");
}

TEST_F(PackageIdTest, FromStringValidAndroid) {
  absl::optional<PackageId> id =
      PackageId::FromString("android:com.google.android.apps.photos");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->app_type(), AppType::kArc);
  ASSERT_EQ(id->identifier(), "com.google.android.apps.photos");
}

TEST_F(PackageIdTest, FromStringInvalidFormat) {
  ASSERT_FALSE(PackageId::FromString("foobar").has_value());
  ASSERT_FALSE(PackageId::FromString("web:").has_value());
  ASSERT_FALSE(PackageId::FromString("").has_value());
  ASSERT_FALSE(PackageId::FromString(":").has_value());
}

TEST_F(PackageIdTest, FromStringInvalidType) {
  absl::optional<PackageId> id = PackageId::FromString("coolplatform:myapp");

  ASSERT_FALSE(id.has_value());
}

TEST_F(PackageIdTest, ToStringWeb) {
  PackageId id(AppType::kWeb, "https://www.app.com/");

  ASSERT_EQ(id.ToString(), "web:https://www.app.com/");
}

TEST_F(PackageIdTest, ToStringAndroid) {
  PackageId id(AppType::kArc, "com.google.android.apps.photos");

  ASSERT_EQ(id.ToString(), "android:com.google.android.apps.photos");
}

}  // namespace apps
