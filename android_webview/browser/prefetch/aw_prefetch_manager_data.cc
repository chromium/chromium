// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"

#include "content/public/browser/browser_thread.h"

namespace android_webview {

using content::BrowserThread;

AwPrefetchManagerData::AwPrefetchManagerData() = default;

AwPrefetchManagerData::~AwPrefetchManagerData() = default;

AwPrefetchKey AwPrefetchManagerData::AddPrefetchHandle(
    std::unique_ptr<AwPrefetchHandleWrapper> prefetch_handle_wrapper) {
  CHECK(prefetch_handle_wrapper);
  CHECK(max_prefetches_ > 0u);
  CHECK(all_prefetches_map_.size() < max_prefetches_);

  const AwPrefetchKey new_prefetch_key = GetNextPrefetchKey();
  all_prefetches_map_[new_prefetch_key] = std::move(prefetch_handle_wrapper);
  UpdateLastPrefetchKey(new_prefetch_key);
  return new_prefetch_key;
}

bool AwPrefetchManagerData::IsPrefetchDuplicate(
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

void AwPrefetchManagerData::MayEvictOldestPrefetchHandleForANewRequest() {
  if (all_prefetches_map_.size() >= max_prefetches_) {
    int num_prefetches_to_evict =
        all_prefetches_map_.size() - max_prefetches_ + 1;
    auto it = all_prefetches_map_.begin();

    while (num_prefetches_to_evict > 0 && it != all_prefetches_map_.end()) {
      // Because the keys should be sequential based on when the prefetch
      // associated with it was added, a standard iteration should always
      // prioritize removing the oldest entry.
      it = all_prefetches_map_.erase(it);
      num_prefetches_to_evict--;
    }
  }
}

void AwPrefetchManagerData::CancelPrefetch(AwPrefetchKey prefetch_key) {
  all_prefetches_map_.erase(prefetch_key);
}

void AwPrefetchManagerData::SetTtlInSec(int ttl_in_sec) {
  ttl_in_sec_ = ttl_in_sec;
}

void AwPrefetchManagerData::SetMaxPrefetches(size_t max_prefetches) {
  max_prefetches_ = max_prefetches;
}

int AwPrefetchManagerData::GetTtlInSec() const {
  return ttl_in_sec_;
}

size_t AwPrefetchManagerData::GetMaxPrefetchesForTesting() const {  // IN-TEST
  return max_prefetches_;
}

std::vector<AwPrefetchKey>
AwPrefetchManagerData::GetAllPrefetchKeysForTesting()  // IN-TEST
    const {
  std::vector<AwPrefetchKey> prefetch_keys;
  prefetch_keys.reserve(all_prefetches_map_.size());
  for (const auto& [key, prefetch_handle_wrapper] : all_prefetches_map_) {
    prefetch_keys.push_back(key);
  }
  return prefetch_keys;
}

AwPrefetchKey AwPrefetchManagerData::GetLastPrefetchKeyForTesting()  // IN-TEST
    const {
  return last_prefetch_key_;
}

bool AwPrefetchManagerData::GetIsPrefetchInCacheForTesting(  // IN-TEST
    AwPrefetchKey prefetch_key) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return all_prefetches_map_.find(prefetch_key) != all_prefetches_map_.end();
}

AwPrefetchKey AwPrefetchManagerData::GetNextPrefetchKey() const {
  return last_prefetch_key_ + 1;
}

void AwPrefetchManagerData::UpdateLastPrefetchKey(AwPrefetchKey new_key) {
  CHECK(new_key > last_prefetch_key_);
  last_prefetch_key_ = new_key;
}

}  // namespace android_webview
