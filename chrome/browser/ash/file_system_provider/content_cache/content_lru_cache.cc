// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"

#include "base/containers/adapters.h"

namespace ash::file_system_provider {

ContentLRUCache::ContentLRUCache()
    : base::HashingLRUCache<base::FilePath, CacheFileContext>(NO_AUTO_EVICT) {}
ContentLRUCache::~ContentLRUCache() = default;

void ContentLRUCache::Init(std::list<PathContextPair> initial_order) {
  for (const auto& [key, value] : base::Reversed(initial_order)) {
    Put(key, value);
  }
}

}  // namespace ash::file_system_provider
