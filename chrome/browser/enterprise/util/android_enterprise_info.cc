// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/android_enterprise_info.h"

#include <jni.h>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/enterprise/util/jni_headers/EnterpriseInfo_jni.h"

namespace enterprise_util {
AndroidEnterpriseInfo::AndroidEnterpriseInfo() = default;
AndroidEnterpriseInfo::~AndroidEnterpriseInfo() = default;

void AndroidEnterpriseInfo::GetAndroidEnterpriseInfoState(
    EnterpriseInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_queue_.push(std::move(callback));

  // If there is already a callback in the queue then that means we're already
  // waiting on a response. No need to initiate another request.
  if (callback_queue_.size() > 1)
    return;

  if (skip_jni_call_for_testing_)
    return;

  Java_EnterpriseInfo_getManagedStateForNative(
      base::android::AttachCurrentThread());
}

void AndroidEnterpriseInfo::ServiceCallbacks(bool profile_owned,
                                             bool device_owned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Move the queue to a local so that we can handle re-entrancy.
  std::queue<EnterpriseInfoCallback> local_queue;
  local_queue.swap(callback_queue_);

  while (local_queue.size() > 0) {
    std::move(local_queue.front()).Run(profile_owned, device_owned);
    local_queue.pop();
  }
}

// JNI_EnterpriseInfo_UpdateNativeOwnedState() is static function, this makes
// friending it a hassle because it must be declared in the file that the friend
// declaration is in, but its declaration can't be included in multiple places
// or things get messy and the linker gets mad. This helper class exists only to
// friend the JNI function and is, in turn, friended by AndroidEnterpriseInfo
// which allows for the private ServiceCallbacks() to be reached.
class AndroidEnterpriseInfoFriendHelper {
 private:
  friend void ::JNI_EnterpriseInfo_UpdateNativeOwnedState(
      JNIEnv* env,
      jboolean hasProfileOwnerApp,
      jboolean hasDeviceOwnerApp);

  static void ForwardToServiceCallbacks(bool profile_owned, bool device_owned) {
    AndroidEnterpriseInfo::GetInstance()->ServiceCallbacks(profile_owned,
                                                           device_owned);
  }
};

}  // namespace enterprise_util

void JNI_EnterpriseInfo_UpdateNativeOwnedState(JNIEnv* env,
                                               jboolean hasProfileOwnerApp,
                                               jboolean hasDeviceOwnerApp) {
  enterprise_util::AndroidEnterpriseInfoFriendHelper::ForwardToServiceCallbacks(
      static_cast<bool>(hasProfileOwnerApp),
      static_cast<bool>(hasDeviceOwnerApp));
}
