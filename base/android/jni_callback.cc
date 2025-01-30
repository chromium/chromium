// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_callback.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/JniCallbackUtils_jni.h"
#include "base/base_jni/JniOnceCallback_jni.h"
#include "base/base_jni/JniRepeatingCallback_jni.h"

namespace base::android {

namespace {
class JniOnceCallback {
 public:
  explicit JniOnceCallback(JniOnceWrappedCallbackType&& on_complete)
      : wrapped_callback_(std::make_unique<JniOnceWrappedCallbackType>(
            std::move(on_complete))) {}
  ~JniOnceCallback() = default;

  JniOnceCallback(const JniOnceCallback&) = delete;
  const JniOnceCallback& operator=(const JniOnceCallback&) = delete;

  jni_zero::ScopedJavaLocalRef<jobject> TransferToJava(JNIEnv* env) && {
    CHECK(wrapped_callback_);
    CHECK(!wrapped_callback_->is_null());
    return Java_JniOnceCallback_Constructor(
        env, reinterpret_cast<jlong>(wrapped_callback_.release()));
  }

 private:
  std::unique_ptr<JniOnceWrappedCallbackType> wrapped_callback_;
};

class JniRepeatingCallback {
 public:
  explicit JniRepeatingCallback(
      const JniRepeatingWrappedCallbackType& on_complete)
      : wrapped_callback_(
            std::make_unique<JniRepeatingWrappedCallbackType>(on_complete)) {}
  explicit JniRepeatingCallback(JniRepeatingWrappedCallbackType&& on_complete)
      : wrapped_callback_(std::make_unique<JniRepeatingWrappedCallbackType>(
            std::move(on_complete))) {}
  ~JniRepeatingCallback() = default;

  jni_zero::ScopedJavaLocalRef<jobject> TransferToJava(JNIEnv* env) && {
    CHECK(wrapped_callback_);
    CHECK(!wrapped_callback_->is_null());
    return Java_JniRepeatingCallback_Constructor(
        env, reinterpret_cast<jlong>(wrapped_callback_.release()));
  }
  JniRepeatingCallback(const JniRepeatingCallback&) = delete;
  const JniRepeatingCallback& operator=(const JniRepeatingCallback&) = delete;

 private:
  std::unique_ptr<JniRepeatingWrappedCallbackType> wrapped_callback_;
};
}  // namespace

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniOnceWrappedCallbackType&& callback) {
  return JniOnceCallback(std::move(callback)).TransferToJava(env);
}

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniRepeatingWrappedCallbackType&& callback) {
  return JniRepeatingCallback(std::move(callback)).TransferToJava(env);
}

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const JniRepeatingWrappedCallbackType& callback) {
  return JniRepeatingCallback(callback).TransferToJava(env);
}

void JNI_JniCallbackUtils_OnResult(
    JNIEnv* env,
    jlong callbackPtr,
    jboolean isRepeating,
    const jni_zero::JavaParamRef<jobject>& j_result) {
  if (isRepeating) {
    auto* callback =
        reinterpret_cast<JniRepeatingWrappedCallbackType*>(callbackPtr);
    callback->Run(j_result);
  } else {
    auto* callback = reinterpret_cast<JniOnceWrappedCallbackType*>(callbackPtr);
    std::move(*callback).Run(j_result);
    delete callback;
  }
}

void JNI_JniCallbackUtils_Destroy(JNIEnv* env,
                                  jlong callbackPtr,
                                  jboolean isRepeating) {
  if (isRepeating) {
    auto* callback =
        reinterpret_cast<JniRepeatingWrappedCallbackType*>(callbackPtr);
    // Call Reset to ensure all accidental use-after-frees fail loudly.
    callback->Reset();
    delete callback;
  } else {
    auto* callback = reinterpret_cast<JniOnceWrappedCallbackType*>(callbackPtr);
    // Call Reset to ensure all accidental use-after-frees fail loudly.
    callback->Reset();
    delete callback;
  }
}

}  // namespace base::android
