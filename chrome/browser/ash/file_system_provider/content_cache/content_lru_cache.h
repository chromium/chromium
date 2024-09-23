// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_LRU_CACHE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_LRU_CACHE_H_

#include <list>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"

namespace ash::file_system_provider {

class ContentLRUCache
    : public base::HashingLRUCache<base::FilePath, CacheFileContext> {
 public:
  ContentLRUCache();

  ContentLRUCache(const ContentLRUCache&) = delete;
  ContentLRUCache& operator=(const ContentLRUCache&) = delete;

  ~ContentLRUCache();

  // Initialize the LRU cache in the supplied order.
  void Init(std::list<PathContextPair> initial_order);
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_LRU_CACHE_H_
