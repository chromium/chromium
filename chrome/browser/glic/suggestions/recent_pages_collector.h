// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_COLLECTOR_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_COLLECTOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "url/gurl.h"

class Profile;

namespace history {
class HistoryService;
class QueryResults;
}  // namespace history

namespace glic {

// Collects the pages recently visited on this device, most recent first, as
// additional context for zero state suggestions. The source is selected by the
// kRecentPagesSource feature param: local history (default) or open tabs.
// Limits come from the kGlicBrowsingHistoryZeroStateSuggestions feature params.
class RecentPagesCollector {
 public:
  using Callback =
      base::OnceCallback<void(std::vector<optimization_guide::proto::Tab>)>;

  // `profile` must be non-null, not off-the-record, and must outlive `this`.
  explicit RecentPagesCollector(Profile* profile);
  RecentPagesCollector(const RecentPagesCollector&) = delete;
  RecentPagesCollector& operator=(const RecentPagesCollector&) = delete;
  ~RecentPagesCollector();

  // Asynchronously collects eligible recent pages from this device, ordered
  // most recent first and capped at `kMaxRecentPages`.
  //
  // A page is eligible if:
  //  - It was active/visited within `kMaxRecentDuration`.
  //  - It was visited on the local device and is known to sync (visits from
  //    other devices and visits not known to sync are excluded).
  //  - It has an HTTP(S) scheme and is not a New Tab Page (including remote
  //    or third-party search engine NTPs).
  //  - It was not a 404 or a blocked visit.
  //  - It is not in `page_context_urls` (e.g. currently focused or pinned tabs
  //    already included as page context).
  //
  // Pages sharing the same URL (ignoring fragments) are deduplicated, keeping
  // the most recent visit.
  //
  // When reading from history, returns an empty list if HistoryService is
  // unavailable or its backend is not yet loaded. `callback` is dropped if
  // `this` is destroyed before collection completes.
  void CollectRecentPages(const std::vector<GURL>& page_context_urls,
                          Callback callback);

 private:
  void OnHistoryQueryDone(const std::vector<GURL>& page_context_urls,
                          Callback callback,
                          history::QueryResults results);

  void CollectFromOpenTabs(const std::vector<GURL>& page_context_urls,
                           Callback callback);

  const raw_ptr<Profile> profile_;
  const raw_ptr<history::HistoryService> history_service_;

  base::CancelableTaskTracker task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RecentPagesCollector> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_COLLECTOR_H_
