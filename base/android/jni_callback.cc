// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_callback.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_minimal_jni/JniCallbackImpl_jni.h"

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
    bool is_repeating = false;
    return Java_JniCallbackImpl_Constructor(
        env, is_repeating,
        reinterpret_cast<jlong>(wrapped_callback_.release()));
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
    bool is_repeating = true;
    return Java_JniCallbackImpl_Constructor(
        env, is_repeating,
        reinterpret_cast<jlong>(wrapped_callback_.release()));
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

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceCallback<void()>&& callback) {
  return ToJniCallback(env, base::BindOnce(
                                [](base::OnceCallback<void()> captured_callback,
                                   const jni_zero::JavaRef<jobject>& j_null) {
                                  // For callbacks with no parameters, the
                                  // parameter from Java should be null.
                                  CHECK(!j_null);
                                  std::move(captured_callback).Run();
                                },
                                std::move(callback)));
}

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingCallback<void()>& callback) {
  return ToJniCallback(
      env, base::BindOnce(
               [](const base::RepeatingCallback<void()>& captured_callback,
                  const jni_zero::JavaRef<jobject>& j_null) {
                 // For callbacks with no parameters, the parameter from Java
                 // should be null.
                 CHECK(!j_null);
                 captured_callback.Run();
               },
               std::move(callback)));
}

void JNI_JniCallbackImpl_OnResult(
    JNIEnv* env,
    jboolean isRepeating,
    jlong callbackPtr,
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

void JNI_JniCallbackImpl_Destroy(JNIEnv* env,
                                 jboolean isRepeating,
                                 jlong callbackPtr) {
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

DEFINE_JNI_FOR_JniCallbackImpl()
