// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_PAGE_CACHE_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_PAGE_CACHE_H_

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"

namespace glic {

// Cache for pages with media context.  Currently, this is a thin wrapper
// around `LinkedList` that avoids shutdown order issues.  This can be
// expanded to cache entries based on other properties, like document
// pip-ness, in the future.
class GlicMediaPageCache {
 public:
  // Handy base class for cache entries.  It would be nice if this could be
  // private inheritance to `LinkNode`, but that's more boiler plate than it's
  // worth.  Simply don't call any `LinkNode` methods outside of the cache.
  class Entry : public base::LinkNode<Entry> {
   public:
    Entry();
    virtual ~Entry();

    GlicMediaPageCache* cache() { return cache_; }

    // Notify us that we've been added to the cache.
    void OnCached(GlicMediaPageCache*);

    // Notify that we've been removed from the cache.  One might override
    // `RemoveFromList`, but it's not virtual and would be very easy to call the
    // base class implementation unintentionally.
    void OnUncached();

   private:
    // Cache to which this entry belongs, or null if we're not cached.
    raw_ptr<GlicMediaPageCache> cache_ = nullptr;
  };

  GlicMediaPageCache();
  virtual ~GlicMediaPageCache();

  // Will either move or add, as appropriate.
  void PlaceAtFront(Entry* entry);

  // Use this in favor of Entry::RemoveFromList, because there is more cleanup
  // than just that.
  void RemoveFromCache(Entry* entry);

  Entry* front() {
    return !cache_.empty() ? static_cast<Entry*>(cache_.head()) : nullptr;
  }

  const base::LinkedList<Entry>& cache_for_testing() const { return cache_; }

 private:
  base::LinkedList<Entry> cache_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_PAGE_CACHE_H_
