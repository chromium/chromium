// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_

#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"

namespace ash::file_system_provider {

// The content cache for every mounted FSP. This will serve as the single point
// of orchestration between the LRU cache and the disk persistence layer.
class ContentCache {
 public:
  explicit ContentCache(const base::FilePath& root_dir);

  ContentCache(const ContentCache&) = delete;
  ContentCache& operator=(const ContentCache&) = delete;

  ~ContentCache();

 private:
  const base::FilePath root_dir_;
  ContentLRUCache lru_cache_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
