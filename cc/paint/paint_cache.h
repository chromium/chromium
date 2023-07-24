// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CACHE_H_
#define CC_PAINT_PAINT_CACHE_H_

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "cc/paint/paint_export.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {

// PaintCache is used to cache high frequency small paint data types, like
// SkTextBlob and SkPath in the GPU service. The ClientPaintCache budgets and
// controls the cache state in the ServicePaintCache, regularly purging old
// entries returned in ClientPaintCache::Purge from the service side cache. In
// addition to this, the complete cache is cleared during the raster context
// idle cleanup. This effectively means that the cache budget is used as working
// memory that is only kept while we are actively rasterizing.
//
// The entries are serialized by the caller during paint op serialization, and
// the cache assumes the deserialization and purging to be done in order for
// accurately tracking the service side state in ServicePaintCache.
//
// Note that while TransferCache should be used for large data types that would
// benefit from a shared cache budgeted across all clients, using a client
// controlled PaintCache with a tighter budget is better for these data types
// since it avoids the need for cross-process ref-counting required by the
// TransferCache.

using PaintCacheId = uint32_t;
using PaintCacheIds = std::vector<PaintCacheId>;
enum class PaintCacheDataType : uint32_t { kPath, kLast = kPath };
enum class PaintCacheEntryState : uint32_t {
  kEmpty,
  kCached,
  kInlined,
  kInlinedDoNotCache,
  kLast = kInlinedDoNotCache
};

constexpr size_t PaintCacheDataTypeCount =
    static_cast<uint32_t>(PaintCacheDataType::kLast) + 1u;

class CC_PAINT_EXPORT ClientPaintCache {
 public:
  // If ClientPaintCache is constructed with a max_budget_bytes of
  // kNoCachingBudget, its Put() method becomes a no-op, rendering the instance
  // a no-op instance.
  static constexpr size_t kNoCachingBudget = 0u;

  explicit ClientPaintCache(size_t max_budget_bytes);
  ClientPaintCache(const ClientPaintCache&) = delete;
  ~ClientPaintCache();

  ClientPaintCache& operator=(const ClientPaintCache&) = delete;

  bool Get(PaintCacheDataType type, PaintCacheId id);
  void Put(PaintCacheDataType type, PaintCacheId id, size_t size);

  // Populates |purged_data| with the list of ids which should be purged from
  // the ServicePaintCache.
  using PurgedData = PaintCacheIds[PaintCacheDataTypeCount];
  void Purge(PurgedData* purged_data);

  // Finalize the state of pending entries, which were sent to the service-side
  // cache.
  void FinalizePendingEntries();

  // Notifies that the pending entries were not sent to the service-side cache
  // and should be discarded.
  void AbortPendingEntries();

  // Notifies that all entries should be purged from the ServicePaintCache.
  // Returns true if any entries were evicted from this call.
  bool PurgeAll();

  size_t bytes_used() const { return bytes_used_; }

 private:
  using CacheKey = std::pair<PaintCacheDataType, PaintCacheId>;
  using CacheMap = base::LRUCache<CacheKey, size_t>;

  template <typename Iterator>
  void EraseFromMap(Iterator it);

  CacheMap cache_map_;
  const size_t max_budget_;
  size_t bytes_used_ = 0u;

  // List of entries added to the map but not committed since we might fail to
  // send them to the service-side cache. This is necessary to ensure we
  // maintain an accurate mirror of the service-side state.
  absl::InlinedVector<CacheKey, 1> pending_entries_;
};

class CC_PAINT_EXPORT ServicePaintCache {
 public:
  ServicePaintCache();
  ~ServicePaintCache();

  // Stores |path| received from the client in the cache.
  void PutPath(PaintCacheId, SkPath path);

  // Retrieves an entry for |id| stored in the cache. The path data is stored in
  // |path| pointed memory. Returns false, if the entry is not found.
  bool GetPath(PaintCacheId id, SkPath* path) const;

  void Purge(PaintCacheDataType type,
             size_t n,
             const volatile PaintCacheId* ids);
  void PurgeAll();
  bool empty() const { return cached_paths_.empty(); }

 private:
  using PathMap = std::map<PaintCacheId, SkPath>;
  PathMap cached_paths_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_CACHE_H_
