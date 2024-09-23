// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_icon_cache.h"

#include <string>

#include "base/containers/flat_map.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

BirchIconCache::BirchIconCache() = default;

BirchIconCache::~BirchIconCache() = default;

gfx::ImageSkia BirchIconCache::Get(const std::string& url) {
  auto it = map_.find(url);
  if (it == map_.end()) {
    // Return a null image if the URL was not found.
    return gfx::ImageSkia();
  }
  return it->second;
}

void BirchIconCache::Put(const std::string& url, const gfx::ImageSkia& icon) {
  map_[url] = icon;
}

}  // namespace ash
