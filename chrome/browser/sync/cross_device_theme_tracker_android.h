// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_ANDROID_H_
#define CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/ntp_customization/jni_headers/CrossDeviceThemeTracker_shared_jni.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/jni_zero/system_jni/List_shared_jni.h"

namespace themes {

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.ntp_customization.theme_sync.data)
enum class PlatformType {
  kUnknown = 0,
  kAndroid = 1,
  kIos = 2,
  kDesktop = 3,
  kMaxCount = 4,
};

// Android-specific subclass of CrossDeviceThemeTracker.
//
// This class implements the JNI bridge to connect the C++ theme tracking
// infrastructure with the Android Java UI layer.
//
// Architecture & Ownership:
// 1. C++ owns Java: `CrossDeviceThemeTrackerAndroid` (C++) instantiates and
// owns the Java `CrossDeviceThemeTracker` instance via `java_object_` (strong
// global ref).
// 2. Java holds weak ref to C++: The Java object stores the C++ pointer as a
//    `long mNativePtr`.
// 3. Lifecycle:
//    - On creation, C++ creates Java partner.
//    - On destruction, C++ calls `clearNativePtr` on Java to null out the
//      pointer, preventing any further calls from Java to deleted C++.
//    - Java object is left to be garbage collected by Java runtime.
// 4. Data flow:
//    - C++ observes sync changes. On change, it serializes C++ theme protos
//      to byte arrays, calls Java `createNtpBackgroundDataFromProto` to convert
//      them to Java `NtpBackgroundDataBase` objects, and caches the result.
//    - Java UI queries themes via `getThemes()` which JNI-calls back to C++
//      `GetThemes()` to retrieve the cached Java array.
class CrossDeviceThemeTrackerAndroid
    : public CrossDeviceThemeTracker<sync_pb::ThemeAndroidSpecifics>,
      public CrossDeviceThemeTracker<sync_pb::ThemeAndroidSpecifics>::Observer {
 public:
  explicit CrossDeviceThemeTrackerAndroid(
      syncer::DeviceInfoTracker* device_info_tracker);
  ~CrossDeviceThemeTrackerAndroid() override;

  // CrossDeviceThemeTracker::Observer overrides:
  void OnCrossDeviceThemeChanged() override;
  void OnServiceStatusChanged(ServiceStatus status) override;

  // JNI methods (called from Java):
  jni_zero::ScopedJavaLocalRef<JList<JNtpBackgroundDataBase>> GetThemes(
      JNIEnv* env,
      const jni_zero::JavaRef<JContext>& jcontext);
  // Retrieves the theme for the specified `device_guid`. If `device_guid` is
  // non-empty, only returns a theme from that device (or nullptr if none found,
  // preventing Frankensteining). If `device_guid` is empty (""), returns the
  // best candidate theme based on platform scoring.
  jni_zero::ScopedJavaLocalRef<jobject> GetThemeForDeviceGuid(
      JNIEnv* env,
      const jni_zero::JavaRef<JContext>& jcontext,
      const std::string& device_guid);
  ServiceStatus GetServiceStatus(JNIEnv* env);

  // Returns the owned Java object.
  const jni_zero::ScopedJavaGlobalRef<JCrossDeviceThemeTracker>& java_object()
      const {
    return java_object_;
  }

 private:
  // Creates a single Java theme object from DeviceThemeInfo.
  jni_zero::ScopedJavaLocalRef<jobject> CreateJavaTheme(
      JNIEnv* env,
      const jni_zero::JavaRef<JContext>& jcontext,
      const DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics>& theme_info);

  // Converts cached C++ theme info and calls Java to recreate Java theme
  // objects.
  void RecreateJavaThemes(JNIEnv* env,
                          const jni_zero::JavaRef<JContext>& jcontext);

  // Maps device OS/form factor to Java PlatformType.
  PlatformType MapToPlatformType(syncer::DeviceInfo::OsType os_type,
                                 syncer::DeviceInfo::FormFactor form_factor);

  // Strong global reference to the Java counterpart.
  jni_zero::ScopedJavaGlobalRef<JCrossDeviceThemeTracker> java_object_;

  // Cached Java list of NtpBackgroundDataBase objects.
  jni_zero::ScopedJavaGlobalRef<JList<JNtpBackgroundDataBase>>
      cached_java_themes_;
};

}  // namespace themes

#endif  // CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_ANDROID_H_
