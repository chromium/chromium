// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_jni/EmbeddedComponentLoaderTest_jni.h"
#include "android_webview/test/webview_instrumentation_test_native_jni/EmbeddedComponentLoaderFactory_jni.h"

namespace component_updater {

namespace {
// This hash corresponds to "jebgalgnebhfojomionfpkfelancnnkf".
constexpr uint8_t kAvailableSha256Hash[] = {
    0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
    0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
    0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

// This hash corresponds to "abcdefjhijk".
constexpr uint8_t kUnavailableComponentSha256Hash[] = {
    0x6a, 0xcc, 0xdf, 0xdb, 0x7b, 0xa0, 0xe9, 0x61, 0x14, 0x94, 0x27,
    0x29, 0xe0, 0x11, 0xaa, 0x24, 0xe8, 0x58, 0xe9, 0x9f, 0x78, 0x03,
    0x13, 0x40, 0x95, 0x2e, 0x65, 0xc3, 0x9c, 0x68, 0xa9, 0xcc};

// Check that `condition` is `true` otherwise send an error message to java that
// will trigger a failure at the end of the java test with the given `error`
// message.
static void ExpectTrueToJava(bool condition, const std::string& error) {
  if (!condition) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_EmbeddedComponentLoaderTest_fail(
        env, base::android::ConvertUTF8ToJavaString(env, error));
  }
}

class AvailableComponentLoaderPolicy : public ComponentLoaderPolicy {
 public:
  AvailableComponentLoaderPolicy() = default;
  ~AvailableComponentLoaderPolicy() override = default;

  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override {
    // Make sure these values match the values in the
    // EmbeddedComponentLoaderTest.
    ExpectTrueToJava(version.GetString() == "123.456.789",
                     "version != 123.456.789");
    ExpectTrueToJava(fd_map.size() == 1u, "fd_map.size != 1");
    ExpectTrueToJava(base::Contains(fd_map, "file.test"),
                     "file.test is not found in the fd_map");
    Java_EmbeddedComponentLoaderTest_onComponentLoaded(
        base::android::AttachCurrentThread());
  }

  void ComponentLoadFailed(ComponentLoadResult /*error*/) override {
    ExpectTrueToJava(
        false, "AvailableComponentLoaderPolicy#ComponentLoadFailed is called");
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kAvailableSha256Hash),
                 std::end(kAvailableSha256Hash));
  }

  std::string GetMetricsSuffix() const override { return "AvailableComponent"; }
};

class UnavailableComponentLoaderPolicy : public ComponentLoaderPolicy {
 public:
  UnavailableComponentLoaderPolicy() = default;
  ~UnavailableComponentLoaderPolicy() override = default;

  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override {
    ExpectTrueToJava(
        false, "UnavailableComponentLoaderPolicy#ComponentLoaded is called");
  }

  void ComponentLoadFailed(ComponentLoadResult /*error*/) override {
    Java_EmbeddedComponentLoaderTest_onComponentLoadFailed(
        base::android::AttachCurrentThread());
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kUnavailableComponentSha256Hash),
                 std::end(kUnavailableComponentSha256Hash));
  }

  std::string GetMetricsSuffix() const override {
    return "UnavailableComponent";
  }
};

}  // namespace

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_EmbeddedComponentLoaderFactory_GetComponentLoaderPolicies(JNIEnv* env) {
  ComponentLoaderPolicyVector loaders;
  loaders.push_back(std::make_unique<AvailableComponentLoaderPolicy>());
  loaders.push_back(std::make_unique<UnavailableComponentLoaderPolicy>());
  return AndroidComponentLoaderPolicy::
      ToJavaArrayOfAndroidComponentLoaderPolicy(env, std::move(loaders));
}

}  // namespace component_updater
