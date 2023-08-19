// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_SITE_PAIR_CACHE_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_SITE_PAIR_CACHE_H_

#include <utility>

#include "base/containers/flat_set.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

// A container that holds pairs of (schemeful) sites. This class is designed to
// avoid the cost of converting from origin to site if possible (assuming that
// map lookups are faster than converting).
class SitePairCache {
 public:
  SitePairCache();
  SitePairCache(const SitePairCache&) = delete;
  SitePairCache& operator=(const SitePairCache&) = delete;
  ~SitePairCache();

  // Inserts into the cache if necessary. Returns true if a new <site, site>
  // pair was inserted; false if the <site, site> pair was already present.
  bool Insert(const url::Origin& first, const url::Origin& second);

  // Clears the cache.
  void Clear();

 private:
  // First layer of the cache: by origin. This lets us avoid converting to
  // SchemefulSites for pairs that have already been inserted.
  base::flat_set<std::pair<url::Origin, url::Origin>> origins_;

  // Second layer of the cache: by SchemefulSite. This is the final source of
  // truth for the contents of this cache.
  base::flat_set<std::pair<net::SchemefulSite, net::SchemefulSite>> sites_;
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_SITE_PAIR_CACHE_H_
