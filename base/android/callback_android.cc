// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_minimal_jni/JniCallbackImpl_jni.h"
#include "base/callback_jni/Callback_jni.h"
#include "base/callback_jni/Callback2_jni.h"

namespace base {
namespace android {

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
        reinterpret_cast<int64_t>(wrapped_callback_.release()));
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
        reinterpret_cast<int64_t>(wrapped_callback_.release()));
  }
  JniRepeatingCallback(const JniRepeatingCallback&) = delete;
  const JniRepeatingCallback& operator=(const JniRepeatingCallback&) = delete;

 private:
  std::unique_ptr<JniRepeatingWrappedCallbackType> wrapped_callback_;
};

class JniOnceCallback2 {
 public:
  explicit JniOnceCallback2(JniOnceWrappedCallback2Type&& on_complete)
      : wrapped_callback_(std::make_unique<JniOnceWrappedCallback2Type>(
            std::move(on_complete))) {}
  ~JniOnceCallback2() = default;

  JniOnceCallback2(const JniOnceCallback2&) = delete;
  const JniOnceCallback2& operator=(const JniOnceCallback2&) = delete;

  jni_zero::ScopedJavaLocalRef<jobject> TransferToJava(JNIEnv* env) && {
    CHECK(wrapped_callback_);
    CHECK(!wrapped_callback_->is_null());
    bool is_repeating = false;
    return Java_JniCallbackImpl_Constructor(
        env, is_repeating,
        reinterpret_cast<int64_t>(wrapped_callback_.release()));
  }

 private:
  std::unique_ptr<JniOnceWrappedCallback2Type> wrapped_callback_;
};

class JniRepeatingCallback2 {
 public:
  explicit JniRepeatingCallback2(
      const JniRepeatingWrappedCallback2Type& on_complete)
      : wrapped_callback_(
            std::make_unique<JniRepeatingWrappedCallback2Type>(on_complete)) {}
  explicit JniRepeatingCallback2(JniRepeatingWrappedCallback2Type&& on_complete)
      : wrapped_callback_(std::make_unique<JniRepeatingWrappedCallback2Type>(
            std::move(on_complete))) {}
  ~JniRepeatingCallback2() = default;

  jni_zero::ScopedJavaLocalRef<jobject> TransferToJava(JNIEnv* env) && {
    CHECK(wrapped_callback_);
    CHECK(!wrapped_callback_->is_null());
    bool is_repeating = true;
    return Java_JniCallbackImpl_Constructor(
        env, is_repeating,
        reinterpret_cast<int64_t>(wrapped_callback_.release()));
  }
  JniRepeatingCallback2(const JniRepeatingCallback2&) = delete;
  const JniRepeatingCallback2& operator=(const JniRepeatingCallback2&) = delete;

 private:
  std::unique_ptr<JniRepeatingWrappedCallback2Type> wrapped_callback_;
};
}  // namespace

void RunObjectCallbackAndroid(const JavaRef<jobject>& callback,
                              const JavaRef<jobject>& arg) {
  Java_Helper_onObjectResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunObjectCallbackAndroid2(const JavaRef<jobject>& callback,
                               const JavaRef<jobject>& arg1,
                               const JavaRef<jobject>& arg2) {
  Java_JniHelper_onResultFromNative(AttachCurrentThread(), callback, arg1,
                                    arg2);
}

void RunBooleanCallbackAndroid(const JavaRef<jobject>& callback, bool arg) {
  Java_Helper_onBooleanResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunIntCallbackAndroid(const JavaRef<jobject>& callback, int32_t arg) {
  Java_Helper_onIntResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunLongCallbackAndroid(const JavaRef<jobject>& callback, int64_t arg) {
  Java_Helper_onLongResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunTimeCallbackAndroid(const JavaRef<jobject>& callback, base::Time time) {
  RunLongCallbackAndroid(callback, time.InMillisecondsSinceUnixEpoch());
}

void RunStringCallbackAndroid(const JavaRef<jobject>& callback,
                              const std::string& arg) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_string = ConvertUTF8ToJavaString(env, arg);
  Java_Helper_onObjectResultFromNative(env, callback, java_string);
}

void RunOptionalStringCallbackAndroid(
    const JavaRef<jobject>& callback,
    base::optional_ref<const std::string> optional_string_arg) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      callback, optional_string_arg
                    ? ConvertUTF8ToJavaString(env, optional_string_arg.value())
                    : nullptr);
}

void RunByteArrayCallbackAndroid(const JavaRef<jobject>& callback,
                                 const std::vector<uint8_t>& arg) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes = ToJavaByteArray(env, arg);
  Java_Helper_onObjectResultFromNative(env, callback, j_bytes);
}

ScopedJavaLocalRef<jobject> ToJniCallback(JNIEnv* env,
                                          base::OnceClosure&& callback) {
  return ToJniCallback(env, base::BindOnce(
                                [](base::OnceClosure captured_callback,
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
    const base::RepeatingClosure& callback) {
  return ToJniCallback(env,
                       base::BindRepeating(
                           [](const base::RepeatingClosure& captured_callback,
                              const jni_zero::JavaRef<jobject>& j_null) {
                             // For callbacks with no parameters, the parameter
                             // from Java should be null.
                             CHECK(!j_null);
                             captured_callback.Run();
                           },
                           callback));
}

ScopedJavaLocalRef<jobject> ToJniCallback(JNIEnv* env,
                                          base::RepeatingClosure&& callback) {
  return ToJniCallback(env, base::BindRepeating(
                                [](base::RepeatingClosure captured_callback,
                                   const jni_zero::JavaRef<jobject>& j_null) {
                                  // For callbacks with no parameters, the
                                  // parameter from Java should be null.
                                  CHECK(!j_null);
                                  captured_callback.Run();
                                },
                                std::move(callback)));
}

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
    JniOnceWrappedCallback2Type&& callback) {
  return JniOnceCallback2(std::move(callback)).TransferToJava(env);
}

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniRepeatingWrappedCallback2Type&& callback) {
  return JniRepeatingCallback2(std::move(callback)).TransferToJava(env);
}

ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const JniRepeatingWrappedCallback2Type& callback) {
  return JniRepeatingCallback2(callback).TransferToJava(env);
}

static void JNI_JniCallbackImpl_OnResult(
    JNIEnv* env,
    bool isRepeating,
    int64_t callbackPtr,
    const jni_zero::JavaRef<jobject>& j_result) {
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

static void JNI_JniCallbackImpl_OnResult2(
    JNIEnv* env,
    bool isRepeating,
    int64_t callbackPtr,
    const jni_zero::JavaRef<jobject>& j_result1,
    const jni_zero::JavaRef<jobject>& j_result2) {
  if (isRepeating) {
    auto* callback =
        reinterpret_cast<JniRepeatingWrappedCallback2Type*>(callbackPtr);
    callback->Run(j_result1, j_result2);
  } else {
    auto* callback =
        reinterpret_cast<JniOnceWrappedCallback2Type*>(callbackPtr);
    std::move(*callback).Run(j_result1, j_result2);
    delete callback;
  }
}

static void JNI_JniCallbackImpl_Destroy(JNIEnv* env,
                                        bool isRepeating,
                                        int64_t callbackPtr) {
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

}  // namespace android
}  // namespace base

DEFINE_JNI(Callback)
DEFINE_JNI(JniCallbackImpl)
