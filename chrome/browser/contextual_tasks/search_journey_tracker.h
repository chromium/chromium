// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sessions/core/session_id.h"

class GURL;

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

// Coarse labels that Search can send about the query. Only used to slice
// metrics.
enum class AimPromoQueryHint {
  kEduHomework = 0,
  kEduConceptual = 1,
  kHowToWebApp = 2,
  kMinValue = kEduHomework,
  kMaxValue = kHowToWebApp,
};

using AimPromoQueryHints = base::EnumSet<AimPromoQueryHint,
                                         AimPromoQueryHint::kMinValue,
                                         AimPromoQueryHint::kMaxValue>;

// Parameters that a Google search results page registers so Chrome can later
// restore its AI Overview as the first turn of an AI Mode thread. Empty
// strings are omitted from the restore URL.
struct AimPromoParams {
  AimPromoParams();
  AimPromoParams(const AimPromoParams&);
  AimPromoParams& operator=(const AimPromoParams&);
  AimPromoParams(AimPromoParams&&);
  AimPromoParams& operator=(AimPromoParams&&);
  ~AimPromoParams();

  // Magi State Token. Required.
  std::string mstk;
  // How to handle the AIM handover.
  std::string aioh;
  // Where to restore the AIM thread from.
  std::string csuir;
  // XSRF token for the restore navigation.
  std::string sxsrf;
};

// An armed ABA journey: the user left an eligible source tab (Tab A) and a
// Google search committed in the tab they moved to (Tab B). Keyed by Tab A's
// `SessionID`.
struct JourneyState {
  JourneyState();
  JourneyState(const JourneyState&);
  JourneyState& operator=(const JourneyState&);
  JourneyState(JourneyState&&);
  JourneyState& operator=(JourneyState&&);
  ~JourneyState();

  // Tab A's `NavigationEntry` unique ID when the user left it.
  int source_nav_entry_id = 0;
  SessionID search_tab_id = SessionID::InvalidValue();
  // The search terms of Tab B's committed results page.
  std::u16string search_terms;
  base::TimeTicks search_committed_time;
  // Whether the query matches text copied, or selected for a context-menu
  // search, on Tab A. This only labels the journey; it never gates it.
  bool matched_copy = false;
  // Whether Tab B has closed. Closing Tab B usually re-activates Tab A, which
  // counts as a return, so the journey is kept.
  bool search_tab_closed = false;
  // Restore parameters registered by Tab B's results page for this query.
  std::optional<AimPromoParams> promo_params;
  // Labels registered along with `promo_params`.
  AimPromoQueryHints query_hints;
};

// Profile-keyed service that tracks ABA search journeys: the user leaves an
// eligible tab (Tab A), a Google default-search-provider search commits in the
// tab they switch to (Tab B), and the user then returns to Tab A. It also keeps
// a bounded volatile ring buffer of hashes of recently copied text, used only
// to label journeys whose query matches a copy made on Tab A.
class SearchJourneyTracker : public KeyedService,
                             public TemplateURLServiceObserver {
 public:
  explicit SearchJourneyTracker(TemplateURLService* template_url_service);
  SearchJourneyTracker(const SearchJourneyTracker&) = delete;
  SearchJourneyTracker& operator=(const SearchJourneyTracker&) = delete;
  ~SearchJourneyTracker() override;

  // KeyedService:
  void Shutdown() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Sanitizes `text` the same way the omnibox sanitizes pasted text
  // (`omnibox::SanitizeTextForPaste`), collapses remaining whitespace runs,
  // lower-cases the result, and returns its `base::FastHash`. Returns
  // `std::nullopt` if the feature is disabled or the normalized string is
  // shorter than `GetABASearchJourneysMinQueryMatchLength()`.
  static std::optional<size_t> NormalizeForJourneyMatch(
      std::u16string_view text);

  // Records a copy or selection of `copied_text` on `source_tab_id` at
  // `source_nav_entry_id`. No-op if the default search provider is not Google,
  // `source_tab_id` is invalid, or `copied_text` is rejected by
  // `NormalizeForJourneyMatch`.
  void OnCopyRecorded(SessionID source_tab_id,
                      int source_nav_entry_id,
                      std::u16string_view copied_text);

  // Called when `tab_id` stops being the tab the user is looking at. If `url`
  // is an HTTPS page that isn't a Google search results page, `tab_id` becomes
  // the candidate Tab A for the next search, at `nav_entry_id`.
  // `opener_tab_id` is the tab that opened `tab_id`, if any.
  void OnTabDeactivated(SessionID tab_id,
                        int nav_entry_id,
                        const GURL& url,
                        SessionID opener_tab_id);

  // Called when `tab_id` becomes the tab the user is looking at. Returns true
  // if `tab_id` is Tab A of an armed journey and is still on `nav_entry_id`,
  // i.e. the user has returned. A journey whose Tab A is now on another entry
  // is dropped. Unless intermediate tabs are allowed, activating a tab other
  // than Tab A or Tab B also drops the journey, so only A -> B -> A counts.
  bool OnTabActivated(SessionID tab_id, int nav_entry_id);

  // Called when a Google default-search-provider results page commits in
  // `search_tab_id` with `search_terms`. A commit for the same query as the
  // tab's journey, such as a same-document navigation, keeps the journey and
  // its restore parameters. Otherwise, drops any journey tied to an earlier
  // search in `search_tab_id`, and if `search_tab_id` is the tab the user is
  // looking at and they left a candidate Tab A to get there, arms a journey for
  // Tab A. A tab opened from `search_tab_id` is never Tab A. No-op if the
  // default search provider is not Google.
  void OnSearchNavigationCommitted(SessionID search_tab_id,
                                   std::u16string_view search_terms);

  // Called when the results page in `search_tab_id`, showing `search_terms`,
  // registers restore parameters for its AI Overview. Stores them on the
  // journey whose Tab B is `search_tab_id`, replacing any earlier ones, but
  // only if that journey is for the same query, so parameters from one search
  // are never paired with another search's query. Returns whether they were
  // stored.
  bool OnAimPromoParamsReceived(SessionID search_tab_id,
                                std::u16string_view search_terms,
                                AimPromoParams params,
                                AimPromoQueryHints query_hints);

  // Returns the journey whose Tab A is `source_tab_id` if its AI Overview can
  // be restored: the journey hasn't expired, Tab A is still on `nav_entry_id`,
  // and Tab B's results page registered restore parameters. Otherwise, returns
  // nullptr.
  const JourneyState* GetRestorableJourney(SessionID source_tab_id,
                                           int nav_entry_id) const;

  // Drops the journey whose Tab A is `source_tab_id` once its AI Overview has
  // been restored, so it can't be restored again.
  void OnJourneyRestored(SessionID source_tab_id);

  // Clears copy records for `tab_id` and any journey whose Tab A is `tab_id`.
  // Journeys whose Tab B is `tab_id` are kept and marked as closed.
  void OnTabDestroyed(SessionID tab_id);

  size_t GetRingBufferSizeForTesting() const { return ring_buffer_.size(); }
  const std::map<SessionID, JourneyState>& GetActiveJourneysForTesting() const {
    return active_journeys_;
  }

 private:
  // The tab the user most recently left that can be Tab A.
  struct SourceTab {
    SessionID tab_id = SessionID::InvalidValue();
    int nav_entry_id = 0;
    SessionID opener_tab_id = SessionID::InvalidValue();
    base::TimeTicks left_time;
  };

  void ClearState();
  void EvictExpiredRecords();

  // Returns whether `search_terms` matches a copy recorded on `tab_id`.
  bool MatchesCopyFromTab(SessionID tab_id,
                          std::u16string_view search_terms) const;

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  std::deque<CopyRecord> ring_buffer_;

  // The tab the user is looking at, if any.
  SessionID active_tab_id_ = SessionID::InvalidValue();

  std::optional<SourceTab> source_tab_;

  // Armed journeys keyed by Tab A's `SessionID`.
  std::map<SessionID, JourneyState> active_journeys_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TRACKER_H_
