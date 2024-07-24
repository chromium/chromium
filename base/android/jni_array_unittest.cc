// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android {

TEST(JniArray, GetLength) {
  const auto bytes = std::to_array<uint8_t>({0, 1, 2, 3});
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes = ToJavaByteArray(env, bytes);
  ASSERT_TRUE(j_bytes);
  ASSERT_EQ(4U, SafeGetArrayLength(env, j_bytes));

  ScopedJavaLocalRef<jbyteArray> j_empty_bytes =
      ToJavaByteArray(env, base::span<uint8_t>());
  ASSERT_TRUE(j_empty_bytes);
  ASSERT_EQ(0U, SafeGetArrayLength(env, j_empty_bytes));
}

TEST(JniArray, BasicConversions) {
  const auto bytes = std::to_array<uint8_t>({0, 1, 2, 3});
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> bytes_from_span = ToJavaByteArray(env, bytes);
  ASSERT_TRUE(bytes_from_span);
  ASSERT_EQ(4U, SafeGetArrayLength(env, bytes_from_span));

  auto input_string = std::string(base::as_string_view(bytes));
  ScopedJavaLocalRef<jbyteArray> bytes_from_string =
      ToJavaByteArray(env, input_string);
  ASSERT_TRUE(bytes_from_string);
  ASSERT_EQ(4U, SafeGetArrayLength(env, bytes_from_string));

  ScopedJavaLocalRef<jbyteArray> bytes_from_ptr =
      UNSAFE_BUFFERS(ToJavaByteArray(env, bytes.data(), bytes.size()));
  ASSERT_TRUE(bytes_from_ptr);
  ASSERT_EQ(4U, SafeGetArrayLength(env, bytes_from_ptr));

  std::vector<uint8_t> vector_from_span(5);
  std::vector<uint8_t> vector_from_string(5);
  std::vector<uint8_t> vector_from_ptr(5);
  JavaByteArrayToByteVector(env, bytes_from_span, &vector_from_span);
  JavaByteArrayToByteVector(env, bytes_from_string, &vector_from_string);
  JavaByteArrayToByteVector(env, bytes_from_ptr, &vector_from_ptr);
  EXPECT_EQ(4U, vector_from_span.size());
  EXPECT_EQ(4U, vector_from_string.size());
  EXPECT_EQ(4U, vector_from_ptr.size());
  EXPECT_EQ(bytes, span(vector_from_span));
  EXPECT_EQ(bytes, span(vector_from_string));
  EXPECT_EQ(bytes, span(vector_from_ptr));
}

TEST(JniArray, ByteArrayStringConversions) {
  JNIEnv* env = AttachCurrentThread();
  std::string input_string("hello\0world", 11u);
  ScopedJavaLocalRef<jbyteArray> bytes_from_string =
      ToJavaByteArray(env, input_string);
  ASSERT_TRUE(bytes_from_string);

  std::string string_from_string;
  JavaByteArrayToString(env, bytes_from_string, &string_from_string);
  EXPECT_EQ(input_string, string_from_string);
}

void CheckBoolConversion(JNIEnv* env,
                         span<const bool> bool_array,
                         const ScopedJavaLocalRef<jbooleanArray>& booleans) {
  ASSERT_TRUE(booleans);

  jsize java_array_len = env->GetArrayLength(booleans.obj());
  ASSERT_EQ(checked_cast<jsize>(bool_array.size()), java_array_len);

  jboolean value;
  for (size_t i = 0; i < bool_array.size(); ++i) {
    env->GetBooleanArrayRegion(booleans.obj(), checked_cast<jsize>(i), jsize{1},
                               &value);
    ASSERT_EQ(bool_array[i], value);
  }
}

TEST(JniArray, BoolConversions) {
  const bool kBools[] = {false, true, false};

  JNIEnv* env = AttachCurrentThread();
  CheckBoolConversion(env, kBools, ToJavaBooleanArray(env, kBools));
}

TEST(JniArray, BoolVectorConversions) {
  const auto kBools = std::to_array<bool>({false, true, false});
  const auto kBoolVector = std::vector<bool>({false, true, false});

  JNIEnv* env = AttachCurrentThread();
  CheckBoolConversion(env, kBools, ToJavaBooleanArray(env, kBoolVector));
}

void CheckIntConversion(JNIEnv* env,
                        span<const int> in,
                        const ScopedJavaLocalRef<jintArray>& ints) {
  ASSERT_TRUE(ints);

  jsize java_array_len = env->GetArrayLength(ints.obj());
  ASSERT_EQ(checked_cast<jsize>(in.size()), java_array_len);

  jint value;
  for (size_t i = 0; i < in.size(); ++i) {
    env->GetIntArrayRegion(ints.obj(), i, 1, &value);
    ASSERT_EQ(in[i], value);
  }
}

TEST(JniArray, IntConversions) {
  const int kInts[] = {0, 1, -1, std::numeric_limits<int32_t>::min(),
                       std::numeric_limits<int32_t>::max()};

  JNIEnv* env = AttachCurrentThread();
  CheckIntConversion(env, kInts, ToJavaIntArray(env, kInts));
}

void CheckLongConversion(JNIEnv* env,
                         span<const int64_t> in,
                         const ScopedJavaLocalRef<jlongArray>& longs) {
  ASSERT_TRUE(longs);

  jsize java_array_len = env->GetArrayLength(longs.obj());
  ASSERT_EQ(checked_cast<jsize>(in.size()), java_array_len);

  jlong value;
  for (size_t i = 0; i < in.size(); ++i) {
    env->GetLongArrayRegion(longs.obj(), i, 1, &value);
    ASSERT_EQ(in[i], value);
  }
}

TEST(JniArray, LongConversions) {
  const int64_t kLongs[] = {0, 1, -1, std::numeric_limits<int64_t>::min(),
                            std::numeric_limits<int64_t>::max()};

  JNIEnv* env = AttachCurrentThread();
  CheckLongConversion(env, kLongs, ToJavaLongArray(env, kLongs));
}

void CheckFloatConversion(JNIEnv* env,
                          span<const float> in,
                          const ScopedJavaLocalRef<jfloatArray>& floats) {
  ASSERT_TRUE(floats);

  jsize java_array_len = env->GetArrayLength(floats.obj());
  ASSERT_EQ(checked_cast<jsize>(in.size()), java_array_len);

  jfloat value;
  for (size_t i = 0; i < in.size(); ++i) {
    env->GetFloatArrayRegion(floats.obj(), i, 1, &value);
    ASSERT_EQ(in[i], value);
  }
}

TEST(JniArray, FloatConversions) {
  const float kFloats[] = {0.0f, 1.0f, -10.0f};

  JNIEnv* env = AttachCurrentThread();
  CheckFloatConversion(env, kFloats, ToJavaFloatArray(env, kFloats));
}

void CheckDoubleConversion(JNIEnv* env,
                           span<const double> in,
                           const ScopedJavaLocalRef<jdoubleArray>& doubles) {
  ASSERT_TRUE(doubles);

  jsize java_array_len = env->GetArrayLength(doubles.obj());
  ASSERT_EQ(checked_cast<jsize>(in.size()), java_array_len);

  jdouble value;
  for (size_t i = 0; i < in.size(); ++i) {
    env->GetDoubleArrayRegion(doubles.obj(), i, 1, &value);
    ASSERT_EQ(in[i], value);
  }
}

TEST(JniArray, DoubleConversions) {
  const double kDoubles[] = {0.0, 1.0, -10.0};

  JNIEnv* env = AttachCurrentThread();
  CheckDoubleConversion(env, kDoubles, ToJavaDoubleArray(env, kDoubles));
}

TEST(JniArray, ArrayOfStringArrayConversionUTF8) {
  std::vector<std::vector<std::string>> kArrays = {
      {"a", "f"}, {"a", ""}, {}, {""}, {"今日は"}};

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> joa = ToJavaArrayOfStringArray(env, kArrays);

  std::vector<std::vector<std::string>> out;
  Java2dStringArrayTo2dStringVector(env, joa, &out);
  ASSERT_TRUE(kArrays == out);
}

TEST(JniArray, ArrayOfStringArrayConversionUTF16) {
  std::vector<std::vector<std::u16string>> kArrays = {
      {u"a", u"f"}, {u"a", u""}, {}, {u""}};

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> joa = ToJavaArrayOfStringArray(env, kArrays);

  std::vector<std::vector<std::u16string>> out;
  Java2dStringArrayTo2dStringVector(env, joa, &out);
  ASSERT_TRUE(kArrays == out);
}

void CheckBoolArrayConversion(JNIEnv* env,
                              ScopedJavaLocalRef<jbooleanArray> jbooleans,
                              std::vector<bool> bool_vector) {
  jboolean value;
  for (size_t i = 0; i < bool_vector.size(); ++i) {
    env->GetBooleanArrayRegion(jbooleans.obj(), i, 1, &value);
    ASSERT_EQ(bool_vector[i], value);
  }
}

TEST(JniArray, JavaBooleanArrayToBoolVector) {
  const auto kBools = std::to_array<bool>({false, true, false});

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbooleanArray> jbooleans(
      env, env->NewBooleanArray(kBools.size()));
  ASSERT_TRUE(jbooleans);

  for (size_t i = 0; i < kBools.size(); ++i) {
    jboolean j = static_cast<jboolean>(kBools[i]);
    env->SetBooleanArrayRegion(jbooleans.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<bool> bools;
  JavaBooleanArrayToBoolVector(env, jbooleans, &bools);

  ASSERT_EQ(checked_cast<jsize>(bools.size()),
            env->GetArrayLength(jbooleans.obj()));
  ASSERT_EQ(bools.size(), kBools.size());

  CheckBoolArrayConversion(env, jbooleans, bools);
}

void CheckIntArrayConversion(JNIEnv* env,
                             ScopedJavaLocalRef<jintArray> jints,
                             std::vector<int> int_vector) {
  jint value;
  for (size_t i = 0; i < int_vector.size(); ++i) {
    env->GetIntArrayRegion(jints.obj(), i, 1, &value);
    ASSERT_EQ(int_vector[i], value);
  }
}

TEST(JniArray, JavaIntArrayToIntVector) {
  const auto kInts = std::to_array<int>({0, 1, -1});

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> jints(env, env->NewIntArray(kInts.size()));
  ASSERT_TRUE(jints);

  for (size_t i = 0; i < kInts.size(); ++i) {
    jint j = static_cast<jint>(kInts[i]);
    env->SetIntArrayRegion(jints.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<int> ints;
  JavaIntArrayToIntVector(env, jints, &ints);

  ASSERT_EQ(checked_cast<jsize>(ints.size()), env->GetArrayLength(jints.obj()));
  ASSERT_EQ(ints.size(), kInts.size());

  CheckIntArrayConversion(env, jints, ints);
}

TEST(JniArray, JavaLongArrayToInt64Vector) {
  const auto kInt64s = std::to_array<int64_t>({0LL, 1LL, -1LL});

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jlongs(env, env->NewLongArray(kInt64s.size()));
  ASSERT_TRUE(jlongs);

  for (size_t i = 0; i < kInt64s.size(); ++i) {
    jlong j = static_cast<jlong>(kInt64s[i]);
    env->SetLongArrayRegion(jlongs.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<int64_t> int64s;
  JavaLongArrayToInt64Vector(env, jlongs, &int64s);

  ASSERT_EQ(checked_cast<jsize>(int64s.size()),
            env->GetArrayLength(jlongs.obj()));
  ASSERT_EQ(int64s.size(), kInt64s.size());

  jlong value;
  for (size_t i = 0; i < kInt64s.size(); ++i) {
    env->GetLongArrayRegion(jlongs.obj(), i, 1, &value);
    ASSERT_EQ(int64s[i], value);
    ASSERT_EQ(kInt64s[i], int64s[i]);
  }
}

TEST(JniArray, JavaLongArrayToLongVector) {
  const auto kInt64s = std::to_array<int64_t>({0LL, 1LL, -1LL});

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jlongArray> jlongs(env, env->NewLongArray(kInt64s.size()));
  ASSERT_TRUE(jlongs);

  for (size_t i = 0; i < kInt64s.size(); ++i) {
    jlong j = static_cast<jlong>(kInt64s[i]);
    env->SetLongArrayRegion(jlongs.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<jlong> jlongs_vector;
  JavaLongArrayToLongVector(env, jlongs, &jlongs_vector);

  ASSERT_EQ(checked_cast<jsize>(jlongs_vector.size()),
            env->GetArrayLength(jlongs.obj()));
  ASSERT_EQ(jlongs_vector.size(), kInt64s.size());

  jlong value;
  for (size_t i = 0; i < kInt64s.size(); ++i) {
    env->GetLongArrayRegion(jlongs.obj(), i, 1, &value);
    ASSERT_EQ(jlongs_vector[i], value);
  }
}

TEST(JniArray, JavaFloatArrayToFloatVector) {
  const auto kFloats = std::to_array<float>({0.0, 0.5, -0.5});

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jfloatArray> jfloats(env,
                                          env->NewFloatArray(kFloats.size()));
  ASSERT_TRUE(jfloats);

  for (size_t i = 0; i < kFloats.size(); ++i) {
    jfloat j = static_cast<jfloat>(kFloats[i]);
    env->SetFloatArrayRegion(jfloats.obj(), i, 1, &j);
    ASSERT_FALSE(HasException(env));
  }

  std::vector<float> floats;
  JavaFloatArrayToFloatVector(env, jfloats, &floats);

  ASSERT_EQ(checked_cast<jsize>(floats.size()),
            env->GetArrayLength(jfloats.obj()));
  ASSERT_EQ(floats.size(), kFloats.size());

  jfloat value;
  for (size_t i = 0; i < kFloats.size(); ++i) {
    env->GetFloatArrayRegion(jfloats.obj(), i, 1, &value);
    ASSERT_EQ(floats[i], value);
  }
}

TEST(JniArray, JavaDoubleArrayToDoubleVector) {
  const auto kDoubles = std::to_array<double>(
      {0.0, 0.5, -0.5, std::numeric_limits<double>::min()});
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jdoubleArray> jdoubles(
      env, env->NewDoubleArray(kDoubles.size()));
  ASSERT_TRUE(jdoubles);

  env->SetDoubleArrayRegion(jdoubles.obj(), 0, kDoubles.size(),
                            reinterpret_cast<const jdouble*>(kDoubles.data()));
  ASSERT_FALSE(HasException(env));

  std::vector<double> doubles;
  JavaDoubleArrayToDoubleVector(env, jdoubles, &doubles);
  ASSERT_EQ(kDoubles, base::span(doubles));
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
    snprintf(text, std::ranges::size(text), "%d", i);
    ScopedJavaLocalRef<jbyteArray> byte_array =
        ToJavaByteArray(env, base::as_byte_span(text));
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

TEST(JniArray, JavaArrayOfByteArrayToBytesVector) {
  const size_t kMaxItems = 50;
  const uint8_t kStep = 37;
  JNIEnv* env = AttachCurrentThread();

  // Create a byte[][] object.
  ScopedJavaLocalRef<jclass> byte_array_clazz(env, env->FindClass("[B"));
  ASSERT_TRUE(byte_array_clazz);

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kMaxItems, byte_array_clazz.obj(), nullptr));
  ASSERT_TRUE(array);

  // Create kMaxItems byte buffers with size |i|+1 on each step;
  std::vector<std::vector<uint8_t>> input_bytes;
  input_bytes.reserve(kMaxItems);
  for (size_t i = 0; i < kMaxItems; ++i) {
    std::vector<uint8_t> cur_bytes(i + 1);
    for (size_t j = 0; j < cur_bytes.size(); ++j)
      cur_bytes[j] = static_cast<uint8_t>(i + j * kStep);
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env, cur_bytes);
    ASSERT_TRUE(byte_array);

    env->SetObjectArrayElement(array.obj(), i, byte_array.obj());
    ASSERT_FALSE(HasException(env));

    input_bytes.push_back(std::move(cur_bytes));
  }
  ASSERT_EQ(kMaxItems, input_bytes.size());

  // Convert to std::vector<std::vector<uint8_t>>, check the content.
  std::vector<std::vector<uint8_t>> result;
  JavaArrayOfByteArrayToBytesVector(env, array, &result);

  EXPECT_EQ(input_bytes.size(), result.size());
  for (size_t i = 0; i < kMaxItems; ++i)
    EXPECT_THAT(result[i], ::testing::ElementsAreArray(input_bytes.at(i)));
}

TEST(JniArray, JavaArrayOfStringArrayToVectorOfStringVector) {
  const std::vector<std::vector<std::u16string>> kArrays = {
      {u"a", u"f"}, {u"a", u""}, {}, {u""}};

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kArrays.size(),
                               env->FindClass("[Ljava/lang/String;"), NULL));
  ASSERT_TRUE(array);

  ScopedJavaLocalRef<jclass> string_clazz(env,
                                          env->FindClass("java/lang/String"));
  ASSERT_TRUE(string_clazz);

  for (size_t i = 0; i < kArrays.size(); ++i) {
    const std::vector<std::u16string>& child_data = kArrays[i];

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

  std::vector<std::vector<std::u16string>> vec;
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
  const auto kInts0 =
      std::to_array<int>({0, 1, -1, std::numeric_limits<int32_t>::min(),
                          std::numeric_limits<int32_t>::max()});
  ScopedJavaLocalRef<jintArray> int_array0 = ToJavaIntArray(env, kInts0);
  env->SetObjectArrayElement(array.obj(), 0, int_array0.obj());

  const auto kInts1 = std::to_array<int>({3, 4, 5});
  ScopedJavaLocalRef<jintArray> int_array1 = ToJavaIntArray(env, kInts1);
  env->SetObjectArrayElement(array.obj(), 1, int_array1.obj());

  const auto kInts2 = std::array<int, 0>();
  ScopedJavaLocalRef<jintArray> int_array2 = ToJavaIntArray(env, kInts2);
  env->SetObjectArrayElement(array.obj(), 2, int_array2.obj());

  const auto kInts3 = std::to_array<int>({16});
  ScopedJavaLocalRef<jintArray> int_array3 = ToJavaIntArray(env, kInts3);
  env->SetObjectArrayElement(array.obj(), 3, int_array3.obj());

  // Convert to std::vector<std::vector<int>>, check the content.
  std::vector<std::vector<int>> out;
  JavaArrayOfIntArrayToIntVector(env, array, &out);

  EXPECT_EQ(kNumItems, out.size());
  EXPECT_EQ(kInts0.size(), out[0].size());
  EXPECT_EQ(kInts1.size(), out[1].size());
  EXPECT_EQ(kInts2.size(), out[2].size());
  EXPECT_EQ(kInts3.size(), out[3].size());
  CheckIntArrayConversion(env, int_array0, out[0]);
  CheckIntArrayConversion(env, int_array1, out[1]);
  CheckIntArrayConversion(env, int_array2, out[2]);
  CheckIntArrayConversion(env, int_array3, out[3]);
}

TEST(JniArray, ToJavaArrayOfObjectsOfClass) {
  JNIEnv* env = AttachCurrentThread();

  std::vector<ScopedJavaLocalRef<jobject>> objects = {
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "one")),
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "two")),
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "three")),
  };

  ScopedJavaLocalRef<jobjectArray> j_array =
      ToJavaArrayOfObjects(env, jni_zero::g_string_class, objects);
  ASSERT_TRUE(j_array);

  EXPECT_EQ("one",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 0)))));
  EXPECT_EQ("two",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 1)))));
  EXPECT_EQ("three",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 2)))));
}

TEST(JniArray, ToJavaArrayOfObjectLocalRef) {
  JNIEnv* env = AttachCurrentThread();

  std::vector<ScopedJavaLocalRef<jobject>> objects = {
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "one")),
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "two")),
      ScopedJavaLocalRef<jobject>(ConvertUTF8ToJavaString(env, "three")),
  };

  ScopedJavaLocalRef<jobjectArray> j_array = ToJavaArrayOfObjects(env, objects);
  ASSERT_TRUE(j_array);

  EXPECT_EQ("one",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 0)))));
  EXPECT_EQ("two",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 1)))));
  EXPECT_EQ("three",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 2)))));
}

TEST(JniArray, ToJavaArrayOfObjectGlobalRef) {
  JNIEnv* env = AttachCurrentThread();

  std::vector<ScopedJavaGlobalRef<jobject>> objects = {
      ScopedJavaGlobalRef<jobject>(ConvertUTF8ToJavaString(env, "one")),
      ScopedJavaGlobalRef<jobject>(ConvertUTF8ToJavaString(env, "two")),
      ScopedJavaGlobalRef<jobject>(ConvertUTF8ToJavaString(env, "three")),
  };

  ScopedJavaLocalRef<jobjectArray> j_array = ToJavaArrayOfObjects(env, objects);
  ASSERT_TRUE(j_array);

  EXPECT_EQ("one",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 0)))));
  EXPECT_EQ("two",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 1)))));
  EXPECT_EQ("three",
            ConvertJavaStringToUTF8(
                env, ScopedJavaLocalRef<jstring>(
                         env, static_cast<jstring>(env->GetObjectArrayElement(
                                  j_array.obj(), 2)))));
}
}  // namespace base::android
