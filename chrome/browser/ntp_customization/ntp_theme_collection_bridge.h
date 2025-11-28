// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "components/themes/ntp_background_service_observer.h"

class NtpBackgroundService;
class NtpCustomBackgroundService;

using base::android::JavaParamRef;

// The C++ counterpart to NtpThemeCollectionBridge.java. This class serves as a
// bridge to the NTP theme services, handling theme collections and custom
// backgrounds for the New Tab Page. It observes changes from
// NtpBackgroundService and NtpCustomBackgroundService and communicates with the
// Java layer.
class NtpThemeCollectionBridge : public NtpBackgroundServiceObserver,
                                 public NtpCustomBackgroundServiceObserver {
 public:
  // Creates an instance of NtpThemeCollectionBridge.
  NtpThemeCollectionBridge(
      JNIEnv* env,
      Profile* profile,
      const base::android::JavaParamRef<jobject>& j_java_obj);

  NtpThemeCollectionBridge(const NtpThemeCollectionBridge&) = delete;
  NtpThemeCollectionBridge& operator=(const NtpThemeCollectionBridge&) = delete;

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

  // Sets the New Tab Page background to a specific image from a theme
  // collection.
  // @param env The JNI environment.
  // @param j_collection_id The ID of the collection the image belongs to.
  // @param j_image_url The URL of the image to set as the background.
  // @param j_preview_image_url The URL of a smaller preview image.
  // @param j_attribution_line_1 The first line of attribution text.
  // @param j_attribution_line_2 The second line of attribution text.
  // @param j_attribution_url A URL associated with the attribution text.
  void SetThemeCollectionImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_collection_id,
      const base::android::JavaParamRef<jobject>& j_image_url,
      const base::android::JavaParamRef<jobject>& j_preview_image_url,
      const base::android::JavaParamRef<jstring>& j_attribution_line_1,
      const base::android::JavaParamRef<jstring>& j_attribution_line_2,
      const base::android::JavaParamRef<jobject>& j_attribution_url);

  // Sets the New Tab Page background to a theme collection with daily refresh
  // enabled.
  void SetThemeCollectionDailyRefreshed(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_collection_id);

  // Fetches the current custom background information (e.g., URL, collection
  // ID) from the NtpCustomBackgroundService.
  base::android::ScopedJavaLocalRef<jobject> GetCustomBackgroundInfo(
      JNIEnv* env);

  // Sets the New Tab Page background to an image chosen by the user from their
  // local device.
  void SelectLocalBackgroundImage(JNIEnv* env);

  // Resets the New Tab Page background to the default theme.
  void ResetCustomBackground(JNIEnv* env);

 private:
  ~NtpThemeCollectionBridge() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  raw_ptr<Profile> profile_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  base::android::ScopedJavaGlobalRef<jobject>
      j_background_collections_callback_;
  base::android::ScopedJavaGlobalRef<jobject> j_background_images_callback_;
  base::android::ScopedJavaGlobalRef<jobject> j_java_obj_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_
