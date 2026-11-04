// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ICON_CACHE_H_
#define ASH_BIRCH_BIRCH_ICON_CACHE_H_

#include <cstddef>
#include <string>

#include "ash/ash_export.h"
#include "base/containers/lru_cache.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Bounded LRU cache for icons downloaded from URLs. URLs are encoded as strings
// as this is common in client code.
class ASH_EXPORT BirchIconCache {
 public:
  // Maximum number of downloaded icons retained in memory. The birch bar shows
  // at most 4 chips, so this covers the icons of several overview sessions as
  // suggestions change, while bounding memory since downloaded icons can be
  // large (e.g. ~147 KB for a 192x192 touch icon).
  static constexpr size_t kMaxCacheSize = 20;

  BirchIconCache();
  BirchIconCache(const BirchIconCache&) = delete;
  BirchIconCache& operator=(const BirchIconCache&) = delete;
  ~BirchIconCache();

  // Gets an icon based on a URL. If the icon is not in the cache a null image
  // is returned.
  gfx::ImageSkia Get(const std::string& url);

  // Adds or replaces an image in the cache.
  void Put(const std::string& url, const gfx::ImageSkia& icon);

  // Clears all cached icons.
  void Clear();

  size_t size_for_test() const { return cache_.size(); }

 private:
  // Maps a URL to an icon with LRU eviction.
  base::LRUCache<std::string, gfx::ImageSkia> cache_{kMaxCacheSize};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ICON_CACHE_H_
