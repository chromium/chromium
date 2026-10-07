// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_ANDROID_TTC_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_TTC_ANDROID_TTC_KEYED_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/ttc/app/public/error_codes.h"

namespace ttc {

// Java-facing counterpart of a TtcKeyedService. Owns the Java TtcKeyedService
// object and lives as user data on the native service so the two share a
// lifetime. Java calls in to start and end sessions; the native side pushes
// service state changes and per-session view events (audio level, session
// initialized, errors) back out to Java, where the Android session UI observes
// them.
class TtcKeyedServiceAndroid : public base::SupportsUserData::Data {
 public:
  // Returns the bridge for `service`, creating it on first use.
  static TtcKeyedServiceAndroid& Get(TtcKeyedService& service);

  explicit TtcKeyedServiceAndroid(TtcKeyedService& service);
  TtcKeyedServiceAndroid(const TtcKeyedServiceAndroid&) = delete;
  TtcKeyedServiceAndroid& operator=(const TtcKeyedServiceAndroid&) = delete;
  ~TtcKeyedServiceAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Called by JNI.
  jboolean IsEnabled(JNIEnv* env);
  jboolean IsSessionActive(JNIEnv* env);
  void StartSession(JNIEnv* env);
  void EndSession(JNIEnv* env);

  // Called by SessionViewAndroid to surface session events in the Java UI.
  void OnSessionInitialized();
  void OnAudioLevelChanged(float audio_level);
  void OnError(ErrorCode error);

 private:
  void OnServiceStateChanged(ServiceState state);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  const raw_ref<TtcKeyedService> service_;
  base::CallbackListSubscription state_changed_subscription_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_ANDROID_TTC_KEYED_SERVICE_ANDROID_H_
