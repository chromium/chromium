// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

class Profile;

namespace ntp_tiles {
class MostVisitedSites;
}  // namespace ntp_tiles

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSitesBridge {
 public:
  MostVisitedSitesBridge(Profile* profile, bool enable_custom_links);

  MostVisitedSitesBridge(const MostVisitedSitesBridge&) = delete;
  MostVisitedSitesBridge& operator=(const MostVisitedSitesBridge&) = delete;

  void Destroy(JNIEnv* env);

  void OnHomepageStateChanged(JNIEnv* env);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaRef<jobject>& j_observer,
                   int32_t num_sites);

  void SetHomepageClient(JNIEnv* env,
                         const base::android::JavaRef<jobject>& j_client);

  bool AddCustomLinkTo(JNIEnv* env,
                       const std::u16string& name,
                       const GURL& url,
                       int32_t pos);

  bool AddCustomLink(JNIEnv* env, const std::u16string& name, const GURL& url);

  bool AssignCustomLink(JNIEnv* env,
                        const GURL& key_url,
                        const std::u16string& j_name,
                        const GURL& url);

  bool DeleteCustomLink(JNIEnv* env, const GURL& key_url);

  bool HasCustomLink(JNIEnv* env, const GURL& key_url);

  bool ReorderCustomLink(JNIEnv* env, const GURL& key_url, int32_t new_pos);

  void AddOrRemoveBlockedUrl(JNIEnv* env,
                             const base::android::JavaRef<jobject>& j_url,
                             bool add_url);
  void RecordPageImpression(JNIEnv* env, int32_t jtiles_count);
  void RecordTileImpression(JNIEnv* env,
                            int32_t jindex,
                            int32_t jvisual_type,
                            int32_t jicon_type,
                            int32_t jtitle_source,
                            int32_t jsource,
                            const base::android::JavaRef<jobject>& jurl);
  void RecordOpenedMostVisitedItem(JNIEnv* env,
                                   int32_t index,
                                   int32_t tile_type,
                                   int32_t title_source,
                                   int32_t source);

  double GetSuggestionScore(JNIEnv* env, const GURL& url);

 private:
  ~MostVisitedSitesBridge();

  class JavaObserver;
  std::unique_ptr<JavaObserver> java_observer_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
