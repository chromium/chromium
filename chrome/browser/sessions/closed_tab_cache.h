// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_H_
#define CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_H_

#include <list>
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sessions/core/session_id.h"

namespace content {
class WebContents;
}  // namespace content

// ClosedTabCache:
//
// A browser feature implemented with the purpose of instantaneously restoring
// recently closed tabs. The main use case of ClosedTabCache is to immediately
// restore accidentally closed tabs.
//
// Main functionality:
// - stores WebContents instances uniquely identified by a SessionID.
// - evicts cache entries after a timeout.
// - evicts the least recently closed tab when the cache is full.
//
// TODO(aebacanu): Hook ClosedTabCache into the tab restore flow.
class ClosedTabCache {
 public:
  ClosedTabCache();
  ClosedTabCache(const ClosedTabCache&) = delete;
  ClosedTabCache& operator=(const ClosedTabCache&) = delete;
  ~ClosedTabCache();

  // Creates a ClosedTabCache::Entry from the given |id|, |wc| and |timestamp|.
  // Moves the entry into the ClosedTabCache and evicts one if necessary.
  void StoreEntry(SessionID id,
                  std::unique_ptr<content::WebContents> wc,
                  base::TimeTicks timestamp);

  // Moves a WebContents out of ClosedTabCache knowing its |id|. Returns nullptr
  // if none is found.
  std::unique_ptr<content::WebContents> RestoreEntry(SessionID id);

  // Returns a pointer to a cached WebContents whose entry is matching |id| if
  // it exists in the ClosedTabCache. Returns nullptr if no matching is found.
  const content::WebContents* GetWebContents(SessionID id) const;

  // We evict tabs from the ClosedTabCache after the time to live, which can be
  // controlled via experiment.
  static base::TimeDelta GetTimeToLiveInClosedTabCache();

  // Set a different cache size limit that is only used in tests.
  void SetCacheSizeLimitForTesting(size_t limit);

  // Inject a task runner for timing control within browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Whether the entries list is empty or not.
  bool IsEmpty();

  // Get the number of currently stored entries.
  size_t EntriesCount();

 private:
  struct Entry {
    Entry(SessionID id,
          std::unique_ptr<content::WebContents> wc,
          base::TimeTicks timestamp);
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    ~Entry();

    // The tab's unique SessionID used in TabRestoreService::Entry.
    SessionID id;

    // The tab being stored.
    std::unique_ptr<content::WebContents> web_contents;

    // Timestamp of tab closure.
    base::TimeTicks tab_closure_timestamp;

    base::OneShotTimer eviction_timer;
  };

  // Start the given entry's eviction timer.
  void StartEvictionTimer(Entry* entry);

  // Evict an entry from the ClosedTabCache based on the given SessionID. Does
  // nothing if the entry cannot be found.
  void EvictEntryById(SessionID id);

  // Flush the cache if memory is tight.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Evict all entries from the ClosedTabCache.
  void Flush();

  // The set of stored Entries.
  // Invariants:
  // - Ordered from the most recently closed tab to the least recently closed.
  // - Once the list is full, the least recently closed tab is evicted.
  std::list<std::unique_ptr<Entry>> entries_;

  size_t cache_size_limit_;

  // Task runner used for evicting cache entries after timeout.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Listener that sets up a callback to flush the cache if there is not enough
  // memory available.
  std::unique_ptr<base::MemoryPressureListener> listener_;
};

#endif  // CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_H_
