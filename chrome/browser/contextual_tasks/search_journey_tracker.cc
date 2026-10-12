// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tracker.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/i18n/case_conversion.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "components/search/search.h"

namespace contextual_tasks {

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
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !source_tab_id.is_valid()) {
    return;
  }
  std::optional<size_t> hash = NormalizeForJourneyMatch(copied_text);
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

  ring_buffer_.push_back(std::move(record));
  const size_t max_ring_buffer_size = GetABASearchJourneysMaxRingBufferSize();
  while (ring_buffer_.size() > max_ring_buffer_size) {
    ring_buffer_.pop_front();
  }
}

void SearchJourneyTracker::OnSearchNavigationCommitted(
    SessionID search_tab_id,
    std::u16string_view search_terms) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !search_tab_id.is_valid()) {
    return;
  }
  std::optional<size_t> query_hash = NormalizeForJourneyMatch(search_terms);
  if (!query_hash.has_value()) {
    return;
  }

  EvictExpiredRecords();

  for (auto it = ring_buffer_.rbegin(); it != ring_buffer_.rend(); ++it) {
    if (it->normalized_query_hash != *query_hash) {
      continue;
    }
    if (it->source_tab_id == search_tab_id) {
      continue;
    }

    JourneyState journey;
    journey.copy_record = *it;
    journey.search_tab_id = search_tab_id;
    journey.search_committed_time = base::TimeTicks::Now();
    active_journeys_[it->source_tab_id] = std::move(journey);
    break;
  }
}

void SearchJourneyTracker::OnTabDestroyed(SessionID tab_id) {
  std::erase_if(ring_buffer_, [tab_id](const CopyRecord& record) {
    return record.source_tab_id == tab_id;
  });
  active_journeys_.erase(tab_id);
  std::erase_if(active_journeys_, [tab_id](const auto& pair) {
    return pair.second.search_tab_id == tab_id;
  });
}

void SearchJourneyTracker::ClearState() {
  ring_buffer_.clear();
  active_journeys_.clear();
}

void SearchJourneyTracker::EvictExpiredRecords() {
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta ttl = GetABASearchJourneysTtl();
  while (!ring_buffer_.empty() && now - ring_buffer_.front().timestamp > ttl) {
    ring_buffer_.pop_front();
  }
  std::erase_if(active_journeys_, [now, ttl](const auto& pair) {
    return now - pair.second.copy_record.timestamp > ttl;
  });
}

}  // namespace contextual_tasks
