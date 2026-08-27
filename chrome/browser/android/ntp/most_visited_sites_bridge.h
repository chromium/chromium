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

  void Destroy();

  void OnHomepageStateChanged();

  void SetObserver(JNIEnv* env,
                   const base::android::JavaRef<jobject>& j_observer,
                   int32_t num_sites);

  void SetHomepageClient(JNIEnv* env,
                         const base::android::JavaRef<jobject>& j_client);

  bool AddCustomLinkTo(const std::u16string& name,
                       const GURL& url,
                       int32_t pos);

  bool AddCustomLink(const std::u16string& name, const GURL& url);

  bool AssignCustomLink(const GURL& key_url,
                        const std::u16string& j_name,
                        const GURL& url);

  bool DeleteCustomLink(const GURL& key_url);

  bool HasCustomLink(const GURL& key_url);

  bool ReorderCustomLink(const GURL& key_url, int32_t new_pos);

  void AddOrRemoveBlockedUrl(const GURL& url, bool add_url);
  void RecordPageImpression(int32_t jtiles_count);
  void RecordTileImpression(int32_t jindex,
                            int32_t jvisual_type,
                            int32_t jicon_type,
                            int32_t jtitle_source,
                            int32_t jsource,
                            const GURL& url);
  void RecordOpenedMostVisitedItem(int32_t index,
                                   int32_t tile_type,
                                   int32_t title_source,
                                   int32_t source);

  double GetSuggestionScore(const GURL& url);

 private:
  ~MostVisitedSitesBridge();

  class JavaObserver;
  std::unique_ptr<JavaObserver> java_observer_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
