// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_input_receiver_compat.h"

#include "base/android/build_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(AndroidInputReceiverCompatTest, CanFindMethodsOnAndroidVPlus) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_V) {
    EXPECT_EQ(AndroidInputReceiverCompat::IsSupportAvailable(), false);
    return;
  }

  EXPECT_EQ(AndroidInputReceiverCompat::IsSupportAvailable(), true);
  const AndroidInputReceiverCompat& instance =
      AndroidInputReceiverCompat::GetInstance();

  EXPECT_NE(instance.AInputTransferToken_fromJavaFn, nullptr);
  EXPECT_NE(instance.AInputTransferToken_toJavaFn, nullptr);
  EXPECT_NE(instance.AInputTransferToken_releaseFn, nullptr);
  EXPECT_NE(instance.AInputEvent_toJavaFn, nullptr);
  EXPECT_NE(instance.AInputReceiverCallbacks_createFn, nullptr);
  EXPECT_NE(instance.AInputReceiverCallbacks_releaseFn, nullptr);
  EXPECT_NE(instance.AInputReceiverCallbacks_setMotionEventCallbackFn, nullptr);
  EXPECT_NE(instance.AInputReceiver_createUnbatchedInputReceiverFn, nullptr);
  EXPECT_NE(instance.AInputReceiver_getInputTransferTokenFn, nullptr);
  EXPECT_NE(instance.AInputReceiver_releaseFn, nullptr);
}

}  // namespace base
