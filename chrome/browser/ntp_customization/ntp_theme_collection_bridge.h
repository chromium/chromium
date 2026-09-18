// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_

#include <jni.h>

#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/themes/ntp_background_service_observer.h"
#include "components/themes/ntp_custom_background_service_observer.h"

class GURL;
class NtpBackgroundService;
class NtpAndroidCustomBackgroundService;

using base::android::JavaRef;

// The C++ counterpart to NtpThemeCollectionBridge.java. This class serves as a
// bridge to the NTP theme services, handling theme collections and custom
// backgrounds for the New Tab Page. It observes changes from
// NtpBackgroundService and NtpAndroidCustomBackgroundService and communicates
// with the Java layer.
class NtpThemeCollectionBridge : public NtpBackgroundServiceObserver,
                                 public NtpCustomBackgroundServiceObserver {
 public:
  // Creates an instance of NtpThemeCollectionBridge.
  NtpThemeCollectionBridge(JNIEnv* env,
                           Profile* profile,
                           const base::android::JavaRef<jobject>& j_java_obj);

  NtpThemeCollectionBridge(const NtpThemeCollectionBridge&) = delete;
  NtpThemeCollectionBridge& operator=(const NtpThemeCollectionBridge&) = delete;

  // Called by the Java counterpart to destroy this object.
  void Destroy(JNIEnv* env);

  // Fetches the list of background collections. The `j_callback` will be
  // invoked with the list of `BackgroundCollection` objects.
  void GetBackgroundCollections(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_callback);

  // Fetches the list of images for a given collection. The `j_callback` will be
  // invoked with the list of `CollectionImage` objects.
  void GetBackgroundImages(const std::string& collection_id,
                           const base::android::JavaRef<jobject>& j_callback);

  // Sets the New Tab Page background to a specific image from a theme
  // collection.
  // @param collection_id The ID of the collection the image belongs to.
  // @param image_url The URL of the image to set as the background.
  // @param preview_image_url The URL of a smaller preview image.
  // @param attribution_line_1 The first line of attribution text.
  // @param attribution_line_2 The second line of attribution text.
  // @param attribution_url A URL associated with the attribution text.
  void SetThemeCollectionImage(const std::string& collection_id,
                               const GURL& image_url,
                               const GURL& preview_image_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& attribution_url);

  // Sets the New Tab Page background to a theme collection with daily refresh
  // enabled.
  void SetThemeCollectionDailyRefreshed(const std::string& collection_id);

  // Fetches the next image for a theme collection with daily refresh enabled.
  void FetchNextThemeCollectionImage(JNIEnv* env);

  // Fetches the current custom background information (e.g., URL, collection
  // ID) from the NtpAndroidCustomBackgroundService.
  base::android::ScopedJavaLocalRef<jobject> GetCustomBackgroundInfo(
      JNIEnv* env);

  // Disconnects from the custom background service when the service is
  // destroyed.
  void DisconnectCustomBackgroundService();

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

 protected:
  NtpThemeCollectionBridge();
  ~NtpThemeCollectionBridge() override;

 private:
  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  raw_ptr<Profile> profile_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  raw_ptr<NtpAndroidCustomBackgroundService> ntp_custom_background_service_;
  base::android::ScopedJavaGlobalRef<jobject>
      j_background_collections_callback_;
  base::android::ScopedJavaGlobalRef<jobject> j_background_images_callback_;
  base::android::ScopedJavaGlobalRef<jobject> j_java_obj_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_THEME_COLLECTION_BRIDGE_H_
