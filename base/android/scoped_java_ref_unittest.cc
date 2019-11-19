// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"

#include <iterator>
#include <type_traits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_SAME_OBJECT(a, b) \
  EXPECT_TRUE(env->IsSameObject((a).obj(), (b).obj()))

namespace base {
namespace android {

namespace {
int g_local_refs = 0;
int g_global_refs = 0;

const JNINativeInterface* g_previous_functions;

jobject NewGlobalRef(JNIEnv* env, jobject obj) {
  ++g_global_refs;
  return g_previous_functions->NewGlobalRef(env, obj);
}

void DeleteGlobalRef(JNIEnv* env, jobject obj) {
  --g_global_refs;
  return g_previous_functions->DeleteGlobalRef(env, obj);
}

jobject NewLocalRef(JNIEnv* env, jobject obj) {
  ++g_local_refs;
  return g_previous_functions->NewLocalRef(env, obj);
}

void DeleteLocalRef(JNIEnv* env, jobject obj) {
  --g_local_refs;
  return g_previous_functions->DeleteLocalRef(env, obj);
}
}  // namespace

class ScopedJavaRefTest : public testing::Test {
 protected:
  void SetUp() override {
    g_local_refs = 0;
    g_global_refs = 0;
    JNIEnv* env = AttachCurrentThread();
    g_previous_functions = env->functions;
    hooked_functions = *g_previous_functions;
    env->functions = &hooked_functions;
    // We inject our own functions in JNINativeInterface so we can keep track
    // of the reference counting ourselves.
    hooked_functions.NewGlobalRef = &NewGlobalRef;
    hooked_functions.DeleteGlobalRef = &DeleteGlobalRef;
    hooked_functions.NewLocalRef = &NewLocalRef;
    hooked_functions.DeleteLocalRef = &DeleteLocalRef;
  }

  void TearDown() override {
    JNIEnv* env = AttachCurrentThread();
    env->functions = g_previous_functions;
  }
  // From JellyBean release, the instance of this struct provided in JNIEnv is
  // read-only, so we deep copy it to allow individual functions to be hooked.
  JNINativeInterface hooked_functions;
};

// The main purpose of this is testing the various conversions compile.
TEST_F(ScopedJavaRefTest, Conversions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(env, "string");
  ScopedJavaGlobalRef<jstring> global(str);

  // Contextual conversions to bool should be allowed.
  EXPECT_TRUE(str);
  EXPECT_FALSE(JavaRef<jobject>());

  // All the types should convert from nullptr, even JavaRef.
  {
    JavaRef<jstring> null_ref(nullptr);
    EXPECT_FALSE(null_ref);
    ScopedJavaLocalRef<jobject> null_local(nullptr);
    EXPECT_FALSE(null_local);
    ScopedJavaGlobalRef<jarray> null_global(nullptr);
    EXPECT_FALSE(null_global);
  }

  // Local and global refs should {copy,move}-{construct,assign}.
  // Moves should leave the source as null.
  {
    ScopedJavaLocalRef<jstring> str2(str);
    EXPECT_SAME_OBJECT(str2, str);
    ScopedJavaLocalRef<jstring> str3(std::move(str2));
    EXPECT_SAME_OBJECT(str3, str);
    EXPECT_FALSE(str2);
    ScopedJavaLocalRef<jstring> str4;
    str4 = str;
    EXPECT_SAME_OBJECT(str4, str);
    ScopedJavaLocalRef<jstring> str5;
    str5 = std::move(str4);
    EXPECT_SAME_OBJECT(str5, str);
    EXPECT_FALSE(str4);
  }
  {
    ScopedJavaGlobalRef<jstring> str2(global);
    EXPECT_SAME_OBJECT(str2, str);
    ScopedJavaGlobalRef<jstring> str3(std::move(str2));
    EXPECT_SAME_OBJECT(str3, str);
    EXPECT_FALSE(str2);
    ScopedJavaGlobalRef<jstring> str4;
    str4 = global;
    EXPECT_SAME_OBJECT(str4, str);
    ScopedJavaGlobalRef<jstring> str5;
    str5 = std::move(str4);
    EXPECT_SAME_OBJECT(str5, str);
    EXPECT_FALSE(str4);
  }

  // As above but going from jstring to jobject.
  {
    ScopedJavaLocalRef<jobject> obj2(str);
    EXPECT_SAME_OBJECT(obj2, str);
    ScopedJavaLocalRef<jobject> obj3(std::move(obj2));
    EXPECT_SAME_OBJECT(obj3, str);
    EXPECT_FALSE(obj2);
    ScopedJavaLocalRef<jobject> obj4;
    obj4 = str;
    EXPECT_SAME_OBJECT(obj4, str);
    ScopedJavaLocalRef<jobject> obj5;
    obj5 = std::move(obj4);
    EXPECT_SAME_OBJECT(obj5, str);
    EXPECT_FALSE(obj4);
  }
  {
    ScopedJavaGlobalRef<jobject> obj2(global);
    EXPECT_SAME_OBJECT(obj2, str);
    ScopedJavaGlobalRef<jobject> obj3(std::move(obj2));
    EXPECT_SAME_OBJECT(obj3, str);
    EXPECT_FALSE(obj2);
    ScopedJavaGlobalRef<jobject> obj4;
    obj4 = global;
    EXPECT_SAME_OBJECT(obj4, str);
    ScopedJavaGlobalRef<jobject> obj5;
    obj5 = std::move(obj4);
    EXPECT_SAME_OBJECT(obj5, str);
    EXPECT_FALSE(obj4);
  }

  // Explicit copy construction or assignment between global<->local is allowed,
  // but not implicit conversions.
  {
    ScopedJavaLocalRef<jstring> new_local(global);
    EXPECT_SAME_OBJECT(new_local, str);
    new_local = global;
    EXPECT_SAME_OBJECT(new_local, str);
    ScopedJavaGlobalRef<jstring> new_global(str);
    EXPECT_SAME_OBJECT(new_global, str);
    new_global = str;
    EXPECT_SAME_OBJECT(new_local, str);
    static_assert(!std::is_convertible<ScopedJavaLocalRef<jobject>,
                                       ScopedJavaGlobalRef<jobject>>::value,
                  "");
    static_assert(!std::is_convertible<ScopedJavaGlobalRef<jobject>,
                                       ScopedJavaLocalRef<jobject>>::value,
                  "");
  }

  // Converting between local/global while also converting to jobject also works
  // because JavaRef<jobject> is the base class.
  {
    ScopedJavaGlobalRef<jobject> global_obj(str);
    ScopedJavaLocalRef<jobject> local_obj(global);
    const JavaRef<jobject>& obj_ref1(str);
    const JavaRef<jobject>& obj_ref2(global);
    EXPECT_SAME_OBJECT(obj_ref1, obj_ref2);
    EXPECT_SAME_OBJECT(global_obj, obj_ref2);
  }
  global.Reset(str);
  const JavaRef<jstring>& str_ref = str;
  EXPECT_EQ("string", ConvertJavaStringToUTF8(str_ref));
  str.Reset();
}

TEST_F(ScopedJavaRefTest, RefCounts) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> str;
  // The ConvertJavaStringToUTF8 below creates a new string that would normally
  // return a local ref. We simulate that by starting the g_local_refs count at
  // 1.
  g_local_refs = 1;
  str.Reset(ConvertUTF8ToJavaString(env, "string"));
  EXPECT_EQ(1, g_local_refs);
  EXPECT_EQ(0, g_global_refs);
  {
    ScopedJavaGlobalRef<jstring> global_str(str);
    ScopedJavaGlobalRef<jobject> global_obj(global_str);
    EXPECT_EQ(1, g_local_refs);
    EXPECT_EQ(2, g_global_refs);

    auto str2 = ScopedJavaLocalRef<jstring>::Adopt(env, str.Release());
    EXPECT_EQ(1, g_local_refs);
    {
      ScopedJavaLocalRef<jstring> str3(str2);
      EXPECT_EQ(2, g_local_refs);
    }
    EXPECT_EQ(1, g_local_refs);
    {
      ScopedJavaLocalRef<jstring> str4((ScopedJavaLocalRef<jstring>(str2)));
      EXPECT_EQ(2, g_local_refs);
    }
    EXPECT_EQ(1, g_local_refs);
    {
      ScopedJavaLocalRef<jstring> str5;
      str5 = ScopedJavaLocalRef<jstring>(str2);
      EXPECT_EQ(2, g_local_refs);
    }
    EXPECT_EQ(1, g_local_refs);
    str2.Reset();
    EXPECT_EQ(0, g_local_refs);
    global_str.Reset();
    EXPECT_EQ(1, g_global_refs);
    ScopedJavaGlobalRef<jobject> global_obj2(global_obj);
    EXPECT_EQ(2, g_global_refs);
  }

  EXPECT_EQ(0, g_local_refs);
  EXPECT_EQ(0, g_global_refs);
}

class JavaObjectArrayReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    JNIEnv* env = AttachCurrentThread();
    int_class_ = GetClass(env, "java/lang/Integer");
    int_constructor_ = MethodID::Get<MethodID::TYPE_INSTANCE>(
        env, int_class_.obj(), "<init>", "(I)V");
    array_ = MakeArray(array_len_);

    // Make array_len_ different Integer objects, keep a reference to each,
    // and add them to the array.
    for (jint i = 0; i < array_len_; ++i) {
      jobject member = env->NewObject(int_class_.obj(), int_constructor_, i);
      ASSERT_NE(member, nullptr);
      array_members_[i] = ScopedJavaLocalRef<jobject>::Adopt(env, member);
      env->SetObjectArrayElement(array_.obj(), i, member);
    }
  }

  // Make an Integer[] with len elements, all initialized to null.
  ScopedJavaLocalRef<jobjectArray> MakeArray(jsize len) {
    JNIEnv* env = AttachCurrentThread();
    jobjectArray array = env->NewObjectArray(len, int_class_.obj(), nullptr);
    EXPECT_NE(array, nullptr);
    return ScopedJavaLocalRef<jobjectArray>::Adopt(env, array);
  }

  static constexpr jsize array_len_ = 10;
  ScopedJavaLocalRef<jclass> int_class_;
  jmethodID int_constructor_;
  ScopedJavaLocalRef<jobject> array_members_[array_len_];
  ScopedJavaLocalRef<jobjectArray> array_;
};

// Must actually define the variable until C++17 :(
constexpr jsize JavaObjectArrayReaderTest::array_len_;

TEST_F(JavaObjectArrayReaderTest, ZeroLengthArray) {
  JavaObjectArrayReader<jobject> zero_length(MakeArray(0));
  EXPECT_TRUE(zero_length.empty());
  EXPECT_EQ(zero_length.size(), 0);
  EXPECT_EQ(zero_length.begin(), zero_length.end());
  for (auto element : zero_length) {
    FAIL() << "Loop body should not execute";
  }
}

// Verify that we satisfy the C++ "InputIterator" named requirements.
TEST_F(JavaObjectArrayReaderTest, InputIteratorRequirements) {
  typedef JavaObjectArrayReader<jobject>::iterator It;

  JNIEnv* env = AttachCurrentThread();
  JavaObjectArrayReader<jobject> reader(array_);
  It i = reader.begin();

  EXPECT_TRUE(std::is_copy_constructible<It>::value);
  It copy = i;
  EXPECT_EQ(copy, i);
  EXPECT_EQ(It(i), i);

  EXPECT_TRUE(std::is_copy_assignable<It>::value);
  It assign = reader.end();
  It& assign2 = (assign = i);
  EXPECT_EQ(assign, i);
  EXPECT_EQ(assign2, assign);

  EXPECT_TRUE(std::is_destructible<It>::value);

  // Swappable
  It left = reader.begin(), right = reader.end();
  std::swap(left, right);
  EXPECT_EQ(left, reader.end());
  EXPECT_EQ(right, reader.begin());

  // Basic check that iterator_traits works
  bool same_type = std::is_same<std::iterator_traits<It>::iterator_category,
                                std::input_iterator_tag>::value;
  EXPECT_TRUE(same_type);

  // Comparisons
  EXPECT_EQ(reader.begin(), reader.begin());
  EXPECT_NE(reader.begin(), reader.end());

  // Dereferencing
  ScopedJavaLocalRef<jobject> o = *(reader.begin());
  EXPECT_SAME_OBJECT(o, array_members_[0]);
  EXPECT_TRUE(env->IsSameObject(o.obj(), reader.begin()->obj()));

  // Incrementing
  It preinc = ++(reader.begin());
  EXPECT_SAME_OBJECT(*preinc, array_members_[1]);
  It postinc = reader.begin();
  EXPECT_SAME_OBJECT(*postinc++, array_members_[0]);
  EXPECT_SAME_OBJECT(*postinc, array_members_[1]);
}

// Check that range-based for and the convenience function work as expected.
TEST_F(JavaObjectArrayReaderTest, RangeBasedFor) {
  JNIEnv* env = AttachCurrentThread();

  int i = 0;
  for (ScopedJavaLocalRef<jobject> element : array_.ReadElements<jobject>()) {
    EXPECT_SAME_OBJECT(element, array_members_[i++]);
  }
  EXPECT_EQ(i, array_len_);
}

}  // namespace android
}  // namespace base
