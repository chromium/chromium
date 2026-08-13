// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include <array>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"

namespace base::android {

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(JNIEnv* env,
                                               const uint8_t* bytes,
                                               size_t len) {
  return ToJavaByteArray(
      env,
      // SAFETY: required from caller, see UNSAFE_BUFFER_USAGE in header.
      UNSAFE_BUFFERS(base::span(bytes, len)));
}

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(
    JNIEnv* env,
    base::span<const uint8_t> bytes) {
  jbyteArray byte_array =
      env->NewByteArray(checked_cast<int32_t>(bytes.size()));
  CheckException(env);
  DCHECK(byte_array);

  env->SetByteArrayRegion(byte_array, 0, checked_cast<int32_t>(bytes.size()),
                          reinterpret_cast<const int8_t*>(bytes.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jbyteArray>::Adopt(env, byte_array);
}

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(JNIEnv* env,
                                               std::string_view str) {
  return ToJavaByteArray(env, base::as_byte_span(str));
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& bools) {
  const size_t size = bools.size();
  if (size <= 1024) {
    std::array<bool, 1024> actual_bools;
    for (size_t i = 0; i < size; ++i) {
      actual_bools[i] = bools[i];
    }
    return ToJavaBooleanArray(env, base::span(actual_bools).first(size));
  }
  // Make an actual array of types equivalent to `bool`.
  auto actual_bools = HeapArray<bool>::Uninit(size);
  std::ranges::copy(bools, actual_bools.begin());
  return ToJavaBooleanArray(env, actual_bools);
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(JNIEnv* env,
                                                     span<const bool> bools) {
  jbooleanArray boolean_array =
      env->NewBooleanArray(checked_cast<int32_t>(bools.size()));
  CheckException(env);
  DCHECK(boolean_array);

  env->SetBooleanArrayRegion(boolean_array, 0,
                             checked_cast<int32_t>(bools.size()),
                             reinterpret_cast<const jboolean*>(bools.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jbooleanArray>::Adopt(env, boolean_array);
}

ScopedJavaLocalRef<jintArray> ToJavaIntArray(JNIEnv* env,
                                             base::span<const int32_t> ints) {
  jintArray int_array = env->NewIntArray(checked_cast<int32_t>(ints.size()));
  CheckException(env);
  DCHECK(int_array);

  env->SetIntArrayRegion(int_array, 0, checked_cast<int32_t>(ints.size()),
                         reinterpret_cast<const int32_t*>(ints.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jintArray>::Adopt(env, int_array);
}

// Returns a new Java long array converted from the given int64_t array.
BASE_EXPORT ScopedJavaLocalRef<jlongArray> ToJavaLongArray(
    JNIEnv* env,
    base::span<const int64_t> longs) {
  jlongArray long_array =
      env->NewLongArray(checked_cast<int32_t>(longs.size()));
  CheckException(env);
  DCHECK(long_array);

  env->SetLongArrayRegion(long_array, 0, checked_cast<int32_t>(longs.size()),
                          reinterpret_cast<const int64_t*>(longs.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jlongArray>::Adopt(env, long_array);
}

BASE_EXPORT ScopedJavaLocalRef<jfloatArray> ToJavaFloatArray(
    JNIEnv* env,
    base::span<const float> floats) {
  jfloatArray float_array =
      env->NewFloatArray(checked_cast<int32_t>(floats.size()));
  CheckException(env);
  DCHECK(float_array);

  env->SetFloatArrayRegion(float_array, 0,
                           checked_cast<int32_t>(floats.size()),
                           reinterpret_cast<const float*>(floats.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jfloatArray>::Adopt(env, float_array);
}

BASE_EXPORT ScopedJavaLocalRef<jdoubleArray> ToJavaDoubleArray(
    JNIEnv* env,
    base::span<const double> doubles) {
  jdoubleArray double_array =
      env->NewDoubleArray(checked_cast<int32_t>(doubles.size()));
  CheckException(env);
  DCHECK(double_array);

  env->SetDoubleArrayRegion(double_array, 0,
                            checked_cast<int32_t>(doubles.size()),
                            reinterpret_cast<const double*>(doubles.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jdoubleArray>::Adopt(env, double_array);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    jclass clazz,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<int32_t>(v.size()), clazz, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  return ToJavaArrayOfObjects(env, jni_zero::g_object_class, v);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaGlobalRef<jobject>> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<int32_t>(v.size()),
                                         jni_zero::g_object_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToTypedJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v,
    jclass type) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<int32_t>(v.size()), type, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToTypedJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaGlobalRef<jobject>> v,
    jclass type) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<int32_t>(v.size()), type, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::string> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(checked_cast<int32_t>(v.size()),
                                         byte_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i),
                               byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::vector<uint8_t>> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(checked_cast<int32_t>(v.size()),
                                         byte_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i),
                               byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::string> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<int32_t>(v.size()),
                                         jni_zero::g_string_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF8ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  jobjectArray joa =
      env->NewObjectArray(checked_cast<int32_t>(vec_outer.size()),
                          string_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), inner.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::u16string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  jobjectArray joa =
      env->NewObjectArray(checked_cast<int32_t>(vec_outer.size()),
                          string_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), inner.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::u16string> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<int32_t>(v.size()),
                                         jni_zero::g_string_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF16ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<int32_t>(i), item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>::Adopt(env, joa);
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::u16string>* out) {
  DCHECK(out);
  if (!array) {
    return;
  }
  int32_t len = array.GetLength(env);
  if (!len) {
    return;
  }
  out->resize(out->size() + static_cast<size_t>(len));
  span<std::u16string> back = span(*out).last(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto str = ScopedJavaLocalRef<jstring>::Adopt(
        env, static_cast<jstring>(env->GetObjectArrayElement(
                 array.obj(), i)));
    ConvertJavaStringToUTF16(env, str.obj(), &back[static_cast<size_t>(i)]);
  }
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::string>* out) {
  DCHECK(out);
  if (!array) {
    return;
  }
  int32_t len = array.GetLength(env);
  if (!len) {
    return;
  }
  out->resize(out->size() + static_cast<size_t>(len));
  span<std::string> back = span(*out).last(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto str = ScopedJavaLocalRef<jstring>::Adopt(
        env, static_cast<jstring>(env->GetObjectArrayElement(
                 array.obj(), i)));
    ConvertJavaStringToUTF8(env, str.obj(), &back[static_cast<size_t>(i)]);
  }
}

void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     const JavaRef<jbyteArray>& byte_array,
                                     std::vector<uint8_t>* out) {
  DCHECK(out);
  if (!byte_array) {
    return;
  }
  int32_t len = byte_array.GetLength(env);
  if (!len) {
    return;
  }
  out->resize(out->size() + static_cast<size_t>(len));
  span<uint8_t> back = span(*out).last(static_cast<size_t>(len));

  env->GetByteArrayRegion(byte_array.obj(), 0, len,
                          reinterpret_cast<int8_t*>(back.data()));
}

void JavaByteArrayToByteVector(JNIEnv* env,
                               const JavaRef<jbyteArray>& byte_array,
                               std::vector<uint8_t>* out) {
  DCHECK(out);
  DCHECK(byte_array);
  out->clear();
  AppendJavaByteArrayToByteVector(env, byte_array, out);
}

size_t JavaByteArrayToByteSpan(JNIEnv* env,
                               const JavaRef<jbyteArray>& byte_array,
                               base::span<uint8_t> dest) {
  CHECK(byte_array);
  int32_t len = byte_array.GetLength(env);
  span<uint8_t> copy_dest = dest.first(static_cast<size_t>(len));

  env->GetByteArrayRegion(byte_array.obj(), 0, len,
                          reinterpret_cast<int8_t*>(copy_dest.data()));
  return static_cast<size_t>(len);
}

void JavaByteArrayToString(JNIEnv* env,
                           const JavaRef<jbyteArray>& byte_array,
                           std::string* out) {
  DCHECK(out);
  CHECK(byte_array);
  int32_t len = byte_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  env->GetByteArrayRegion(byte_array.obj(), 0, len,
                          reinterpret_cast<int8_t*>(out->data()));
  CheckException(env);
}

void JavaBooleanArrayToBoolVector(JNIEnv* env,
                                  const JavaRef<jbooleanArray>& boolean_array,
                                  std::vector<bool>* out) {
  DCHECK(out);
  if (!boolean_array) {
    return;
  }
  int32_t len = boolean_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  if (len <= 1024) {
    std::array<jboolean, 1024> values;
    env->GetBooleanArrayRegion(boolean_array.obj(), 0, len,
                               values.data());
    CheckException(env);
    base::span<jboolean> values_span =
        base::span<jboolean>(values).first(static_cast<size_t>(len));
    for (int32_t i = 0; i < len; ++i) {
      (*out)[static_cast<size_t>(i)] =
          static_cast<bool>(values_span[static_cast<size_t>(i)]);
    }
  } else {
    std::vector<jboolean> values(static_cast<size_t>(len));
    env->GetBooleanArrayRegion(boolean_array.obj(), 0, len,
                               values.data());
    CheckException(env);
    base::span<jboolean> values_span(values);
    for (int32_t i = 0; i < len; ++i) {
      (*out)[static_cast<size_t>(i)] =
          static_cast<bool>(values_span[static_cast<size_t>(i)]);
    }
  }
}

void JavaIntArrayToIntVector(JNIEnv* env,
                             const JavaRef<jintArray>& int_array,
                             std::vector<int>* out) {
  DCHECK(out);
  int32_t len = int_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  env->GetIntArrayRegion(int_array.obj(), 0, len, out->data());
}

void JavaLongArrayToInt64Vector(JNIEnv* env,
                                const JavaRef<jlongArray>& long_array,
                                std::vector<int64_t>* out) {
  DCHECK(out);
  JavaLongArrayToLongVector(env, long_array, out);
}

void JavaLongArrayToLongVector(JNIEnv* env,
                               const JavaRef<jlongArray>& long_array,
                               std::vector<int64_t>* out) {
  DCHECK(out);
  int32_t len = long_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  env->GetLongArrayRegion(long_array.obj(), 0, len, out->data());
}

void JavaFloatArrayToFloatVector(JNIEnv* env,
                                 const JavaRef<jfloatArray>& float_array,
                                 std::vector<float>* out) {
  DCHECK(out);
  int32_t len = float_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  env->GetFloatArrayRegion(float_array.obj(), 0, len, out->data());
}

void JavaDoubleArrayToDoubleVector(JNIEnv* env,
                                   const JavaRef<jdoubleArray>& double_array,
                                   std::vector<double>* out) {
  DCHECK(out);
  int32_t len = double_array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  if (!len) {
    return;
  }
  env->GetDoubleArrayRegion(double_array.obj(), 0, len, out->data());
}

void JavaArrayOfByteArrayToStringVector(JNIEnv* env,
                                        const JavaRef<jobjectArray>& array,
                                        std::vector<std::string>* out) {
  DCHECK(out);
  int32_t len = array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto bytes_array = ScopedJavaLocalRef<jbyteArray>::Adopt(
        env, static_cast<jbyteArray>(env->GetObjectArrayElement(
                 array.obj(), i)));
    JavaByteArrayToString(env, bytes_array, &(*out)[static_cast<size_t>(i)]);
  }
}

void JavaArrayOfByteArrayToBytesVector(JNIEnv* env,
                                       const JavaRef<jobjectArray>& array,
                                       std::vector<std::vector<uint8_t>>* out) {
  DCHECK(out);
  const int32_t len = array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto bytes_array = ScopedJavaLocalRef<jbyteArray>::Adopt(
        env, static_cast<jbyteArray>(env->GetObjectArrayElement(
                 array.obj(), i)));
    JavaByteArrayToByteVector(env, bytes_array, &(*out)[static_cast<size_t>(i)]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::u16string>>* out) {
  DCHECK(out);
  int32_t len = array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto strings_array = ScopedJavaLocalRef<jobjectArray>::Adopt(
        env, static_cast<jobjectArray>(env->GetObjectArrayElement(
                 array.obj(), i)));

    (*out)[static_cast<size_t>(i)].clear();
    AppendJavaStringArrayToStringVector(env, strings_array,
                                        &(*out)[static_cast<size_t>(i)]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::string>>* out) {
  DCHECK(out);
  int32_t len = array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto strings_array = ScopedJavaLocalRef<jobjectArray>::Adopt(
        env, static_cast<jobjectArray>(env->GetObjectArrayElement(
                 array.obj(), i)));

    (*out)[static_cast<size_t>(i)].clear();
    AppendJavaStringArrayToStringVector(env, strings_array,
                                        &(*out)[static_cast<size_t>(i)]);
  }
}

void JavaArrayOfIntArrayToIntVector(JNIEnv* env,
                                    const JavaRef<jobjectArray>& array,
                                    std::vector<std::vector<int>>* out) {
  DCHECK(out);
  int32_t len = array.GetLength(env);
  out->resize(static_cast<size_t>(len));
  for (int32_t i = 0; i < len; ++i) {
    auto int_array = ScopedJavaLocalRef<jintArray>::Adopt(
        env, static_cast<jintArray>(env->GetObjectArrayElement(
                 array.obj(), i)));
    JavaIntArrayToIntVector(env, int_array, &(*out)[static_cast<size_t>(i)]);
  }
}

}  // namespace base::android
