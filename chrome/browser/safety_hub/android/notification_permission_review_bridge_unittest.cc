// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/notification_permission_review_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

namespace {

constexpr char kOriginPattern[] = "https://example.com";
const int kNotificationCount = 5;

}  // namespace

class NotificationPermissionReviewBridgeTest : public testing::Test {
 public:
  NotificationPermissionReviewBridgeTest() : env_(AttachCurrentThread()) {}

  raw_ptr<JNIEnv> env() { return env_; }

 private:
  raw_ptr<JNIEnv> env_;
};

TEST_F(NotificationPermissionReviewBridgeTest, TestJavaRoundTrip) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(kOriginPattern);
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  NotificationPermissions expected(primary_pattern, secondary_pattern,
                                   kNotificationCount);

  const auto jobject = ToJavaNotificationPermissions(env(), expected);
  NotificationPermissions converted =
      FromJavaNotificationPermissions(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.secondary_pattern, converted.secondary_pattern);
  EXPECT_EQ(expected.notification_count, converted.notification_count);
}
