// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

class Profile;
class TabAndroid;

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
  // `prevent_close` whether to prevent closing the UI if it's already open.
  // `profile` associated with this request.
  // `source` for the UI toggle.
  void ToggleUI(JNIEnv* env,
                int64_t browser_window_ptr,
                bool prevent_close,
                Profile* profile,
                int32_t source);

  bool InvokeWithAutoSubmit(JNIEnv* env,
                            TabAndroid* tab,
                            std::string text,
                            int32_t source);

  bool IsPanelShowingForBrowser(JNIEnv* env, int64_t browser_window_ptr);

  bool GetUserEnabledActuationOnWeb(JNIEnv* env);
  void SetUserEnabledActuationOnWeb(JNIEnv* env, bool enabled);

  void OnGlobalShowHide();
  void OnUserEnabledActuationOnWebChanged();

  // Returns the GlicKeyedServiceImpl java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // Not owned.
  raw_ptr<GlicKeyedService> service_;

  // A reference to the Java counterpart of this class. See
  // GlicKeyedServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  base::CallbackListSubscription global_show_hide_subscription_;
  base::CallbackListSubscription web_actuation_pref_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
