// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/contextual_tasks/public/features.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace contextual_tasks {

namespace {

// Added to Google searches whose query matches recently copied text. `1` means
// an exact match.
constexpr char kCopyMatchParam[] = "ictfc";

size_t CountWords(std::u16string_view text) {
  return base::SplitStringPiece(text, base::kWhitespaceUTF16,
                                base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)
      .size();
}

void RecordCueSliceMetrics(std::string_view interaction,
                           const JourneyState& journey) {
  const std::string prefix =
      base::StrCat({"ContextualTasks.SearchJourney.Cue.", interaction, "."});
  base::UmaHistogramBoolean(base::StrCat({prefix, "CopyMatch"}),
                            journey.matched_copy);
  base::UmaHistogramBoolean(base::StrCat({prefix, "ContextMenuSearch"}),
                            journey.is_context_menu_search);
  base::UmaHistogramBoolean(base::StrCat({prefix, "SearchTabClosed"}),
                            journey.search_tab_closed);
  base::UmaHistogramCounts100(
      base::StrCat({prefix, "WordCount"}),
      base::saturated_cast<int>(CountWords(journey.search_terms)));
  base::UmaHistogramLongTimes(
      base::StrCat({prefix, "SearchDwellTime"}),
      base::TimeTicks::Now() - journey.search_committed_time);
  if (journey.query_hints.empty()) {
    base::UmaHistogramEnumeration(base::StrCat({prefix, "QueryHint"}),
                                  AimPromoQueryHint::kNone);
  } else {
    for (AimPromoQueryHint hint : journey.query_hints) {
      base::UmaHistogramEnumeration(base::StrCat({prefix, "QueryHint"}), hint);
    }
  }
}

}  // namespace

AimPromoParams::AimPromoParams() = default;
AimPromoParams::AimPromoParams(const AimPromoParams&) = default;
AimPromoParams& AimPromoParams::operator=(const AimPromoParams&) = default;
AimPromoParams::AimPromoParams(AimPromoParams&&) = default;
AimPromoParams& AimPromoParams::operator=(AimPromoParams&&) = default;
AimPromoParams::~AimPromoParams() = default;

JourneyState::JourneyState() = default;
JourneyState::JourneyState(const JourneyState&) = default;
JourneyState& JourneyState::operator=(const JourneyState&) = default;
JourneyState::JourneyState(JourneyState&&) = default;
JourneyState& JourneyState::operator=(JourneyState&&) = default;
JourneyState::~JourneyState() = default;

SearchJourneyTracker::SearchJourneyTracker(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
  if (template_url_service_) {
    template_url_service_observation_.Observe(template_url_service_);
  }
}

SearchJourneyTracker::~SearchJourneyTracker() = default;

void SearchJourneyTracker::Shutdown() {
  template_url_service_observation_.Reset();
  template_url_service_ = nullptr;
  ClearState();
}

void SearchJourneyTracker::OnTemplateURLServiceChanged() {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_)) {
    ClearState();
  }
}

void SearchJourneyTracker::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
  template_url_service_ = nullptr;
  ClearState();
}

// static
std::optional<size_t> SearchJourneyTracker::NormalizeForJourneyMatch(
    std::u16string_view text) {
  if (!IsABASearchJourneysEnabled()) {
    return std::nullopt;
  }
  const std::u16string normalized =
      base::i18n::ToLower(base::CollapseWhitespace(
          omnibox::SanitizeTextForPaste(std::u16string(text)),
          /*trim_sequences_with_line_breaks=*/false));
  if (normalized.size() < GetABASearchJourneysMinQueryMatchLength()) {
    return std::nullopt;
  }
  return base::FastHash(base::as_byte_span(normalized));
}

void SearchJourneyTracker::OnCopyRecorded(SessionID source_tab_id,
                                          int source_nav_entry_id,
                                          std::u16string_view copied_text) {
  RecordCopy(source_tab_id, source_nav_entry_id, copied_text,
             /*is_context_menu_search=*/false);
}

void SearchJourneyTracker::OnContextMenuSearchRecorded(
    SessionID source_tab_id,
    int source_nav_entry_id,
    std::u16string_view selected_text) {
  RecordCopy(source_tab_id, source_nav_entry_id, selected_text,
             /*is_context_menu_search=*/true);
}

void SearchJourneyTracker::OnTabDeactivated(SessionID tab_id,
                                            int nav_entry_id,
                                            const GURL& url,
                                            SessionID opener_tab_id) {
  if (active_tab_id_ == tab_id) {
    active_tab_id_ = SessionID::InvalidValue();
  }
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !tab_id.is_valid()) {
    return;
  }
  if (!url.SchemeIs(url::kHttpsScheme) || google_util::IsGoogleSearchUrl(url)) {
    // The user left `tab_id` from a page that can't be Tab A.
    if (source_tab_ && source_tab_->tab_id == tab_id) {
      source_tab_.reset();
    }
    return;
  }
  source_tab_ = SourceTab{.tab_id = tab_id,
                          .nav_entry_id = nav_entry_id,
                          .opener_tab_id = opener_tab_id,
                          .left_time = base::TimeTicks::Now()};
}

bool SearchJourneyTracker::OnTabActivated(SessionID tab_id, int nav_entry_id) {
  if (!tab_id.is_valid()) {
    return false;
  }
  active_tab_id_ = tab_id;

  EvictExpiredRecords();

  const bool allow_intermediate_tabs =
      ShouldAllowABASearchJourneysIntermediateTabs();
  bool returned = false;
  std::erase_if(active_journeys_, [&](const auto& pair) {
    const auto& [source_tab_id, journey] = pair;
    if (source_tab_id == tab_id) {
      // Only a return to the page the user left counts.
      returned = journey.source_nav_entry_id == nav_entry_id;
      return !returned;
    }
    if (journey.search_tab_id == tab_id) {
      return false;
    }
    // Any other tab is an intermediate tab, as in A -> B -> C -> A.
    return !allow_intermediate_tabs;
  });
  if (returned) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.SearchJourney.ReturnedToSourceTab"));
  }
  return returned;
}

std::u16string SearchJourneyTracker::GetSearchTerms(const GURL& url) const {
  if (!template_url_service_ || !google_util::IsGoogleSearchUrl(url)) {
    return std::u16string();
  }
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  std::u16string search_terms;
  if (!default_provider ||
      !default_provider->ExtractSearchTermsFromURL(
          url, template_url_service_->search_terms_data(), &search_terms)) {
    return std::u16string();
  }
  return search_terms;
}

GURL SearchJourneyTracker::MaybeAppendCopyMatchParam(const GURL& url) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_)) {
    return url;
  }
  const std::optional<size_t> query_hash =
      NormalizeForJourneyMatch(GetSearchTerms(url));
  if (!query_hash.has_value()) {
    return url;
  }

  EvictExpiredRecords();
  if (std::ranges::none_of(ring_buffer_, [&](const CopyRecord& record) {
        return !record.is_context_menu_search &&
               record.normalized_query_hash == *query_hash;
      })) {
    return url;
  }
  return net::AppendOrReplaceQueryParameter(url, kCopyMatchParam, "1");
}

void SearchJourneyTracker::OnSearchNavigationCommitted(
    SessionID search_tab_id,
    std::u16string_view search_terms) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !search_tab_id.is_valid() || search_terms.empty()) {
    return;
  }

  EvictExpiredRecords();

  // A commit for the query the tab's journey is already for, such as a
  // same-document navigation or a reload, isn't a new search.
  if (std::ranges::any_of(active_journeys_, [&](const auto& pair) {
        return pair.second.search_tab_id == search_tab_id &&
               pair.second.search_terms == search_terms;
      })) {
    return;
  }

  // A new search in Tab B replaces the journey tied to its previous search.
  std::erase_if(active_journeys_, [search_tab_id](const auto& pair) {
    return pair.second.search_tab_id == search_tab_id;
  });

  // Only a search in the tab the user is looking at, after they left a
  // candidate Tab A, starts a journey. A tab opened from Tab B is never Tab A,
  // so opening a search result never counts as a return.
  if (search_tab_id != active_tab_id_ || !source_tab_ ||
      source_tab_->tab_id == search_tab_id ||
      source_tab_->opener_tab_id == search_tab_id) {
    return;
  }

  JourneyState journey;
  journey.source_nav_entry_id = source_tab_->nav_entry_id;
  journey.search_tab_id = search_tab_id;
  journey.search_terms = std::u16string(search_terms);
  journey.search_committed_time = base::TimeTicks::Now();
  if (const CopyRecord* matching_copy =
          FindMatchingCopyFromTab(source_tab_->tab_id, search_terms)) {
    journey.matched_copy = true;
    journey.is_context_menu_search = matching_copy->is_context_menu_search;
  }
  active_journeys_.insert_or_assign(source_tab_->tab_id, std::move(journey));
  base::RecordAction(
      base::UserMetricsAction("ContextualTasks.SearchJourney.Armed"));
}

bool SearchJourneyTracker::OnAimPromoParamsReceived(
    SessionID search_tab_id,
    std::u16string_view search_terms,
    AimPromoParams params,
    AimPromoQueryHints query_hints) {
  if (!search_tab_id.is_valid() || params.mstk.empty()) {
    return false;
  }

  EvictExpiredRecords();

  for (auto& [source_tab_id, journey] : active_journeys_) {
    if (journey.search_tab_id == search_tab_id && !journey.search_tab_closed &&
        journey.search_terms == search_terms) {
      journey.promo_params = std::move(params);
      journey.query_hints = query_hints;
      base::RecordAction(base::UserMetricsAction(
          "ContextualTasks.SearchJourney.PromoParamsReceived"));
      return true;
    }
  }
  return false;
}

const JourneyState* SearchJourneyTracker::GetRestorableJourney(
    SessionID source_tab_id,
    int nav_entry_id) const {
  auto it = active_journeys_.find(source_tab_id);
  if (it == active_journeys_.end()) {
    return nullptr;
  }
  const JourneyState& journey = it->second;
  if (base::TimeTicks::Now() - journey.search_committed_time >
          GetABASearchJourneysTtl() ||
      journey.source_nav_entry_id != nav_entry_id ||
      !journey.promo_params.has_value()) {
    return nullptr;
  }
  const size_t min_words_without_hint =
      GetABASearchJourneysMinWordsWithoutHint();
  if (min_words_without_hint > 0 && journey.query_hints.empty() &&
      CountWords(journey.search_terms) < min_words_without_hint) {
    return nullptr;
  }
  return &journey;
}

void SearchJourneyTracker::OnJourneyRestored(SessionID source_tab_id) {
  auto it = active_journeys_.find(source_tab_id);
  if (it == active_journeys_.end()) {
    return;
  }
  if (it->second.cue_shown) {
    base::RecordAction(
        base::UserMetricsAction("ContextualTasks.SearchJourney.Cue.Clicked"));
    RecordCueSliceMetrics("Clicked", it->second);
  }
  base::RecordAction(
      base::UserMetricsAction("ContextualTasks.SearchJourney.Restored"));
  active_journeys_.erase(it);
}

void SearchJourneyTracker::OnJourneyCueEligible(SessionID source_tab_id) {
  auto it = active_journeys_.find(source_tab_id);
  if (it != active_journeys_.end() && !it->second.eligible_recorded) {
    it->second.eligible_recorded = true;
    base::RecordAction(
        base::UserMetricsAction("ContextualTasks.SearchJourney.Eligible"));
    RecordCueSliceMetrics("Eligible", it->second);
  }
}

void SearchJourneyTracker::OnJourneyCueShown(SessionID source_tab_id,
                                             bool is_anchored_message) {
  auto it = active_journeys_.find(source_tab_id);
  if (it != active_journeys_.end() && !it->second.cue_shown) {
    it->second.cue_shown = true;
    base::RecordAction(
        base::UserMetricsAction("ContextualTasks.SearchJourney.Cue.Shown"));
    base::RecordAction(
        is_anchored_message
            ? base::UserMetricsAction(
                  "ContextualTasks.SearchJourney.Cue.Shown.AnchoredMessage")
            : base::UserMetricsAction(
                  "ContextualTasks.SearchJourney.Cue.Shown.Chip"));
    RecordCueSliceMetrics("Shown", it->second);
  }
}

void SearchJourneyTracker::OnTabDestroyed(SessionID tab_id) {
  std::erase_if(ring_buffer_, [tab_id](const CopyRecord& record) {
    return record.source_tab_id == tab_id;
  });
  active_journeys_.erase(tab_id);
  for (auto& [source_tab_id, journey] : active_journeys_) {
    if (journey.search_tab_id == tab_id) {
      journey.search_tab_closed = true;
    }
  }
  if (source_tab_ && source_tab_->tab_id == tab_id) {
    source_tab_.reset();
  }
  if (active_tab_id_ == tab_id) {
    active_tab_id_ = SessionID::InvalidValue();
  }
}

void SearchJourneyTracker::ClearState() {
  ring_buffer_.clear();
  source_tab_.reset();
  active_journeys_.clear();
}

void SearchJourneyTracker::EvictExpiredRecords() {
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta ttl = GetABASearchJourneysTtl();
  while (!ring_buffer_.empty() && now - ring_buffer_.front().timestamp > ttl) {
    ring_buffer_.pop_front();
  }
  if (source_tab_ && now - source_tab_->left_time > ttl) {
    source_tab_.reset();
  }
  std::erase_if(active_journeys_, [now, ttl](const auto& pair) {
    return now - pair.second.search_committed_time > ttl;
  });
}

void SearchJourneyTracker::RecordCopy(SessionID source_tab_id,
                                      int source_nav_entry_id,
                                      std::u16string_view text,
                                      bool is_context_menu_search) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !source_tab_id.is_valid()) {
    return;
  }
  std::optional<size_t> hash = NormalizeForJourneyMatch(text);
  if (!hash.has_value()) {
    return;
  }

  EvictExpiredRecords();

  CopyRecord record;
  record.timestamp = base::TimeTicks::Now();
  record.normalized_query_hash = *hash;
  record.source_tab_id = source_tab_id;
  record.source_nav_entry_id = source_nav_entry_id;
  record.nonce = base::RandUint64();
  record.is_context_menu_search = is_context_menu_search;

  ring_buffer_.push_back(std::move(record));
  const size_t max_ring_buffer_size = GetABASearchJourneysMaxRingBufferSize();
  while (ring_buffer_.size() > max_ring_buffer_size) {
    ring_buffer_.pop_front();
  }
}

const CopyRecord* SearchJourneyTracker::FindMatchingCopyFromTab(
    SessionID tab_id,
    std::u16string_view search_terms) const {
  const std::optional<size_t> query_hash =
      NormalizeForJourneyMatch(search_terms);
  if (!query_hash.has_value()) {
    return nullptr;
  }
  for (const CopyRecord& record : base::Reversed(ring_buffer_)) {
    if (record.source_tab_id == tab_id &&
        record.normalized_query_hash == *query_hash) {
      return &record;
    }
  }
  return nullptr;
}

}  // namespace contextual_tasks
