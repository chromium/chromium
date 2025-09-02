// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/themes/ntp_background_service_observer.h"

class NtpBackgroundService;

using base::android::JavaParamRef;

// The C++ counterpart of `NtpThemeBridge.java`. It is responsible for dealing
// with theme collections for the NTP.
class NtpThemeBridge : public NtpBackgroundServiceObserver {
 public:
  explicit NtpThemeBridge(Profile* profile);

  NtpThemeBridge(const NtpThemeBridge&) = delete;
  NtpThemeBridge& operator=(const NtpThemeBridge&) = delete;

  // Called by the Java counterpart to destroy this object.
  void Destroy(JNIEnv* env);

  // Fetches the list of background collections. The `j_callback` will be
  // invoked with the list of `BackgroundCollection` objects.
  void GetBackgroundCollections(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_callback);

  // Fetches the list of images for a given collection. The `j_callback` will be
  // invoked with the list of `CollectionImage` objects.
  void GetBackgroundImages(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_collection_id,
      const base::android::JavaParamRef<jobject>& j_callback);

 private:
  virtual ~NtpThemeBridge();

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  raw_ptr<Profile> profile_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  base::android::ScopedJavaGlobalRef<jobject>
      j_background_collections_callback_;
  base::android::ScopedJavaGlobalRef<jobject> j_background_images_callback_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_BRIDGE_H_
