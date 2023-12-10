// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_FAVICON_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_FAVICON_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class FaviconFetcher {
 public:
  explicit FaviconFetcher(
      raw_ptr<favicon::LargeIconService> large_icon_service);

  FaviconFetcher() = delete;

  FaviconFetcher(const FaviconFetcher&) = delete;

  FaviconFetcher& operator=(const FaviconFetcher&) = delete;

  virtual ~FaviconFetcher();

  // Initiates a request to fetch a favicon for a specific url.
  // Wraps the calls to a service that obtains the favicon and returns
  // through the provided callback. The FaviconFetcher will delete
  // itself upon callback completion.
  void FetchFavicon(
      const GURL& url,
      bool continue_to_server,
      int min_source_side_size_in_pixel,
      int desired_side_size_in_pixel,
      const base::android::ScopedJavaGlobalRef<jobject>& callback);

  base::WeakPtr<FaviconFetcher> GetWeakPtr();

 private:
  // Wrapper for favicon specs.
  struct FaviconDimensions {
    int min_source_size_in_pixel;
    // Set to zero to return icon without rescaling.
    int desired_size_in_pixel;
  };

  // Provides access to the status of a request for fetching a favicon.
  void OnFaviconDownloaded(
      const GURL& url,
      const base::android::ScopedJavaGlobalRef<jobject>& callback,
      FaviconDimensions faviconDimensions,
      favicon_base::GoogleFaviconServerRequestStatus status);

  // Returns the downloaded favicon through the provided callback.
  // If the favicon is not present in cache, requests it from google servers.
  void OnGetFaviconFromCacheFinished(
      const GURL& url,
      bool continue_to_server,
      const base::android::ScopedJavaGlobalRef<jobject>& callback,
      FaviconDimensions faviconDimensions,
      const favicon_base::LargeIconImageResult& image_result);

  // Helper method for returning the favicon to the caller.
  virtual void ExecuteFaviconCallback(
      const base::android::ScopedJavaGlobalRef<jobject>& callback,
      SkBitmap bitmap);

  // Helper destructor wrapper.
  virtual void Destroy();

  // Required for execution of icon fetching.
  raw_ptr<favicon::LargeIconService> large_icon_service_;
  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<FaviconFetcher> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_FAVICON_FETCHER_H_
