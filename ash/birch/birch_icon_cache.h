// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ICON_CACHE_H_
#define ASH_BIRCH_BIRCH_ICON_CACHE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Caches icons downloaded from URLs. Birch has a very small number of icons so
// there is no cache invalidation or removal of items from the cache. URLs are
// encoded as strings as this is common in client code.
class ASH_EXPORT BirchIconCache {
 public:
  BirchIconCache();
  BirchIconCache(const BirchIconCache&) = delete;
  BirchIconCache& operator=(const BirchIconCache&) = delete;
  ~BirchIconCache();

  // Gets an icon based on a URL. If the icon is not in the cache a null image
  // is returned.
  gfx::ImageSkia Get(const std::string& url);

  // Adds or replaces an image in the cache.
  void Put(const std::string& url, const gfx::ImageSkia& icon);

  size_t size_for_test() const { return map_.size(); }

 private:
  // Maps a URL to an icon.
  base::flat_map<std::string, gfx::ImageSkia> map_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ICON_CACHE_H_
