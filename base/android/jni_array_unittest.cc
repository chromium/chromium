// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(JniArray, BasicConversions) {
  const uint8_t kBytes[] = {0, 1, 2, 3};
  const size_t kLen = base::size(kBytes);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> bytes = ToJavaByteArray(env, kBytes, kLen);
  ASSERT_TRUE(bytes);

  std::vector<uint8_t> inputVector(kBytes, kBytes + kLen);
  ScopedJavaLocalRef<jbyteArray> bytesFromVector =
      ToJavaByteArray(env, inputVector);
  ASSERT_TRUE(bytesFromVector);

  std::vector<uint8_t> vectorFromBytes(5);
  std::vector<uint8_t> vectorFromVector(5);
  JavaByteArrayToByteVector(env, bytes, &vectorFromBytes);
  JavaByteArrayToByteVector(env, bytesFromVector, &vectorFromVector);
  EXPECT_EQ(4U, vectorFromBytes.size());
  EXPECT_EQ(4U, vectorFromVector.size());
  std::vector<uint8_t> expected_vec(kBytes, kBytes + kLen);
  EXPECT_EQ(expected_vec, vectorFromBytes);
  EXPECT_EQ(expected_vec, vectorFromVector);

  AppendJavaByteArrayToByteVector(env, bytes, &vectorFromBytes);
  EXPECT_EQ(8U, vectorFromBytes.size());
  expected_vec.insert(expected_vec.end(), kBytes, kBytes + kLen);
  EXPECT_EQ(expected_vec, vectorFromBytes);
}

TEST(JniArray, ByteArrayStringConversions) {
  JNIEnv* env = AttachCurrentThread();
  std::string inputString("hello\0world");
  ScopedJavaLocalRef<jbyteArray> bytesFromString =
      ToJavaByteArray(env, inputString);
  ASSERT_TRUE(bytesFromString);

  std::string stringFromString;
  JavaByteArrayToString(env, bytesFromString, &stringFromString);
  EXPECT_EQ(inputString, stringFromString);
}

void CheckBoolConversion(JNIEnv* env,
                         const bool* bool_array,
                         const size_t len,
                         const ScopedJavaLocalRef<jbooleanArray>& booleans) {
  ASSERT_TRUE(booleans);

  jsize java_array_len = env->GetArrayLength(booleans.obj());
  ASSERT_EQ(static_cast<jsize>(len), java_array_len);

  jboolean value;
  for (size_t i = 0; i < len; ++i) {
    env->GetBooleanArrayRegion(booleans.obj(), i, 1, &value);
    ASSERT_EQ(bool_array[i], value);
  }
}

TEST(JniArray, BoolConversions) {
  const bool kBools[] = {false, true, false};
  const size_t kLen = base::size(kBools);

  JNIEnv* env = AttachCurrentThread();
  CheckBoolConversion(env, kBools, kLen, ToJavaBooleanArray(env, kBools, kLen));
}

void CheckIntConversion(
    JNIEnv* env,
    const int* int_array,
    const size_t len,
    const ScopedJavaLocalRef<jintArray>& ints) {
  ASSERT_TRUE(ints);

  jsize java_array_len = env->GetArrayLength(ints.obj());
  ASSERT_EQ(static_cast<jsize>(len), java_array_len);

  jint value;
  for (size_t i = 0; i < len; ++i) {
    env->GetIntArrayRegion(ints.obj(), i, 1, &value);
    ASSERT_EQ(int_array[i], value);
  }
}

TEST(JniArray, IntConversions) {
  const int kInts[] = {0, 1, -1, std::numeric_limits<int32_t>::min(),
                       std::numeric_limits<int32_t>::max()};
  const size_t kLen = base::size(kInts);

  JNIEnv* env = AttachCurrentThread();
  CheckIntConversion(env, kInts, kLen, ToJavaIntArray(env, kInts, kLen));

  const std::vector<int> vec(kInts, kInts + kLen);
  CheckIntConversion(env, kInts, kLen, ToJavaIntArray(env, vec));
}

void CheckLongConversion(JNIEnv* env,
                         const int64_t* long_array,
                         const size_t len,
                         const ScopedJavaLocalRef<jlongArray>& longs) {
  ASSERT_TRUE(longs);

  jsize java_array_len = env->GetArrayLength(longs.obj());
  ASSERT_EQ(static_cast<jsize>(len), java_array_len);

  jlong value;
  for (size_t i = 0; i < len; ++i) {
    env->GetLongArrayRegion(longs.obj(), i, 1, &value);
    ASSERT_EQ(long_array[i], value);
  }
}

TEST(JniArray, LongConversions) {
  const int64_t kLongs[] = {0, 1, -1, std::numeric_limits<int64_t>::min(),
                            std::numeric_limits<int64_t>::max()};
  const size_t kLen = base::size(kLongs);

  JNIEnv* env = AttachCurrentThread();
  CheckLongConversion(env, kLongs, kLen, ToJavaLongArray(env, kLongs, kLen));

  const std::vector<int64_t> vec(kLongs, kLongs + kLen);
  CheckLongConversion(env, kLongs, kLen, ToJavaLongArray(env, vec));
}

void CheckIntArrayConversion(JNIEnv* env,
                             ScopedJavaLocalRef<jintArray> jints,
                             std::vector<int> int_vector,
                             const size_t len) {
  jint value;
  for (size_t i = 0; i < len; ++i) {
    env->GetIntArrayRegion(jints.obj(), i, 1, &value);
    ASSERT_EQ(int_vector[i], value);
  }
}

void CheckBoolArrayConversion(JNIEnv* env,
                              ScopedJavaLocalRef<jbooleanArray> jbooleans,
                              std::vector<bool> bool_vector,
                              const size_t len) {
  jboolean value;
  for (size_t i = 0; i < len; ++i) {
    env->GetBooleanArrayRegion(jbooleans.obj(), i, 1, &value);
    ASSERT_EQ(bool_vector[i], value);
  }
}

void CheckFloatConversion(
    JNIEnv* env,
    const float* float_array,
    const size_t len,
    const ScopedJavaLocalRef<jfloatArray>& floats) {
  ASSERT_TRUE(floats);

  jsize java_array_len = env->GetArrayLength(floats.obj());
  ASSERT_EQ(static_cast<jsize>(len), java_array_len);

  jfloat value;
  for (size_t i = 0; i < len; ++i) {
    env->GetFloatArrayRegion(floats.obj(), i, 1, &value);
    ASSERT_EQ(float_array[i], value);
  }
}

TEST(JniArray, ArrayOfStringArrayConversion) {
  std::vector<std::vector<string16>> kArrays = {
      {ASCIIToUTF16("a"), ASCIIToUTF16("f")},
      {ASCIIToUTF16("a"), ASCIIToUTF16("")},
      {},
      {ASCIIToUTF16("")}};

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> joa = ToJavaArrayOfStringArray(env, kArrays);

  std::vector<std::vector<string16>> out;
  Java2dStringArrayTo2dStringVector(env, joa, &out);
  ASSERT_TRUE(kArrays == out);
}

TEST(JniArray, FloatConversions) {
  const float kFloats[] = { 0.0f, 1.0f, -10.0f};
  const size_t kLen = base::size(kFloats);

  JNIEnv* env = AttachCurrentThread();
  CheckFloatConversion(env, kFloats, kLen,
                       ToJavaFloatArray(env, kFloats, kLen));

  const std::vector<float> vec(kFloats, kFloats + kLen);
  CheckFloatConversion(env, kFloats, kLen, ToJavaFloatArray(env, vec));
}

TEST(JniArray, JavaBooleanArrayToBoolVector) {
  const bool kBools[] = {false, true, false};
  const size_t kLen = base::size(kBools);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbooleanArray> jbooleans(env, env->NewBooleanArray(kLen));
  ASSERT_TRUE(jbooleans);

  for (size_t i = 0; i < kLen; ++i) {
    jboolean j = static_cast<jboolean>(kBools[i]);
    env->SetBooleanArrayRegion(jbooleans.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<bool> bools;
  JavaBooleanArrayToBoolVector(env, jbooleans, &bools);

  ASSERT_EQ(static_cast<jsize>(bools.size()),
            env->GetArrayLength(jbooleans.obj()));

  CheckBoolArrayConversion(env, jbooleans, bools, kLen);
}

TEST(JniArray, JavaIntArrayToIntVector) {
  const int kInts[] = {0, 1, -1};
  const size_t kLen = base::size(kInts);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> jints(env, env->NewIntArray(kLen));
  ASSERT_TRUE(jints);

  for (size_t i = 0; i < kLen; ++i) {
    jint j = static_cast<jint>(kInts[i]);
    env->SetIntArrayRegion(jints.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<int> ints;
  JavaIntArrayToIntVector(env, jints, &ints);

  ASSERT_EQ(static_cast<jsize>(ints.size()), env->GetArrayLength(jints.obj()));

  CheckIntArrayConversion(env, jints, ints, kLen);
}

TEST(JniArray, JavaLongArrayToInt64Vector) {
  const int64_t kInt64s[] = {0LL, 1LL, -1LL};
  const size_t kLen = base::size(kInt64s);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jlongs(env, env->NewLongArray(kLen));
  ASSERT_TRUE(jlongs);

  for (size_t i = 0; i < kLen; ++i) {
    jlong j = static_cast<jlong>(kInt64s[i]);
    env->SetLongArrayRegion(jlongs.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<int64_t> int64s;
  JavaLongArrayToInt64Vector(env, jlongs, &int64s);

  ASSERT_EQ(static_cast<jsize>(int64s.size()),
            env->GetArrayLength(jlongs.obj()));

  jlong value;
  for (size_t i = 0; i < kLen; ++i) {
    env->GetLongArrayRegion(jlongs.obj(), i, 1, &value);
    ASSERT_EQ(int64s[i], value);
    ASSERT_EQ(kInt64s[i], int64s[i]);
  }
}

TEST(JniArray, JavaLongArrayToLongVector) {
  const int64_t kInt64s[] = {0LL, 1LL, -1LL};
  const size_t kLen = base::size(kInt64s);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jlongs(env, env->NewLongArray(kLen));
  ASSERT_TRUE(jlongs);

  for (size_t i = 0; i < kLen; ++i) {
    jlong j = static_cast<jlong>(kInt64s[i]);
    env->SetLongArrayRegion(jlongs.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<jlong> jlongs_vector;
  JavaLongArrayToLongVector(env, jlongs, &jlongs_vector);

  ASSERT_EQ(static_cast<jsize>(jlongs_vector.size()),
            env->GetArrayLength(jlongs.obj()));

  jlong value;
  for (size_t i = 0; i < kLen; ++i) {
    env->GetLongArrayRegion(jlongs.obj(), i, 1, &value);
    ASSERT_EQ(jlongs_vector[i], value);
  }
}

TEST(JniArray, JavaFloatArrayToFloatVector) {
  const float kFloats[] = {0.0, 0.5, -0.5};
  const size_t kLen = base::size(kFloats);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jfloatArray> jfloats(env, env->NewFloatArray(kLen));
  ASSERT_TRUE(jfloats);

  for (size_t i = 0; i < kLen; ++i) {
    jfloat j = static_cast<jfloat>(kFloats[i]);
    env->SetFloatArrayRegion(jfloats.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<float> floats;
  JavaFloatArrayToFloatVector(env, jfloats, &floats);

  ASSERT_EQ(static_cast<jsize>(floats.size()),
      env->GetArrayLength(jfloats.obj()));

  jfloat value;
  for (size_t i = 0; i < kLen; ++i) {
    env->GetFloatArrayRegion(jfloats.obj(), i, 1, &value);
    ASSERT_EQ(floats[i], value);
  }
}

TEST(JniArray, JavaArrayOfByteArrayToStringVector) {
  const int kMaxItems = 50;
  JNIEnv* env = AttachCurrentThread();

  // Create a byte[][] object.
  ScopedJavaLocalRef<jclass> byte_array_clazz(env, env->FindClass("[B"));
  ASSERT_TRUE(byte_array_clazz);

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kMaxItems, byte_array_clazz.obj(), NULL));
  ASSERT_TRUE(array);

  // Create kMaxItems byte buffers.
  char text[16];
  for (int i = 0; i < kMaxItems; ++i) {
    snprintf(text, sizeof text, "%d", i);
    ScopedJavaLocalRef<jbyteArray> byte_array =
        ToJavaByteArray(env, reinterpret_cast<uint8_t*>(text),
                        static_cast<size_t>(strlen(text)));
    ASSERT_TRUE(byte_array);

    env->SetObjectArrayElement(array.obj(), i, byte_array.obj());
    ASSERT_FALSE(HasException(env));
  }

  // Convert to std::vector<std::string>, check the content.
  std::vector<std::string> vec;
  JavaArrayOfByteArrayToStringVector(env, array, &vec);

  EXPECT_EQ(static_cast<size_t>(kMaxItems), vec.size());
  for (int i = 0; i < kMaxItems; ++i) {
    snprintf(text, sizeof text, "%d", i);
    EXPECT_STREQ(text, vec[i].c_str());
  }
}

TEST(JniArray, JavaArrayOfStringArrayToVectorOfStringVector) {
  const std::vector<std::vector<string16>> kArrays = {
      {ASCIIToUTF16("a"), ASCIIToUTF16("f")},
      {ASCIIToUTF16("a"), ASCIIToUTF16("")},
      {},
      {ASCIIToUTF16("")}};

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kArrays.size(),
                               env->FindClass("[Ljava/lang/String;"), NULL));
  ASSERT_TRUE(array);

  ScopedJavaLocalRef<jclass> string_clazz(env,
                                          env->FindClass("java/lang/String"));
  ASSERT_TRUE(string_clazz);

  for (size_t i = 0; i < kArrays.size(); ++i) {
    const std::vector<string16>& child_data = kArrays[i];

    ScopedJavaLocalRef<jobjectArray> child_array(
        env, env->NewObjectArray(child_data.size(), string_clazz.obj(), NULL));
    ASSERT_TRUE(child_array);

    for (size_t j = 0; j < child_data.size(); ++j) {
      ScopedJavaLocalRef<jstring> item =
          base::android::ConvertUTF16ToJavaString(env, child_data[j]);
      env->SetObjectArrayElement(child_array.obj(), j, item.obj());
      ASSERT_FALSE(HasException(env));
    }
    env->SetObjectArrayElement(array.obj(), i, child_array.obj());
  }

  std::vector<std::vector<string16>> vec;
  Java2dStringArrayTo2dStringVector(env, array, &vec);

  ASSERT_EQ(kArrays, vec);
}

TEST(JniArray, JavaArrayOfIntArrayToIntVector) {
  const size_t kNumItems = 4;
  JNIEnv* env = AttachCurrentThread();

  // Create an int[][] object.
  ScopedJavaLocalRef<jclass> int_array_clazz(env, env->FindClass("[I"));
  ASSERT_TRUE(int_array_clazz);

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kNumItems, int_array_clazz.obj(), nullptr));
  ASSERT_TRUE(array);

  // Populate int[][] object.
  const int kInts0[] = {0, 1, -1, std::numeric_limits<int32_t>::min(),
                        std::numeric_limits<int32_t>::max()};
  const size_t kLen0 = base::size(kInts0);
  ScopedJavaLocalRef<jintArray> int_array0 = ToJavaIntArray(env, kInts0, kLen0);
  env->SetObjectArrayElement(array.obj(), 0, int_array0.obj());

  const int kInts1[] = {3, 4, 5};
  const size_t kLen1 = base::size(kInts1);
  ScopedJavaLocalRef<jintArray> int_array1 = ToJavaIntArray(env, kInts1, kLen1);
  env->SetObjectArrayElement(array.obj(), 1, int_array1.obj());

  const int kInts2[] = {};
  const size_t kLen2 = 0;
  ScopedJavaLocalRef<jintArray> int_array2 = ToJavaIntArray(env, kInts2, kLen2);
  env->SetObjectArrayElement(array.obj(), 2, int_array2.obj());

  const int kInts3[] = {16};
  const size_t kLen3 = base::size(kInts3);
  ScopedJavaLocalRef<jintArray> int_array3 = ToJavaIntArray(env, kInts3, kLen3);
  env->SetObjectArrayElement(array.obj(), 3, int_array3.obj());

  // Convert to std::vector<std::vector<int>>, check the content.
  std::vector<std::vector<int>> out;
  JavaArrayOfIntArrayToIntVector(env, array, &out);

  EXPECT_EQ(kNumItems, out.size());
  CheckIntArrayConversion(env, int_array0, out[0], kLen0);
  CheckIntArrayConversion(env, int_array1, out[1], kLen1);
  CheckIntArrayConversion(env, int_array2, out[2], kLen2);
  CheckIntArrayConversion(env, int_array3, out[3], kLen3);
}

}  // namespace android
}  // namespace base
