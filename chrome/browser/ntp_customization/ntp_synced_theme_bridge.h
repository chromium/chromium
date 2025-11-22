// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"

class NtpCustomBackgroundService;

using base::android::JavaParamRef;

// The C++ counterpart to NtpSyncedThemeBridge.java. This class serves as
// a bridge to the NTP theme services, handling theme collections and custom
// backgrounds for the New Tab Page. It observes changes from
// NtpCustomBackgroundService and communicates with the Java layer.
class NtpSyncedThemeBridge : public NtpCustomBackgroundServiceObserver {
 public:
  // Creates an instance of NtpSyncedThemeBridge.
  NtpSyncedThemeBridge(JNIEnv* env,
                       Profile* profile,
                       const base::android::JavaParamRef<jobject>& j_java_obj);

  NtpSyncedThemeBridge(const NtpSyncedThemeBridge&) = delete;
  NtpSyncedThemeBridge& operator=(const NtpSyncedThemeBridge&) = delete;

  // Called by the Java counterpart to destroy this object.
  void Destroy(JNIEnv* env);

  // Fetches the current custom background information (e.g., URL, collection
  // ID) from the NtpCustomBackgroundService.
  base::android::ScopedJavaLocalRef<jobject> GetCustomBackgroundInfo(
      JNIEnv* env);

 private:
  ~NtpSyncedThemeBridge() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  raw_ptr<Profile> profile_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_java_obj_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_
