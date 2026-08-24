// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "third_party/jni_zero/default_conversions.h"

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
  return jni_zero::NewArray<uint8_t>(env, bytes);
}

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(JNIEnv* env,
                                               std::string_view str) {
  return ToJavaByteArray(env, base::as_byte_span(str));
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& bools) {
  return jni_zero::NewArray(env, bools);
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(
    JNIEnv* env,
    base::span<const bool> bools) {
  return jni_zero::NewArray<bool>(env, bools);
}

ScopedJavaLocalRef<jintArray> ToJavaIntArray(JNIEnv* env,
                                             base::span<const int32_t> ints) {
  return jni_zero::NewArray<int32_t>(env, ints);
}

ScopedJavaLocalRef<jlongArray> ToJavaLongArray(
    JNIEnv* env,
    base::span<const int64_t> longs) {
  return jni_zero::NewArray<int64_t>(env, longs);
}

ScopedJavaLocalRef<jfloatArray> ToJavaFloatArray(
    JNIEnv* env,
    base::span<const float> floats) {
  return jni_zero::NewArray<float>(env, floats);
}

ScopedJavaLocalRef<jdoubleArray> ToJavaDoubleArray(
    JNIEnv* env,
    base::span<const double> doubles) {
  return jni_zero::NewArray<double>(env, doubles);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    jclass clazz,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  return jni_zero::NewArray<jobject>(env, v, clazz);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  return jni_zero::NewObjectArray<ScopedJavaLocalRef<jobject>>(env, v);
}

ScopedJavaLocalRef<jobjectArray> ToTypedJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v,
    jclass type) {
  return jni_zero::NewArray<jobject>(env, v, type);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::string> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  ScopedJavaLocalRef<jobjectArray> joa =
      jni_zero::NewArray<jobject>(env, v.size(), byte_array_clazz.obj());
  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    joa.Set(env, i, byte_array);
  }
  return joa;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::vector<uint8_t>> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  ScopedJavaLocalRef<jobjectArray> joa =
      jni_zero::NewArray<jobject>(env, v.size(), byte_array_clazz.obj());
  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    joa.Set(env, i, byte_array);
  }
  return joa;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::string> v) {
  return jni_zero::NewStringArray<std::string>(env, v);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  ScopedJavaLocalRef<jobjectArray> joa = jni_zero::NewArray<jobject>(
      env, vec_outer.size(), string_array_clazz.obj());

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    joa.Set(env, i, inner);
  }

  return joa;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::u16string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  ScopedJavaLocalRef<jobjectArray> joa = jni_zero::NewArray<jobject>(
      env, vec_outer.size(), string_array_clazz.obj());

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    joa.Set(env, i, inner);
  }

  return joa;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::u16string> v) {
  return jni_zero::NewStringArray<std::u16string>(env, v);
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::u16string>* out) {
  DCHECK(out);
  if (!array) {
    return;
  }
  size_t len = array.GetSize(env);
  if (!len) {
    return;
  }
  size_t old_size = out->size();
  out->resize(old_size + len);
  span<std::u16string> back = span(*out).last(len);
  for (size_t i = 0; i < len; ++i) {
    back[i] = array.GetAs<std::u16string>(env, i);
  }
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::string>* out) {
  DCHECK(out);
  if (!array) {
    return;
  }
  size_t len = array.GetSize(env);
  if (!len) {
    return;
  }
  size_t old_size = out->size();
  out->resize(old_size + len);
  span<std::string> back = span(*out).last(len);
  for (size_t i = 0; i < len; ++i) {
    back[i] = array.GetAs<std::string>(env, i);
  }
}

void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     const JavaRef<jbyteArray>& byte_array,
                                     std::vector<uint8_t>* out) {
  DCHECK(out);
  if (!byte_array) {
    return;
  }
  size_t len = byte_array.GetSize(env);
  if (!len) {
    return;
  }
  size_t old_size = out->size();
  out->resize(old_size + len);
  byte_array.CopyTo(env, span(*out).subspan(old_size).data(), len);
}

void JavaByteArrayToByteVector(JNIEnv* env,
                               const JavaRef<jbyteArray>& byte_array,
                               std::vector<uint8_t>* out) {
  DCHECK(out);
  DCHECK(byte_array);
  size_t len = byte_array.GetSize(env);
  out->resize(len);
  byte_array.CopyTo(env, out->data(), len);
}

size_t JavaByteArrayToByteSpan(JNIEnv* env,
                               const JavaRef<jbyteArray>& byte_array,
                               base::span<uint8_t> dest) {
  CHECK(byte_array);
  size_t len = byte_array.GetSize(env);
  CHECK_GE(dest.size(), len);
  byte_array.CopyTo(env, dest.data(), len);
  return len;
}

void JavaByteArrayToString(JNIEnv* env,
                           const JavaRef<jbyteArray>& byte_array,
                           std::string* out) {
  DCHECK(out);
  CHECK(byte_array);
  size_t len = byte_array.GetSize(env);
  out->resize(len);
  byte_array.CopyTo(env, out->data(), len);
}

void JavaBooleanArrayToBoolVector(JNIEnv* env,
                                  const JavaRef<jbooleanArray>& boolean_array,
                                  std::vector<bool>* out) {
  DCHECK(out);
  if (!boolean_array) {
    return;
  }
  *out = jni_zero::FromJniArray<std::vector<bool>>(env, boolean_array);
}

void JavaIntArrayToIntVector(JNIEnv* env,
                             const JavaRef<jintArray>& int_array,
                             std::vector<int>* out) {
  DCHECK(out);
  size_t len = int_array.GetSize(env);
  out->resize(len);
  int_array.CopyTo(env, out->data(), len);
}

void JavaLongArrayToInt64Vector(JNIEnv* env,
                                const JavaRef<jlongArray>& long_array,
                                std::vector<int64_t>* out) {
  JavaLongArrayToLongVector(env, long_array, out);
}

void JavaLongArrayToLongVector(JNIEnv* env,
                               const JavaRef<jlongArray>& long_array,
                               std::vector<int64_t>* out) {
  DCHECK(out);
  size_t len = long_array.GetSize(env);
  out->resize(len);
  long_array.CopyTo(env, out->data(), len);
}

void JavaFloatArrayToFloatVector(JNIEnv* env,
                                 const JavaRef<jfloatArray>& float_array,
                                 std::vector<float>* out) {
  DCHECK(out);
  size_t len = float_array.GetSize(env);
  out->resize(len);
  float_array.CopyTo(env, out->data(), len);
}

void JavaDoubleArrayToDoubleVector(JNIEnv* env,
                                   const JavaRef<jdoubleArray>& double_array,
                                   std::vector<double>* out) {
  DCHECK(out);
  size_t len = double_array.GetSize(env);
  out->resize(len);
  double_array.CopyTo(env, out->data(), len);
}

void JavaArrayOfByteArrayToStringVector(JNIEnv* env,
                                        const JavaRef<jobjectArray>& array,
                                        std::vector<std::string>* out) {
  DCHECK(out);
  size_t len = array.GetSize(env);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jbyteArray> bytes_array =
        array.GetAs<jbyteArray>(env, i);
    JavaByteArrayToString(env, bytes_array, &(*out)[i]);
  }
}

void JavaArrayOfByteArrayToBytesVector(JNIEnv* env,
                                       const JavaRef<jobjectArray>& array,
                                       std::vector<std::vector<uint8_t>>* out) {
  DCHECK(out);
  const size_t len = array.GetSize(env);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jbyteArray> bytes_array =
        array.GetAs<jbyteArray>(env, i);
    JavaByteArrayToByteVector(env, bytes_array, &(*out)[i]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::u16string>>* out) {
  DCHECK(out);
  size_t len = array.GetSize(env);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobjectArray> strings_array =
        array.GetAs<jobjectArray>(env, i);
    (*out)[i].clear();
    AppendJavaStringArrayToStringVector(env, strings_array, &(*out)[i]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::string>>* out) {
  DCHECK(out);
  size_t len = array.GetSize(env);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobjectArray> strings_array =
        array.GetAs<jobjectArray>(env, i);
    (*out)[i].clear();
    AppendJavaStringArrayToStringVector(env, strings_array, &(*out)[i]);
  }
}

void JavaArrayOfIntArrayToIntVector(JNIEnv* env,
                                    const JavaRef<jobjectArray>& array,
                                    std::vector<std::vector<int>>* out) {
  DCHECK(out);
  size_t len = array.GetSize(env);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jintArray> int_array = array.GetAs<jintArray>(env, i);
    JavaIntArrayToIntVector(env, int_array, &(*out)[i]);
  }
}

}  // namespace base::android
