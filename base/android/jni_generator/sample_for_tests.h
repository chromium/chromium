// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_GENERATOR_SAMPLE_FOR_TESTS_H_
#define BASE_ANDROID_JNI_GENERATOR_SAMPLE_FOR_TESTS_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/jni_android.h"

namespace base {
namespace android {

// This file is used to:
// - document the best practices and guidelines on JNI usage.
// - ensure sample_for_tests_jni.h compiles and the functions declared in it
// as expected.
//
// Methods are called directly from Java. More documentation in
// SampleForTests.java. See BUILD.gn for the build rules necessary for JNI
// to be used in an APK.
//
// For C++ to access Java methods:
// - GN Build must be configured to generate bindings:
//  # Add import at top of file:
//  if (is_android) {
//    import("//build/config/android/rules.gni")  # For generate_jni().
//  }
//  # ...
//  # An example target that will rely on JNI:
//  component("foo") {
//    # ... normal sources, defines, deps.
//    #     For each jni generated .java -> .h header file in jni_headers
//    #     target there will be a single .cc file here that includes it.
//    #
//    # Add a dep for JNI:
//    if (is_android) {
//      deps += [ ":foo_jni" ]
//    }
//  }
//  # ...
//  # Create target for JNI:
//  if (is_android) {
//    generate_jni("jni_headers") {
//      sources = [
//        "java/src/org/chromium/example/jni_generator/SampleForTests.java",
//      ]
//    }
//    android_library("java") {
//      java_files = [
//        "java/src/org/chromium/example/jni_generator/SampleForTests.java",
//        "java/src/org/chromium/example/jni_generator/NonJniFile.java",
//      ]
//    }
//  }
// The build rules above are generally that that's needed when adding new
// JNI methods/files. For a full GN example, see
// base/android/jni_generator/BUILD.gn
//
// For C++ methods to be exposed to Java:
// - The Java class must be part of an android_apk target that depends on
//   a generate_jni_registration target. This generate_jni_registration target
//   automatically generates all necessary registration functions. The
//   generated header file exposes two functions that should be called when a
//   library is first loaded:
//     1) RegisterMainDexNatives()
//       - Registers all methods that are used outside the browser process
//     2) RegisterNonMainDexNatives()
//       - Registers all methods used in the browser process
//
class CPPClass {
 public:
  CPPClass();
  ~CPPClass();

  // Java @CalledByNative methods implicitly available to C++ via the _jni.h
  // file included in the .cc file.

  class InnerClass {
   public:
    jdouble MethodOtherP0(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& caller);
  };

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

  jint Method(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

  void AddStructB(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  const base::android::JavaParamRef<jobject>& structb);

  void IterateAndDoSomethingWithStructB(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  base::android::ScopedJavaLocalRef<jstring> ReturnAString(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

 private:
  std::map<long, std::string> map_;

  DISALLOW_COPY_AND_ASSIGN(CPPClass);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_GENERATOR_SAMPLE_FOR_TESTS_H_
