// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/site_pair_cache.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

SitePairCache::SitePairCache() = default;
SitePairCache::~SitePairCache() = default;

bool SitePairCache::Insert(const url::Origin& fst, const url::Origin& snd) {
  if (!origins_.emplace(fst, snd).second) {
    return false;
  }

  return sites_.emplace(net::SchemefulSite(fst), net::SchemefulSite(snd))
      .second;
}

void SitePairCache::Clear() {
  origins_.clear();
  sites_.clear();
}
