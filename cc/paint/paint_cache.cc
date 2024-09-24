// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_cache.h"

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"

namespace cc {
namespace {

template <typename T>
void EraseFromMap(T* map, size_t n, const volatile PaintCacheId* ids) {
  for (size_t i = 0; i < n; ++i) {
    auto id = ids[i];
    map->erase(id);
  }
}

}  // namespace

constexpr size_t ClientPaintCache::kNoCachingBudget;

ClientPaintCache::ClientPaintCache(size_t max_budget_bytes)
    : cache_map_(CacheMap::NO_AUTO_EVICT), max_budget_(max_budget_bytes) {}
ClientPaintCache::~ClientPaintCache() = default;

bool ClientPaintCache::Get(PaintCacheDataType type, PaintCacheId id) {
  return cache_map_.Get(std::make_pair(type, id)) != cache_map_.end();
}

void ClientPaintCache::Put(PaintCacheDataType type,
                           PaintCacheId id,
                           size_t size) {
  if (max_budget_ == kNoCachingBudget)
    return;
  auto key = std::make_pair(type, id);
  DCHECK(cache_map_.Peek(key) == cache_map_.end());

  pending_entries_.push_back(key);
  cache_map_.Put(key, size);
  bytes_used_ += size;
}

template <typename Iterator>
void ClientPaintCache::EraseFromMap(Iterator it) {
  DCHECK_GE(bytes_used_, it->second);
  bytes_used_ -= it->second;
  cache_map_.Erase(it);
}

void ClientPaintCache::FinalizePendingEntries() {
  pending_entries_.clear();
}

void ClientPaintCache::AbortPendingEntries() {
  for (const auto& entry : pending_entries_) {
    auto it = cache_map_.Peek(entry);
    CHECK(it != cache_map_.end(), base::NotFatalUntil::M130);
    EraseFromMap(it);
  }
  pending_entries_.clear();
}

void ClientPaintCache::Purge(PurgedData* purged_data) {
  DCHECK(pending_entries_.empty());

  while (bytes_used_ > max_budget_) {
    auto it = cache_map_.rbegin();
    PaintCacheDataType type = it->first.first;
    PaintCacheId id = it->first.second;

    EraseFromMap(it);
    (*purged_data)[static_cast<uint32_t>(type)].push_back(id);
  }
}

bool ClientPaintCache::PurgeAll() {
  DCHECK(pending_entries_.empty());

  bool has_data = !cache_map_.empty();
  cache_map_.Clear();
  bytes_used_ = 0u;
  return has_data;
}

ServicePaintCache::ServicePaintCache() = default;
ServicePaintCache::~ServicePaintCache() = default;

void ServicePaintCache::PutPath(PaintCacheId id, SkPath path) {
  cached_paths_.emplace(id, std::move(path));
}

bool ServicePaintCache::GetPath(PaintCacheId id, SkPath* path) const {
  auto it = cached_paths_.find(id);
  if (it == cached_paths_.end())
    return false;
  *path = it->second;
  return true;
}

void ServicePaintCache::Purge(PaintCacheDataType type,
                              size_t n,
                              const volatile PaintCacheId* ids) {
  switch (type) {
    case PaintCacheDataType::kPath:
      EraseFromMap(&cached_paths_, n, ids);
      return;
  }

  NOTREACHED();
}

void ServicePaintCache::PurgeAll() {
  cached_paths_.clear();
}

}  // namespace cc
