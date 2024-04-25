// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_media_url_interceptor.h"

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;

namespace android_webview {

namespace {

// Sentinel value to check whether the fields have been set.
const int UNSET_VALUE = -1;

class AwMediaUrlInterceptorTest : public Test {
 public:
  AwMediaUrlInterceptorTest()
      : fd_(UNSET_VALUE),
        offset_(UNSET_VALUE),
        size_(UNSET_VALUE),
        url_interceptor_(new AwMediaUrlInterceptor()) {}

 protected:
  int fd_;
  int64_t offset_;
  int64_t size_;
  std::unique_ptr<AwMediaUrlInterceptor> url_interceptor_;
};

}  // namespace

TEST_F(AwMediaUrlInterceptorTest, TestInterceptValidAssetUrl) {
  // This asset file exists in the android_webview_unittests-debug.apk.
  // See gyp rule android_webview_unittests_apk.
  const std::string valid_asset_url("file:///android_asset/asset_file.ogg");

  ASSERT_TRUE(
      url_interceptor_->Intercept(valid_asset_url, &fd_, &offset_, &size_));
  EXPECT_NE(UNSET_VALUE, fd_);
  EXPECT_NE(UNSET_VALUE, offset_);
  EXPECT_NE(UNSET_VALUE, size_);
}

// TODO(crbug.com/41355155)
TEST_F(AwMediaUrlInterceptorTest, DISABLED_TestInterceptInvalidAssetUrl) {
  // This asset file does not exist in the android_webview_unittests-debug.apk.
  // See gyp rule android_webview_unittests_apk.
  const std::string invalid_asset_url(
      "file:///android_asset/file_does_not_exist.ogg");

  ASSERT_FALSE(
      url_interceptor_->Intercept(invalid_asset_url, &fd_, &offset_, &size_));
  EXPECT_EQ(UNSET_VALUE, fd_);
  EXPECT_EQ(UNSET_VALUE, offset_);
  EXPECT_EQ(UNSET_VALUE, size_);
}

TEST_F(AwMediaUrlInterceptorTest, TestInterceptNonAssetUrl) {
  // This url does not refer to an asset in the apk.
  const std::string non_asset_url("file:///sdcard/file.txt");

  ASSERT_FALSE(
      url_interceptor_->Intercept(non_asset_url, &fd_, &offset_, &size_));
  EXPECT_EQ(UNSET_VALUE, fd_);
  EXPECT_EQ(UNSET_VALUE, offset_);
  EXPECT_EQ(UNSET_VALUE, size_);
}

}  // namespace android_webview
