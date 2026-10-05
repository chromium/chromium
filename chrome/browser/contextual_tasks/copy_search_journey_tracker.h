// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

// Ephemeral in-memory record of a text copy (or context-menu selection) on a
// tab. Plaintext is never retained.
struct CopyRecord {
  base::TimeTicks timestamp;
  size_t normalized_query_hash = 0;
  SessionID source_tab_id = SessionID::InvalidValue();
  int source_nav_entry_id = 0;
  uint64_t nonce = 0;
};

// Correlated state of a cross-tab copy-to-search journey, keyed by the
// originating tab's SessionID.
struct JourneyState {
  CopyRecord copy_record;
  SessionID search_tab_id = SessionID::InvalidValue();
  base::TimeTicks search_committed_time;
};

// Profile-keyed service that keeps a bounded volatile ring buffer of recent
// text copy hashes and correlates them with Google default-search-provider
// queries committed in other tabs.
class CopySearchJourneyTracker : public KeyedService,
                                 public TemplateURLServiceObserver {
 public:
  explicit CopySearchJourneyTracker(TemplateURLService* template_url_service);
  CopySearchJourneyTracker(const CopySearchJourneyTracker&) = delete;
  CopySearchJourneyTracker& operator=(const CopySearchJourneyTracker&) = delete;
  ~CopySearchJourneyTracker() override;

  // KeyedService:
  void Shutdown() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Sanitizes `text` the same way the omnibox sanitizes pasted text
  // (`omnibox::SanitizeTextForPaste`), collapses remaining whitespace runs,
  // lower-cases the result, and returns its `base::FastHash`. Returns
  // `std::nullopt` if the feature is disabled or the normalized string is
  // shorter than `GetCopyTextJourneysMinQueryMatchLength()`.
  static std::optional<size_t> NormalizeForJourneyMatch(
      std::u16string_view text);

  // Records a copy or selection of `copied_text` on `source_tab_id` at
  // `source_nav_entry_id`. No-op if the default search provider is not Google,
  // `source_tab_id` is invalid, or `copied_text` is rejected by
  // `NormalizeForJourneyMatch`.
  void OnCopyRecorded(SessionID source_tab_id,
                      int source_nav_entry_id,
                      std::u16string_view copied_text);

  // Correlates committed `search_terms` on `search_tab_id` against the copy
  // ring buffer from newest to oldest, ignoring same-tab matches. No-op if the
  // default search provider is not Google.
  void OnSearchNavigationCommitted(SessionID search_tab_id,
                                   std::u16string_view search_terms);

  // Clears any copy records and active journeys associated with `tab_id`.
  void OnTabDestroyed(SessionID tab_id);

  size_t GetRingBufferSizeForTesting() const { return ring_buffer_.size(); }
  const std::map<SessionID, JourneyState>& GetActiveJourneysForTesting() const {
    return active_journeys_;
  }

 private:
  void ClearState();
  void EvictExpiredRecords();

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  std::deque<CopyRecord> ring_buffer_;

  // Armed journeys keyed by `source_tab_id`.
  std::map<SessionID, JourneyState> active_journeys_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_H_
