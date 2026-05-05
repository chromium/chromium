// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"

#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "android_webview/common/aw_features.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

using content::BrowserThread;

AwPrefetchManagerData::AwPrefetchManagerData()
    : lock_(IsWebViewPrefetchOffTheMainThreadEnabled()
                ? std::make_unique<base::Lock>()
                : nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AwPrefetchManagerData::~AwPrefetchManagerData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AwPrefetchKey AwPrefetchManagerData::AddNewPrefetchHandleWrapper(
    std::unique_ptr<AwPrefetchHandleWrapper> prefetch_handle_wrapper) {
  CHECK(!IsWebViewPrefetchOffTheMainThreadEnabled());
  int32_t new_prefetch_key;

  base::AutoLockMaybe auto_lock(lock_.get());

  CHECK(prefetch_handle_wrapper);
  CHECK(max_prefetches_ > 0u);
  CHECK(all_prefetches_map_.size() < max_prefetches_);

  new_prefetch_key = GetNextPrefetchKeyLocked();
  // `all_prefetches_map_[new_prefetch_key]` should have no entry because
  // `GetNextPrefetchKeyLocked()` always returns a new key.
  all_prefetches_map_[new_prefetch_key] = std::move(prefetch_handle_wrapper);
  UpdateLastPrefetchKeyLocked(new_prefetch_key);

  return new_prefetch_key;
}

AwPrefetchKey AwPrefetchManagerData::ReservePrefetchHandleWrapper(
    const GURL& url,
    const std::optional<net::HttpNoVarySearchData>& expected_no_vary_search) {
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());
  std::vector<std::unique_ptr<AwPrefetchHandleWrapper>>
      old_prefetch_handle_wrappers;
  AwPrefetchKey new_prefetch_key;
  {
    base::AutoLockMaybe auto_lock(lock_.get());

    bool is_duplicate = IsPrefetchDuplicateLocked(url, expected_no_vary_search);
    if (is_duplicate) {
      return NO_PREFETCH_KEY;
    }

    // Evict oldest handles if necessary.
    old_prefetch_handle_wrappers =
        MayEvictOldestPrefetchHandleForANewRequestLocked();

    new_prefetch_key = GetNextPrefetchKeyLocked();
    // `all_prefetches_map_[new_prefetch_key]` should have no entry because
    // `GetNextPrefetchKeyLocked()` always returns a new key.
    CHECK(!all_prefetches_map_[new_prefetch_key]);
    all_prefetches_map_[new_prefetch_key] =
        std::make_unique<AwPrefetchHandleWrapper>(url, expected_no_vary_search);
    UpdateLastPrefetchKeyLocked(new_prefetch_key);
  }

  return new_prefetch_key;
  // `old_prefetch_handle_wrappers` is dropped here, outside of the
  // `lock_` to prevent accidental reentrancy.
}

void AwPrefetchManagerData::CommitInitialPrePrefetchHandle(
    AwPrefetchKey prefetch_key,
    std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle) {
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());
  base::AutoLockMaybe auto_lock(lock_.get());

  auto it = all_prefetches_map_.find(prefetch_key);
  if (it != all_prefetches_map_.end()) {
    it->second->CommitInitialPrePrefetchHandle(std::move(pre_prefetch_handle));
  }
}

void AwPrefetchManagerData::CommitInitialPrefetchHandle(
    AwPrefetchKey prefetch_key,
    std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());
  base::AutoLockMaybe auto_lock(lock_.get());

  auto it = all_prefetches_map_.find(prefetch_key);
  if (it != all_prefetches_map_.end()) {
    it->second->CommitInitialPrefetchHandle(std::move(prefetch_handle));
  }
}

std::unique_ptr<content::PrePrefetchHandle>
AwPrefetchManagerData::TakePrePrefetchHandleForConsume(
    AwPrefetchKey prefetch_key) {
  base::AutoLockMaybe auto_lock(lock_.get());

  auto it = all_prefetches_map_.find(prefetch_key);
  if (it != all_prefetches_map_.end() &&
      it->second->CanTakePrePrefetchHandleForConsume()) {
    return it->second->TakePrePrefetchHandleForConsume();
  }

  return nullptr;
}

void AwPrefetchManagerData::CommitPrefetchHandleAfterConsume(
    AwPrefetchKey prefetch_key,
    std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
  base::AutoLockMaybe auto_lock(lock_.get());

  auto it = all_prefetches_map_.find(prefetch_key);
  if (it != all_prefetches_map_.end()) {
    it->second->CommitPrefetchHandleAfterConsume(std::move(prefetch_handle));
  }

  // If the prefetch was already removed by other calls, this does nothing.
  // `prefetch_handle` will be released here.
}

bool AwPrefetchManagerData::IsPrefetchDuplicateLocked(
    const GURL& url,
    const std::optional<net::HttpNoVarySearchData>& expected_no_vary_search)
    const {
  std::vector<const content::PrefetchDeduplicationEntry*> candidates;
  candidates.reserve(all_prefetches_map_.size());
  for (const auto& [_, prefetch_handle_wrapper] : all_prefetches_map_) {
    candidates.push_back(prefetch_handle_wrapper.get());
  }
  return content::IsPrefetchDuplicate(candidates, url, expected_no_vary_search);
}

std::vector<std::unique_ptr<AwPrefetchHandleWrapper>>
AwPrefetchManagerData::MayEvictOldestPrefetchHandleForANewRequestLocked() {
  std::vector<std::unique_ptr<AwPrefetchHandleWrapper>>
      old_prefetch_handle_wrappers;
  if (all_prefetches_map_.size() >= max_prefetches_) {
    int num_prefetches_to_evict =
        all_prefetches_map_.size() - max_prefetches_ + 1;
    auto it = all_prefetches_map_.begin();
    while (num_prefetches_to_evict > 0 && it != all_prefetches_map_.end()) {
      // Because the keys should be sequential based on when the prefetch
      // associated with them was added, a standard iteration should always
      // prioritize removing the oldest entry.
      old_prefetch_handle_wrappers.push_back(std::move(it->second));
      it = all_prefetches_map_.erase(it);
      num_prefetches_to_evict--;
    }
  }
  return old_prefetch_handle_wrappers;
}

void AwPrefetchManagerData::MayEvictOldestPrefetchHandleForANewRequest() {
  std::vector<std::unique_ptr<AwPrefetchHandleWrapper>>
      old_prefetch_handle_wrappers;
  {
    base::AutoLockMaybe auto_lock(lock_.get());
    old_prefetch_handle_wrappers =
        MayEvictOldestPrefetchHandleForANewRequestLocked();
  }

  // `old_prefetch_handle_wrappers` is dropped here, outside of the
  // `lock_` to prevent accidental reentrancy.
}

void AwPrefetchManagerData::CancelPrefetch(AwPrefetchKey prefetch_key) {
  std::unique_ptr<AwPrefetchHandleWrapper> old_prefetch_handle_wrapper;
  {
    base::AutoLockMaybe auto_lock(lock_.get());

    auto it = all_prefetches_map_.find(prefetch_key);
    if (it != all_prefetches_map_.end()) {
      old_prefetch_handle_wrapper = std::move(it->second);
      all_prefetches_map_.erase(it);
    }
  }

  // `old_prefetch_handle_wrapper` is dropped here, outside of the
  // `lock_` to prevent accidental reentrancy.
}

bool AwPrefetchManagerData::UpdateLatestPrefetchInfo(
    const AwPrefetchLatestInfoPref& info) {
  base::AutoLockMaybe auto_lock(lock_.get());
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());
  if (prefetch_latest_info_ == info) {
    return false;
  }
  prefetch_latest_info_ = info;
  return true;
}

void AwPrefetchManagerData::SetTtlInSec(int ttl_in_sec) {
  base::AutoLockMaybe auto_lock(lock_.get());

  ttl_in_sec_ = ttl_in_sec;
}

void AwPrefetchManagerData::SetMaxPrefetches(size_t max_prefetches) {
  base::AutoLockMaybe auto_lock(lock_.get());

  max_prefetches_ = max_prefetches;
}

int AwPrefetchManagerData::GetTtlInSec() const {
  base::AutoLockMaybe auto_lock(lock_.get());

  return ttl_in_sec_;
}

size_t AwPrefetchManagerData::GetMaxPrefetches() const {
  base::AutoLockMaybe auto_lock(lock_.get());

  return max_prefetches_;
}

std::vector<AwPrefetchKey>
AwPrefetchManagerData::GetAllPrefetchKeysForTesting()  // IN-TEST
    const {
  base::AutoLockMaybe auto_lock(lock_.get());

  std::vector<AwPrefetchKey> prefetch_keys;
  prefetch_keys.reserve(all_prefetches_map_.size());
  for (const auto& [key, prefetch_handle_wrapper] : all_prefetches_map_) {
    prefetch_keys.push_back(key);
  }
  return prefetch_keys;
}

AwPrefetchKey AwPrefetchManagerData::GetLastPrefetchKeyForTesting()  // IN-TEST
    const {
  base::AutoLockMaybe auto_lock(lock_.get());

  return last_prefetch_key_;
}

bool AwPrefetchManagerData::GetIsPrefetchInCacheForTesting(  // IN-TEST
    AwPrefetchKey prefetch_key) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLockMaybe auto_lock(lock_.get());

  return all_prefetches_map_.find(prefetch_key) != all_prefetches_map_.end();
}

AwPrefetchKey AwPrefetchManagerData::GetNextPrefetchKeyLocked() const {
  return last_prefetch_key_ + 1;
}

void AwPrefetchManagerData::UpdateLastPrefetchKeyLocked(AwPrefetchKey new_key) {
  CHECK(new_key > last_prefetch_key_);
  last_prefetch_key_ = new_key;
}

}  // namespace android_webview
