// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"

#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {

using PromiseAppWrapperTest = testing::Test;

TEST_F(PromiseAppWrapperTest, ConversionSuccessful) {
  PackageId package_id(PackageType::kArc, "test.package.name");
  GURL url("http://www.image.com");

  proto::PromiseAppResponse response;
  response.set_package_id(package_id.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url(url.spec());
  response.mutable_icons(0)->set_width_in_pixels(512);
  response.mutable_icons(0)->set_mime_type("image/png");
  response.mutable_icons(0)->set_is_masking_allowed(true);

  PromiseAppWrapper promise_app_wrapper(response);
  ASSERT_EQ(promise_app_wrapper.GetPackageId(), package_id);
  ASSERT_EQ(promise_app_wrapper.GetName(), "Name");

  std::vector<IconWrapper> icons = promise_app_wrapper.GetIcons();
  ASSERT_EQ(icons.size(), 1u);
  ASSERT_EQ(icons[0].GetUrl(), url);
  ASSERT_EQ(icons[0].GetWidthInPixels(), 512);
  ASSERT_EQ(icons[0].GetMimeType(), "image/png");
  ASSERT_TRUE(icons[0].IsMaskingAllowed());
}

TEST_F(PromiseAppWrapperTest, EmptyFields) {
  proto::PromiseAppResponse response;
  PromiseAppWrapper promise_app_wrapper(response);

  ASSERT_FALSE(promise_app_wrapper.GetPackageId().has_value());
  ASSERT_FALSE(promise_app_wrapper.GetName().has_value());
  std::vector<IconWrapper> icons = promise_app_wrapper.GetIcons();
  ASSERT_EQ(icons.size(), 0u);
}

TEST_F(PromiseAppWrapperTest, InvalidPackageIdReturnsNull) {
  proto::PromiseAppResponse response;
  response.set_package_id("something:package.name");
  PromiseAppWrapper promise_app_wrapper(response);
  ASSERT_FALSE(promise_app_wrapper.GetPackageId().has_value());
}

TEST_F(PromiseAppWrapperTest, IconWrapperHasNoWidth) {
  GURL url("http://www.image.com");

  proto::PromiseAppResponse response;
  response.add_icons();

  // Set every field except the optional width_in_pixels field.
  response.mutable_icons(0)->set_url(url.spec());
  response.mutable_icons(0)->set_mime_type("image/png");
  response.mutable_icons(0)->set_is_masking_allowed(true);

  std::vector<IconWrapper> icons = PromiseAppWrapper(response).GetIcons();

  ASSERT_EQ(icons.size(), 1u);
  ASSERT_EQ(icons[0].GetUrl(), url);
  ASSERT_FALSE(icons[0].GetWidthInPixels().has_value());
  ASSERT_EQ(icons[0].GetMimeType(), "image/png");
  ASSERT_TRUE(icons[0].IsMaskingAllowed());
}

}  // namespace apps
