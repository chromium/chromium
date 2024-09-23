// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/heap_array.h"
#include "base/numerics/safe_conversions.h"

namespace base::android {

UNSAFE_BUFFER_USAGE ScopedJavaLocalRef<jbyteArray>
ToJavaByteArray(JNIEnv* env, const uint8_t* bytes, size_t len) {
  return ToJavaByteArray(
      env,
      // SAFETY: The caller must provide a valid pointer and length.
      UNSAFE_BUFFERS(base::span(bytes, len)));
}

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(
    JNIEnv* env,
    base::span<const uint8_t> bytes) {
  jbyteArray byte_array = env->NewByteArray(checked_cast<jsize>(bytes.size()));
  CheckException(env);
  DCHECK(byte_array);

  static_assert(sizeof(jbyte) == sizeof(uint8_t));
  static_assert(alignof(jbyte) <= alignof(uint8_t));
  env->SetByteArrayRegion(byte_array, jsize{0},
                          checked_cast<jsize>(bytes.size()),
                          reinterpret_cast<const jbyte*>(bytes.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jbyteArray>(env, byte_array);
}

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(JNIEnv* env,
                                               std::string_view str) {
  return ToJavaByteArray(env, base::as_byte_span(str));
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& bools) {
  // Make an actual array of types equivalent to `bool`.
  auto actual_bools = HeapArray<bool>::Uninit(bools.size());
  std::ranges::copy(bools, actual_bools.begin());
  return ToJavaBooleanArray(env, actual_bools);
}

ScopedJavaLocalRef<jbooleanArray> ToJavaBooleanArray(JNIEnv* env,
                                                     span<const bool> bools) {
  jbooleanArray boolean_array =
      env->NewBooleanArray(checked_cast<jsize>(bools.size()));
  CheckException(env);
  DCHECK(boolean_array);

  static_assert(sizeof(jboolean) == sizeof(bool));
  static_assert(alignof(jboolean) <= alignof(bool));
  env->SetBooleanArrayRegion(boolean_array, jsize{0},
                             checked_cast<jsize>(bools.size()),
                             reinterpret_cast<const jboolean*>(bools.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jbooleanArray>(env, boolean_array);
}

ScopedJavaLocalRef<jintArray> ToJavaIntArray(JNIEnv* env,
                                             base::span<const int32_t> ints) {
  jintArray int_array = env->NewIntArray(checked_cast<jsize>(ints.size()));
  CheckException(env);
  DCHECK(int_array);

  static_assert(sizeof(jint) == sizeof(int32_t));
  static_assert(alignof(jint) <= alignof(int32_t));
  env->SetIntArrayRegion(int_array, jsize{0}, checked_cast<jsize>(ints.size()),
                         reinterpret_cast<const jint*>(ints.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jintArray>(env, int_array);
}

// Returns a new Java long array converted from the given int64_t array.
BASE_EXPORT ScopedJavaLocalRef<jlongArray> ToJavaLongArray(
    JNIEnv* env,
    base::span<const int64_t> longs) {
  jlongArray long_array = env->NewLongArray(checked_cast<jsize>(longs.size()));
  CheckException(env);
  DCHECK(long_array);

  static_assert(sizeof(jlong) == sizeof(int64_t));
  static_assert(alignof(jlong) <= alignof(int64_t));
  env->SetLongArrayRegion(long_array, jsize{0},
                          checked_cast<jsize>(longs.size()),
                          reinterpret_cast<const jlong*>(longs.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jlongArray>(env, long_array);
}

BASE_EXPORT ScopedJavaLocalRef<jfloatArray> ToJavaFloatArray(
    JNIEnv* env,
    base::span<const float> floats) {
  jfloatArray float_array =
      env->NewFloatArray(checked_cast<jsize>(floats.size()));
  CheckException(env);
  DCHECK(float_array);

  static_assert(sizeof(jfloat) == sizeof(float));
  static_assert(alignof(jfloat) <= alignof(float));
  env->SetFloatArrayRegion(float_array, jsize{0},
                           checked_cast<jsize>(floats.size()),
                           reinterpret_cast<const jfloat*>(floats.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jfloatArray>(env, float_array);
}

BASE_EXPORT ScopedJavaLocalRef<jdoubleArray> ToJavaDoubleArray(
    JNIEnv* env,
    base::span<const double> doubles) {
  jdoubleArray double_array =
      env->NewDoubleArray(checked_cast<jsize>(doubles.size()));
  CheckException(env);
  DCHECK(double_array);

  static_assert(sizeof(jdouble) == sizeof(double));
  static_assert(alignof(jdouble) <= alignof(double));
  env->SetDoubleArrayRegion(double_array, jsize{0},
                            checked_cast<jsize>(doubles.size()),
                            reinterpret_cast<const jdouble*>(doubles.data()));
  CheckException(env);

  return ScopedJavaLocalRef<jdoubleArray>(env, double_array);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    jclass clazz,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<jsize>(v.size()), clazz, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v) {
  return ToJavaArrayOfObjects(env, jni_zero::g_object_class, v);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaGlobalRef<jobject>> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(v.size()),
                                         jni_zero::g_object_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToTypedJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaLocalRef<jobject>> v,
    jclass type) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<jsize>(v.size()), type, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

BASE_EXPORT ScopedJavaLocalRef<jobjectArray> ToTypedJavaArrayOfObjects(
    JNIEnv* env,
    base::span<const ScopedJavaGlobalRef<jobject>> v,
    jclass type) {
  jobjectArray joa =
      env->NewObjectArray(checked_cast<jsize>(v.size()), type, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::string> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(v.size()),
                                         byte_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env,
    base::span<const std::vector<uint8_t>> v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(v.size()),
                                         byte_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::string> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(v.size()),
                                         jni_zero::g_string_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF8ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(vec_outer.size()),
                                         string_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), inner.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStringArray(
    JNIEnv* env,
    base::span<const std::vector<std::u16string>> vec_outer) {
  ScopedJavaLocalRef<jclass> string_array_clazz =
      GetClass(env, "[Ljava/lang/String;");

  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(vec_outer.size()),
                                         string_array_clazz.obj(), nullptr);
  CheckException(env);

  for (size_t i = 0; i < vec_outer.size(); ++i) {
    ScopedJavaLocalRef<jobjectArray> inner =
        ToJavaArrayOfStrings(env, vec_outer[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), inner.obj());
  }

  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,
    base::span<const std::u16string> v) {
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(v.size()),
                                         jni_zero::g_string_class, nullptr);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF16ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, checked_cast<jsize>(i), item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::u16string>* out) {
  DCHECK(out);
  if (!array)
    return;
  size_t len = SafeGetArrayLength(env, array);
  if (!len) {
    return;
  }
  out->resize(out->size() + len);
  span<std::u16string> back = span(*out).last(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jstring> str(
        env, static_cast<jstring>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));
    ConvertJavaStringToUTF16(env, str.obj(), &back[i]);
  }
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         const JavaRef<jobjectArray>& array,
                                         std::vector<std::string>* out) {
  DCHECK(out);
  if (!array)
    return;
  size_t len = SafeGetArrayLength(env, array);
  if (!len) {
    return;
  }
  out->resize(out->size() + len);
  span<std::string> back = span(*out).last(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jstring> str(
        env, static_cast<jstring>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));
    ConvertJavaStringToUTF8(env, str.obj(), &back[i]);
  }
}

void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     const JavaRef<jbyteArray>& byte_array,
                                     std::vector<uint8_t>* out) {
  DCHECK(out);
  if (!byte_array)
    return;
  size_t len = SafeGetArrayLength(env, byte_array);
  if (!len) {
    return;
  }
  out->resize(out->size() + len);
  span<uint8_t> back = span(*out).last(len);

  static_assert(sizeof(jbyte) == sizeof(uint8_t));
  static_assert(alignof(jbyte) <= alignof(uint8_t));
  env->GetByteArrayRegion(byte_array.obj(), jsize{0},
                          checked_cast<jsize>(back.size()),
                          reinterpret_cast<jbyte*>(back.data()));
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
  size_t len = SafeGetArrayLength(env, byte_array);
  span<uint8_t> copy_dest = dest.first(len);

  static_assert(sizeof(jbyte) == sizeof(uint8_t));
  static_assert(alignof(jbyte) <= alignof(uint8_t));
  env->GetByteArrayRegion(byte_array.obj(), jsize{0},
                          checked_cast<jsize>(copy_dest.size()),
                          reinterpret_cast<jbyte*>(copy_dest.data()));
  return len;
}

void JavaByteArrayToString(JNIEnv* env,
                           const JavaRef<jbyteArray>& byte_array,
                           std::string* out) {
  DCHECK(out);
  DCHECK(byte_array);

  std::vector<uint8_t> byte_vector;
  JavaByteArrayToByteVector(env, byte_array, &byte_vector);
  out->assign(byte_vector.begin(), byte_vector.end());
}

void JavaBooleanArrayToBoolVector(JNIEnv* env,
                                  const JavaRef<jbooleanArray>& boolean_array,
                                  std::vector<bool>* out) {
  DCHECK(out);
  if (!boolean_array)
    return;
  size_t len = SafeGetArrayLength(env, boolean_array);
  out->resize(len);
  if (!len) {
    return;
  }
  // SAFETY: `SafeGetArrayLength()` returns the number of elements in the
  // `boolean_array`, though it can return 0 if the array is invalid. So we only
  // call `GetBooleanArrayElements()` when it's positive. Then
  // GetBooleanArrayElements() returns a buffer of the size returned from
  // `SafeGetArrayLength()`.
  span<jboolean> values = UNSAFE_BUFFERS(
      span(env->GetBooleanArrayElements(boolean_array.obj(), nullptr), len));
  for (size_t i = 0; i < values.size(); ++i) {
    (*out)[i] = static_cast<bool>(values[i]);
  }
  env->ReleaseBooleanArrayElements(boolean_array.obj(), values.data(),
                                   JNI_ABORT);
}

void JavaIntArrayToIntVector(JNIEnv* env,
                             const JavaRef<jintArray>& int_array,
                             std::vector<int>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, int_array);
  out->resize(len);
  if (!len)
    return;
  env->GetIntArrayRegion(int_array.obj(), jsize{0}, checked_cast<jsize>(len),
                         out->data());
}

void JavaLongArrayToInt64Vector(JNIEnv* env,
                                const JavaRef<jlongArray>& long_array,
                                std::vector<int64_t>* out) {
  DCHECK(out);
  std::vector<jlong> temp;
  JavaLongArrayToLongVector(env, long_array, &temp);
  out->resize(0);
  Extend(*out, temp);
}

void JavaLongArrayToLongVector(JNIEnv* env,
                               const JavaRef<jlongArray>& long_array,
                               std::vector<jlong>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, long_array);
  out->resize(len);
  if (!len)
    return;
  env->GetLongArrayRegion(long_array.obj(), jsize{0}, checked_cast<jsize>(len),
                          out->data());
}

void JavaFloatArrayToFloatVector(JNIEnv* env,
                                 const JavaRef<jfloatArray>& float_array,
                                 std::vector<float>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, float_array);
  out->resize(len);
  if (!len)
    return;
  env->GetFloatArrayRegion(float_array.obj(), jsize{0},
                           checked_cast<jsize>(len), out->data());
}

void JavaDoubleArrayToDoubleVector(JNIEnv* env,
                                   const JavaRef<jdoubleArray>& double_array,
                                   std::vector<double>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, double_array);
  out->resize(len);
  if (!len)
    return;
  env->GetDoubleArrayRegion(double_array.obj(), jsize{0},
                            checked_cast<jsize>(len), out->data());
}

void JavaArrayOfByteArrayToStringVector(JNIEnv* env,
                                        const JavaRef<jobjectArray>& array,
                                        std::vector<std::string>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, array);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jbyteArray> bytes_array(
        env, static_cast<jbyteArray>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));
    size_t bytes_len = SafeGetArrayLength(env, bytes_array);
    // SAFETY: `SafeGetArrayLength()` returns the number of elements in the
    // `boobytes_array`, though it can return 0 if the array is invalid. So we
    // only call `GetByteArrayElements()` when it's positive. Then
    // GetByteArrayElements() returns a buffer of the size returned from
    // `SafeGetArrayLength()`.
    if (!bytes_len) {
      (*out)[i].clear();
      continue;
    }
    span<jbyte> bytes = UNSAFE_BUFFERS(
        span(env->GetByteArrayElements(bytes_array.obj(), nullptr), bytes_len));
    (*out)[i] = base::as_string_view(base::as_bytes(bytes));
    env->ReleaseByteArrayElements(bytes_array.obj(), bytes.data(), JNI_ABORT);
  }
}

void JavaArrayOfByteArrayToBytesVector(JNIEnv* env,
                                       const JavaRef<jobjectArray>& array,
                                       std::vector<std::vector<uint8_t>>* out) {
  DCHECK(out);
  const size_t len = SafeGetArrayLength(env, array);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jbyteArray> bytes_array(
        env, static_cast<jbyteArray>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));
    JavaByteArrayToByteVector(env, bytes_array, &(*out)[i]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::u16string>>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, array);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobjectArray> strings_array(
        env, static_cast<jobjectArray>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));

    (*out)[i].clear();
    AppendJavaStringArrayToStringVector(env, strings_array, &(*out)[i]);
  }
}

void Java2dStringArrayTo2dStringVector(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array,
    std::vector<std::vector<std::string>>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, array);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobjectArray> strings_array(
        env, static_cast<jobjectArray>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));

    (*out)[i].clear();
    AppendJavaStringArrayToStringVector(env, strings_array, &(*out)[i]);
  }
}

void JavaArrayOfIntArrayToIntVector(JNIEnv* env,
                                    const JavaRef<jobjectArray>& array,
                                    std::vector<std::vector<int>>* out) {
  DCHECK(out);
  size_t len = SafeGetArrayLength(env, array);
  out->resize(len);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jintArray> int_array(
        env, static_cast<jintArray>(env->GetObjectArrayElement(
                 array.obj(), checked_cast<jsize>(i))));
    JavaIntArrayToIntVector(env, int_array, &(*out)[i]);
  }
}

}  // namespace base::android
