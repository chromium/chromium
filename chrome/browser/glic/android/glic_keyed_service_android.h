// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

class Profile;

namespace glic {

class GlicKeyedService;

// Helper class responsible for bridging the GlicKeyedService between
// C++ and Java.
class GlicKeyedServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit GlicKeyedServiceAndroid(GlicKeyedService* service);
  ~GlicKeyedServiceAndroid() override;

  GlicKeyedServiceAndroid(const GlicKeyedServiceAndroid&) = delete;
  GlicKeyedServiceAndroid& operator=(const GlicKeyedServiceAndroid&) = delete;

  // JNI bridge to show, summon, or activate the panel, or close it if it is
  // already active.
  // `env` is the JNI environment.
  // `browser_window_ptr` is unpacked to call the cross-platform service.
  // `profile` associated with this request.
  // `source` for the UI toggle.
  void ToggleUI(JNIEnv* env,
                int64_t browser_window_ptr,
                Profile* profile,
                int32_t source);

  // Returns the GlicKeyedServiceImpl java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // Not owned.
  raw_ptr<GlicKeyedService> service_;

  // A reference to the Java counterpart of this class. See
  // GlicKeyedServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
