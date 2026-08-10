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
#include "components/themes/ntp_custom_background_service_observer.h"

class NtpAndroidCustomBackgroundService;

using base::android::JavaRef;

// The C++ counterpart to NtpSyncedThemeBridge.java. This class serves as
// a bridge to the NTP theme services, handling theme collections and custom
// backgrounds for the New Tab Page. It observes changes from
// NtpAndroidCustomBackgroundService and communicates with the Java layer.
class NtpSyncedThemeBridge : public NtpCustomBackgroundServiceObserver {
 public:
  // Creates an instance of NtpSyncedThemeBridge.
  NtpSyncedThemeBridge(JNIEnv* env,
                       Profile* profile,
                       const base::android::JavaRef<jobject>& j_java_obj);

  NtpSyncedThemeBridge(const NtpSyncedThemeBridge&) = delete;
  NtpSyncedThemeBridge& operator=(const NtpSyncedThemeBridge&) = delete;

  // Called by the Java counterpart to destroy this object.
  void Destroy(JNIEnv* env);

  // Fetches the next image for a theme collection with daily refresh enabled.
  void FetchNextThemeCollectionImage(JNIEnv* env);

  // Fetches the current custom background information (e.g., URL, collection
  // ID) from the NtpAndroidCustomBackgroundService.
  base::android::ScopedJavaLocalRef<jobject> GetCustomBackgroundInfo(
      JNIEnv* env);

  // Exposes whether the underlying service is processing a sync update.
  bool IsProcessingSyncUpdate(JNIEnv* env);

  // Disconnects from the custom background service when the service is
  // destroyed.
  void DisconnectCustomBackgroundService();

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

 protected:
  NtpSyncedThemeBridge();
  ~NtpSyncedThemeBridge() override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<NtpAndroidCustomBackgroundService> ntp_custom_background_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_java_obj_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_
