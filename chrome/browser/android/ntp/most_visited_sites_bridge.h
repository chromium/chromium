// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace ntp_tiles {
class MostVisitedSites;
}  // namespace ntp_tiles

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSitesBridge {
 public:
  explicit MostVisitedSitesBridge(Profile* profile);

  MostVisitedSitesBridge(const MostVisitedSitesBridge&) = delete;
  MostVisitedSitesBridge& operator=(const MostVisitedSitesBridge&) = delete;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void OnHomepageStateChanged(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_observer,
                   jint num_sites);

  void SetHomepageClient(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jobject>& j_client);

  void AddOrRemoveBlockedUrl(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             const base::android::JavaParamRef<jobject>& j_url,
                             jboolean add_url);
  void RecordPageImpression(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            jint jtiles_count);
  void RecordTileImpression(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            jint jindex,
                            jint jvisual_type,
                            jint jicon_type,
                            jint jtitle_source,
                            jint jsource,
                            const base::android::JavaParamRef<jobject>& jurl);
  void RecordOpenedMostVisitedItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index,
      jint tile_type,
      jint title_source,
      jint source);

 private:
  ~MostVisitedSitesBridge();

  class JavaObserver;
  std::unique_ptr<JavaObserver> java_observer_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
