// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_page_cache.h"

#include "base/check_op.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicMediaPageCache::Entry::Entry() = default;

GlicMediaPageCache::Entry::~Entry() {
  if (cache_) {
    cache_->RemoveFromCache(this);
  }
}

void GlicMediaPageCache::Entry::OnCached(GlicMediaPageCache* cache) {
  CHECK_EQ(cache_, nullptr);
  cache_ = cache;
}

void GlicMediaPageCache::Entry::OnUncached() {
  CHECK(cache_);
  cache_ = nullptr;
}

GlicMediaPageCache::GlicMediaPageCache() = default;

GlicMediaPageCache::~GlicMediaPageCache() {
  while (!cache_.empty()) {
    RemoveFromCache(static_cast<Entry*>(cache_.head()));
  }
}

void GlicMediaPageCache::PlaceAtFront(Entry* entry) {
  // It's okay if `entry` is in our cache already, or not on any cache.  It is
  // not okay if it is on some other cache.
  CHECK(entry->cache() == nullptr || entry->cache() == this);
  if (entry->next()) {
    RemoveFromCache(entry);
  }
  entry->InsertBefore(cache_.head());
  if (entry->cache() == nullptr) {
    entry->OnCached(this);
  }
}

void GlicMediaPageCache::RemoveFromCache(Entry* entry) {
  CHECK(entry->next());
  CHECK_EQ(entry->cache(), this);
  entry->RemoveFromList();
  entry->OnUncached();
}

}  // namespace glic
