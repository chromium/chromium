// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_FAVICON_REQUEST_HANDLER_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_FAVICON_REQUEST_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace favicon {
class LargeIconService;
}

// Retrieves the favicon for given page URL. If the favicon is not in the cache,
// a network request is made to fetch the icon. If the favicon cannot be
// retrieved, then a fallback monogram icon is created.
class SupervisedUserFaviconRequestHandler {
 public:
  // These enum values represent whether the favicon was ready at the time it
  // was requested. These values are logged to UMA. Entries should not be
  // renumbered and numeric values should never be reused. Please keep in sync
  // with "SupervisedUserFaviconAvailability" in
  // src/tools/metrics/histograms/enums.xml.
  enum class FaviconAvailability {
    kAvailable = 0,
    kUnavailable = 1,
    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kUnavailable,
  };

  SupervisedUserFaviconRequestHandler(
      const GURL& url,
      favicon::LargeIconService* large_icon_service);
  SupervisedUserFaviconRequestHandler(
      const SupervisedUserFaviconRequestHandler&) = delete;
  SupervisedUserFaviconRequestHandler& operator=(
      const SupervisedUserFaviconRequestHandler&) = delete;
  ~SupervisedUserFaviconRequestHandler();

  static const char* GetFaviconAvailabilityHistogramForTesting();

  // Starts fetching the URL favicon and calls on_fetched_callback when
  // finished. The favicon can be then obtained by calling GetFaviconOrFallback.
  void StartFaviconFetch(base::OnceClosure on_fetched_callback);

  // Returns the fetched favicon if it is available. If the requestor asks for
  // the favicon before it has been fetched or the favicon has failed to be
  // fetched, then a monogram fallback icon is constructed. This method is not
  // thread-safe, therefore only the creator of this request handler should get
  // the favicon through this method.
  SkBitmap GetFaviconOrFallback();

 private:
  // Attempts to fetch the favicon from the cache. If the favicon is not
  // available in the cache, then a network request is made to fetch the icon.
  void FetchFaviconFromCache();

  // Callbacks for favicon fetches.
  void OnGetFaviconFromCacheFinished(
      const favicon_base::LargeIconResult& result);
  void OnGetFaviconFromGoogleServerFinished(
      favicon_base::GoogleFaviconServerRequestStatus status);

  // The page that the favicon is being fetched for.
  GURL page_url_;

  // Stores the fetched favicon. May be an empty bitmap if the favicon fetch
  // task has not finished or fails.
  SkBitmap favicon_;

  // True if a network request has already been made to fetch the favicon.
  bool network_request_completed_ = false;

  // Callback run when the favicon has been fetched, either from the cache or
  // via a network request.
  base::OnceClosure on_fetched_callback_;

  raw_ptr<favicon::LargeIconService> large_icon_service_ = nullptr;
  base::CancelableTaskTracker favicon_task_tracker_;

  base::WeakPtrFactory<SupervisedUserFaviconRequestHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_FAVICON_REQUEST_HANDLER_H_
