// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_

#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"
#include "net/http/http_no_vary_search_data.h"
#include "url/gurl.h"

namespace android_webview {

using AwPrefetchKey = int;

// The default TTL value in `//content` is 10 minutes which is too long for most
// of WebView cases. This value here can change in the future and that shouldn't
// affect the `//content` TTL default value.
inline constexpr int kDefaultTtlInSec = 60;
// The MaxPrefetches number is not present in the `//content` layer, so it is
// specific to WebView.
inline constexpr size_t kDefaultMaxPrefetches = 10;
// This is the source of truth for the absolute maximum number of prefetches
// that can ever be cached in WebView. It can override the number set by the
// AndroidX API.
inline constexpr size_t kAbsoluteMaxPrefetches = 20;
// Returned from `AwPrefetchManager::StartPrefetchRequest` if the prefetch
// request was unsuccessful (i.e. there is no key for the prefetch).
inline constexpr AwPrefetchKey NO_PREFETCH_KEY = -1;

// An `AwPrefetchManager`-specific data store that manages its mutable state,
// which can be modified by API calls.
class AwPrefetchManagerData {
 public:
  AwPrefetchManagerData();
  ~AwPrefetchManagerData();

  // Returns true if the prefetch with URL and NVS hint is considered a
  // duplicate of existing `all_prefetches_map_`.
  // Used only when `kWebViewPrefetchOffTheMainThread` is enabled.
  bool IsPrefetchDuplicate(const GURL& url,
                           const std::optional<net::HttpNoVarySearchData>&
                               expected_no_vary_search) const;

  // Evicts the oldest prefetch from `all_prefetches_map_` to guarantee
  // there is space for one new request when `max_prefetches_` is reached.
  void MayEvictOldestPrefetchHandleForANewRequest();

  // Stores the wrapper and returns its tracking key.
  //
  // Currently callers must ensure there is available capacity before
  // insertion, by calling `MayEvictOldestPrefetchHandleForANewRequest()`. See
  // `MayEvictOldestPrefetchHandleForANewRequest()`'s inner comment for more
  // context.
  AwPrefetchKey AddPrefetchHandle(
      std::unique_ptr<AwPrefetchHandleWrapper> prefetch_handle_wrapper);

  void CancelPrefetch(AwPrefetchKey prefetch_key);

  void SetTtlInSec(int ttl_in_sec);
  int GetTtlInSec() const;
  void SetMaxPrefetches(size_t max_prefetches);

  // Testing utilities.
  size_t GetMaxPrefetchesForTesting() const;
  std::vector<AwPrefetchKey> GetAllPrefetchKeysForTesting() const;
  AwPrefetchKey GetLastPrefetchKeyForTesting() const;
  bool GetIsPrefetchInCacheForTesting(AwPrefetchKey prefetch_key) const;

 private:
  AwPrefetchKey GetNextPrefetchKey() const;
  void UpdateLastPrefetchKey(AwPrefetchKey new_key);

  int ttl_in_sec_ = kDefaultTtlInSec;

  size_t max_prefetches_ = kDefaultMaxPrefetches;

  std::map<AwPrefetchKey, std::unique_ptr<AwPrefetchHandleWrapper>>
      all_prefetches_map_;

  // Should only be incremented. Acts as an "order added" mechanism
  // inside of `all_prefetches_map_` since `std::map` stores keys
  // in a sorted order.
  AwPrefetchKey last_prefetch_key_ = -1;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_
