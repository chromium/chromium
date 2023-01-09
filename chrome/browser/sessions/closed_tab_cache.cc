// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

// The number of entries the ClosedTabCache can hold.
static constexpr size_t kClosedTabCacheLimit = 1;

// The default time to live in seconds for entries in the ClosedTabCache.
static constexpr base::TimeDelta kDefaultTimeToLiveInClosedTabCacheInSeconds =
    base::Seconds(15);

// The memory pressure level from which we should evict all entries from the
// cache to preserve memory.
// TODO(https://crbug.com/1119368): Integrate memory pressure logic with
// PerformanceManager.
static constexpr base::MemoryPressureListener::MemoryPressureLevel
    kClosedTabCacheMemoryPressureThreshold =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
}  // namespace

ClosedTabCache::Entry::Entry(SessionID id,
                             std::unique_ptr<content::WebContents> wc,
                             base::TimeTicks timestamp)
    : id(id), web_contents(std::move(wc)), tab_closure_timestamp(timestamp) {}
ClosedTabCache::Entry::~Entry() = default;

ClosedTabCache::ClosedTabCache()
    : cache_size_limit_(kClosedTabCacheLimit),
      task_runner_(
          content::GetUIThreadTaskRunner(content::BrowserTaskTraits())) {
  listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&ClosedTabCache::OnMemoryPressure,
                                     base::Unretained(this)));
}
ClosedTabCache::~ClosedTabCache() = default;

bool ClosedTabCache::CanCacheWebContents(absl::optional<SessionID> id) {
  TRACE_EVENT0("browser", "ClosedTabCache::CanCacheWebContents");

  // Only store if the kClosedTabCache feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kClosedTabCache))
    return false;

  // Only store if tab has valid session id associated with it.
  if (!id.has_value() || !id.value().is_valid())
    return false;

  // If the current memory pressure exceeds the threshold, we should not cache
  // any WebContents. `memory_pressure_level_` is initialized to
  // MEMORY_PRESSURE_LEVEL_NONE and will only be updated if the feature gets
  // enabled, thus this branch won't be taken if the feature is disabled.
  if (memory_pressure_level_ >= kClosedTabCacheMemoryPressureThreshold)
    return false;

  // For all other cases, you can store the tab in ClosedTabCache.
  return true;
}

void ClosedTabCache::CacheWebContents(
    std::pair<absl::optional<SessionID>, std::unique_ptr<content::WebContents>>
        cached) {
  TRACE_EVENT0("browser", "ClosedTabCache::CacheWebContents");

  DCHECK(CanCacheWebContents(cached.first));
  auto entry = std::make_unique<Entry>(
      cached.first.value(), std::move(cached.second), base::TimeTicks::Now());
  // TODO(https://crbug.com/1117377): Add a WebContents::SetInClosedTabCache()
  // method to replace freezing the page.
  entry->web_contents->WasHidden();
  DCHECK_EQ(content::Visibility::HIDDEN, entry->web_contents->GetVisibility());
  entry->web_contents->SetPageFrozen(/*frozen=*/true);
  StartEvictionTimer(entry.get());

  entries_.push_front(std::move(entry));

  // Evict least recently used tab if the ClosedTabCache is full.
  if (entries_.size() > cache_size_limit_) {
    entries_.pop_back();
  }
}

std::unique_ptr<content::WebContents> ClosedTabCache::RestoreEntry(
    SessionID id) {
  TRACE_EVENT1("browser", "ClosedTabCache::RestoreEntry", "SessionID", id.id());
  auto matching_entry = base::ranges::find(entries_, id, &Entry::id);

  if (matching_entry == entries_.end())
    return nullptr;

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  entries_.erase(matching_entry);
  entry->web_contents->SetPageFrozen(/*frozen=*/false);
  // TODO: Dispatch pageshow() after unfreezing.

  return std::move(entry->web_contents);
}

const content::WebContents* ClosedTabCache::GetWebContents(SessionID id) const {
  auto matching_entry = base::ranges::find(entries_, id, &Entry::id);

  if (matching_entry == entries_.end())
    return nullptr;

  return (*matching_entry).get()->web_contents.get();
}

base::TimeDelta ClosedTabCache::GetTimeToLiveInClosedTabCache() {
  // We use the following order of priority if multiple values exist:
  // - The programmatical value set in params. Used in specific tests.
  // - Infinite if kClosedTabCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInClosedTabCacheInSeconds.
  if (base::FeatureList::IsEnabled(kClosedTabCacheNoTimeEviction) &&
      GetFieldTrialParamValueByFeature(
          features::kClosedTabCache,
          "time_to_live_in_closed_tab_cache_in_seconds")
          .empty()) {
    return base::TimeDelta::Max();
  }

  return base::Seconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kClosedTabCache, "time_to_live_in_closed_tab_cache_in_seconds",
      kDefaultTimeToLiveInClosedTabCacheInSeconds.InSeconds()));
}

void ClosedTabCache::SetCacheSizeLimitForTesting(size_t limit) {
  cache_size_limit_ = limit;
}

void ClosedTabCache::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

bool ClosedTabCache::IsEmpty() {
  return entries_.empty();
}

// static
bool ClosedTabCache::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kClosedTabCache);
}

size_t ClosedTabCache::EntriesCount() {
  return entries_.size();
}

void ClosedTabCache::StartEvictionTimer(Entry* entry) {
  base::TimeDelta evict_after = GetTimeToLiveInClosedTabCache();
  entry->eviction_timer.SetTaskRunner(task_runner_);
  entry->eviction_timer.Start(
      FROM_HERE, evict_after,
      base::BindOnce(&ClosedTabCache::EvictEntryById, base::Unretained(this),
                     entry->id));
}

void ClosedTabCache::EvictEntryById(SessionID id) {
  auto matching_entry = base::ranges::find(entries_, id, &Entry::id);

  if (matching_entry == entries_.end())
    return;

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  entries_.erase(matching_entry);
}

void ClosedTabCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (!base::FeatureList::IsEnabled(kClosedTabCacheMemoryPressure)) {
    // Don't flush entries if MemoryPressure is disabled for ClosedTabCache.
    return;
  }

  if (memory_pressure_level_ != level)
    memory_pressure_level_ = level;

  if (memory_pressure_level_ >= kClosedTabCacheMemoryPressureThreshold)
    Flush();
}

void ClosedTabCache::Flush() {
  TRACE_EVENT0("browser", "ClosedTabCache::Flush");
  entries_.clear();
}
