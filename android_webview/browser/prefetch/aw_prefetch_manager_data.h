// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_

#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"
#include "android_webview/browser/prefetch/aw_prefetch_prefs.h"
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

// An `AwPrefetchManager`-specific store that manages its mutable state, which
// can be modified by this public methods.
//
// Thread mode:
//
// This class is designed to be thread-safe.
//
// This should be created/owned/destructed by `AwPrefetchManager` on the UI
// thread.
//
// When `kWebViewPrefetchOffTheMainThread` feature is enabled:
// - All public methods acquire `lock_` before accessing member variables.
//   And thus public methods shouldn't call other public methods, as annotated
//   by LOCKS_EXCLUDED(lock_).
// - Private methods (`*Locked()`) must be called after acquiring `lock_`, as
//   annotated by `EXCLUSIVE_LOCKS_REQUIRED(lock_)`.
// - To avoid unexpected reentrancy/deadlocks, any method shouldn't trigger any
//   complex logic, external delegates/callbacks etc. `lock_` is not needed to
//   be instantiated, and thus ignored by `AutoLockMaybe`.
//
// When `kWebViewPrefetchOffTheMainThread` feature is disabled, this class
// should only be accessed from the UI thread.
class AwPrefetchManagerData {
 public:
  AwPrefetchManagerData();
  ~AwPrefetchManagerData();

  // Evicts the oldest prefetch from `all_prefetches_map_` to guarantee
  // there is space for one new request when `max_prefetches_` is reached.
  void MayEvictOldestPrefetchHandleForANewRequest() LOCKS_EXCLUDED(lock_);

  // Stores the wrapper and returns its tracking key.
  //
  // Currently callers are expected to ensure there is available capacity and
  // no deduplication before insertion, by calling
  // `MayEvictOldestPrefetchHandleForANewRequest()` and `IsPrefetchDuplicate()`.
  // For the latter, see `MayEvictOldestPrefetchHandleForANewRequest()`'s inner
  // comment for more context.
  // Used only when `kWebViewPrefetchOffTheMainThread` is disabled.
  AwPrefetchKey AddNewPrefetchHandleWrapper(
      std::unique_ptr<AwPrefetchHandleWrapper> prefetch_handle_wrapper)
      LOCKS_EXCLUDED(lock_);

  // The pair of functions to start and store a (Pre)Prefetch, used when
  // `kWebViewPrefetchOffTheMainThread` is enabled.
  //
  // `ReservePrefetchHandleWrapper()` atomically checks for deduplication,
  // evicts the oldest prefetch if necessary, and inserts an empty
  // `AwPrefetchHandleWrapper` to `AwPrefetchManagerData` as a reservation.
  // This prevents a TOCTOU race condition while (pre-)prefetch actually
  // starts. Returns NO_PREFETCH_KEY if it is a duplicate.
  // `CommitInitial(Pre)PrefetchHandle()` actually commits the
  // `(Pre)PrefetchHandle` to the wrapper.
  //
  // Currently it is the caller's responsibility to clean up the wrapper from
  // `all_prefetches_map_` when `ReservePrefetchHandleWrapper()` is called but
  // `CommitInitial(Pre)PrefetchHandle()` can't eventually be called (e.g. if
  // starting the (pre-)prefetch fails).
  // TODO(crbug.com/452406598): This should ideally be mitigated by introducing
  // a writer interface that grants write permission to the wrapper and
  // automatically handles rollback on failure.
  AwPrefetchKey ReservePrefetchHandleWrapper(
      const GURL& url,
      const std::optional<net::HttpNoVarySearchData>& expected_no_vary_search)
      LOCKS_EXCLUDED(lock_);
  void CommitInitialPrePrefetchHandle(
      AwPrefetchKey prefetch_key,
      std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle)
      LOCKS_EXCLUDED(lock_);
  void CommitInitialPrefetchHandle(
      AwPrefetchKey prefetch_key,
      std::unique_ptr<content::PrefetchHandle> prefetch_handle)
      LOCKS_EXCLUDED(lock_);

  // The pair of functions to take `PrePrefetchHandle` from
  // an existing `AwPrefetchHandleWrapper` and commit a `PrefetchHandle` to the
  // same wrapper after PrePrefetch consumption.
  //
  // Currently it is the caller's responsibility to clean up the wrapper from
  // `all_prefetches_map_`, when `TakePrePrefetchHandleForConsume` is called
  // but `CommitPrefetchHandleAfterConsume` can't eventually be called (e.g. if
  // starting the prefetch fails).
  // TODO(crbug.com/452406598): This should ideally be mitigated by introducing
  // a writer interface that grants write permission to the wrapper and
  // automatically handles rollback on failure.
  std::unique_ptr<content::PrePrefetchHandle> TakePrePrefetchHandleForConsume(
      AwPrefetchKey prefetch_key) LOCKS_EXCLUDED(lock_);
  void CommitPrefetchHandleAfterConsume(
      AwPrefetchKey prefetch_key,
      std::unique_ptr<content::PrefetchHandle> prefetch_handle)
      LOCKS_EXCLUDED(lock_);

  void CancelPrefetch(AwPrefetchKey prefetch_key) LOCKS_EXCLUDED(lock_);

  size_t GetMaxPrefetches() const LOCKS_EXCLUDED(lock_);
  void SetTtlInSec(int ttl_in_sec) LOCKS_EXCLUDED(lock_);
  int GetTtlInSec() const LOCKS_EXCLUDED(lock_);
  void SetMaxPrefetches(size_t max_prefetches) LOCKS_EXCLUDED(lock_);

  // Updates the `latest_info_cache_`.
  // Returns `true` if the settings were actually changed, which signals to the
  // caller (`AwPrefetchManager`) that the new values should be written to the
  // persistent `PrefService`.
  // Only used when `kWebViewPrefetchOffTheMainThread` is enabled.
  bool UpdateLatestPrefetchInfo(const AwPrefetchLatestInfoPref& info)
      LOCKS_EXCLUDED(lock_);

  // Testing utilities.
  std::vector<AwPrefetchKey> GetAllPrefetchKeysForTesting() const
      LOCKS_EXCLUDED(lock_);
  AwPrefetchKey GetLastPrefetchKeyForTesting() const LOCKS_EXCLUDED(lock_);
  bool GetIsPrefetchInCacheForTesting(AwPrefetchKey prefetch_key) const
      LOCKS_EXCLUDED(lock_);

 private:
  // Utility functions that assume the lock is already held.

  // Returns true if the prefetch with URL and NVS hint is considered a
  // duplicate of existing `all_prefetches_map_`.
  // Used only when `kWebViewPrefetchOffTheMainThread` is enabled.
  bool IsPrefetchDuplicateLocked(const GURL& url,
                                 const std::optional<net::HttpNoVarySearchData>&
                                     expected_no_vary_search) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::vector<std::unique_ptr<AwPrefetchHandleWrapper>>
  MayEvictOldestPrefetchHandleForANewRequestLocked()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  AwPrefetchKey GetNextPrefetchKeyLocked() const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UpdateLastPrefetchKeyLocked(AwPrefetchKey new_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Guards the below non-const members. Allocated only when the
  // `WebViewPrefetchOffTheMainThread` feature is enabled. Should be used with
  // `base::AutoLockMaybe` to avoid lock overhead when the feature is
  // disabled.
  const std::unique_ptr<base::Lock> lock_;

  // Memorizes and caches the latest prefetch request info. Used to prevent
  // asking `PrefService` with redundant updates.
  // Only used when `kWebViewPrefetchOffTheMainThread` is enabled.
  AwPrefetchLatestInfoPref prefetch_latest_info_ GUARDED_BY(lock_) = {
      url::Origin(), false};

  int ttl_in_sec_ GUARDED_BY(lock_) = kDefaultTtlInSec;

  size_t max_prefetches_ GUARDED_BY(lock_) = kDefaultMaxPrefetches;

  std::map<AwPrefetchKey, std::unique_ptr<AwPrefetchHandleWrapper>>
      all_prefetches_map_ GUARDED_BY(lock_);

  // Should only be incremented. Acts as an "order added" mechanism
  // inside of `all_prefetches_map_` since `std::map` stores keys
  // in a sorted order.
  AwPrefetchKey last_prefetch_key_ GUARDED_BY(lock_) = -1;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_DATA_H_
