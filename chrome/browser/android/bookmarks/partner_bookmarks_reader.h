// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {
class LargeIconService;
}

class PartnerBookmarksShim;
class Profile;

// Generates a partner bookmark hierarchy and handles submitting the results to
// the global PartnerBookmarksShim.
class PartnerBookmarksReader {
 public:
  PartnerBookmarksReader(PartnerBookmarksShim* partner_bookmarks_shim,
                         Profile* profile);

  PartnerBookmarksReader(const PartnerBookmarksReader&) = delete;
  PartnerBookmarksReader& operator=(const PartnerBookmarksReader&) = delete;

  ~PartnerBookmarksReader();

  // JNI methods
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Reset(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jlong AddPartnerBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle,
      jboolean is_folder,
      jlong parent_id,
      const base::android::JavaParamRef<jbyteArray>& favicon,
      const base::android::JavaParamRef<jbyteArray>& touchicon,
      jboolean fetch_uncached_favicons_from_server,
      jint desired_favicon_size_px,
      // Callback<FaviconFetchResult>
      const base::android::JavaParamRef<jobject>& j_callback);
  void PartnerBookmarksCreationComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  static std::unique_ptr<bookmarks::BookmarkNode>
  CreatePartnerBookmarksRootForTesting();

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.partnerbookmarks
  enum class FaviconFetchResult {
    // Successfully fetched a favicon from cache or server.
    // Deprecated, SUCCESS_FROM_CACHE and SUCCESS_FROM_SERVER should be used.
    DEPRECATED_SUCCESS = 0,
    // Received a server error fetching the favicon.
    FAILURE_SERVER_ERROR,
    // The icon service was unavailable.
    FAILURE_ICON_SERVICE_UNAVAILABLE,
    // There was nothing in the cache, but we opted out of retrieving from
    // server.
    FAILURE_NOT_IN_CACHE,
    // Request sent out and a connection error occurred (no valid HTTP response
    // received).
    FAILURE_CONNECTION_ERROR,
    // Success fetching the favicon from the cache without reaching out to the
    // server.
    SUCCESS_FROM_CACHE,
    // Success fetching the favicon from server.
    SUCCESS_FROM_SERVER,
    // Failed to write the favicon to cache, likely from attempting to add a
    // duplicate.
    FAILURE_WRITING_FAVICON_CACHE,
    // Boundary value for UMA.
    UMA_BOUNDARY,
  };

  using FaviconFetchedCallback = base::OnceCallback<void(FaviconFetchResult)>;

  favicon::LargeIconService* GetLargeIconService();
  void GetFavicon(const GURL& page_url,
                  Profile* profile,
                  bool fallback_to_server,
                  int desired_favicon_size_px,
                  FaviconFetchedCallback callback);
  void GetFaviconImpl(const GURL& page_url,
                      Profile* profile,
                      bool fallback_to_server,
                      int desired_favicon_size_px,
                      FaviconFetchedCallback callback);
  void GetFaviconFromCacheOrServer(const GURL& page_url,
                                   bool fallback_to_server,
                                   bool from_server,
                                   int desired_favicon_size_px,
                                   FaviconFetchedCallback callback);
  void OnGetFaviconFromCacheFinished(
      const GURL& page_url,
      FaviconFetchedCallback callback,
      bool fallback_to_server,
      bool from_server,
      int desired_favicon_size_px,
      const favicon_base::LargeIconResult& result);
  void OnGetFaviconFromServerFinished(
      const GURL& page_url,
      int desired_favicon_size_px,
      FaviconFetchedCallback callback,
      favicon_base::GoogleFaviconServerRequestStatus status);
  void OnFaviconFetched(const base::android::JavaRef<jobject>& j_callback,
                        FaviconFetchResult result);
  // Putting in class in order to set the friend class access for
  // base::ScopedAllowBaseSyncPrimitives.
  static void PrepareAndSetFavicon(jbyte* icon_bytes,
                                   int icon_len,
                                   bookmarks::BookmarkNode* node,
                                   Profile* profile,
                                   favicon_base::IconType icon_type);

  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;
  raw_ptr<Profile> profile_;

  raw_ptr<favicon::LargeIconService> large_icon_service_;
  base::CancelableTaskTracker favicon_task_tracker_;

  // JNI
  std::unique_ptr<bookmarks::BookmarkNode> wip_partner_bookmarks_root_;
  int64_t wip_next_available_id_;
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
