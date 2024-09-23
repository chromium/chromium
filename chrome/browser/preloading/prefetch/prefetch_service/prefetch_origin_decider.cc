// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"

#include <memory>
#include <vector>

#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// static
void PrefetchOriginDecider::RegisterPrefs(PrefRegistrySimple* registry) {
  // Some loss in this pref (especially following a browser crash) is well
  // tolerated and helps ensure the pref service isn't slammed.
  registry->RegisterDictionaryPref(prefetch::prefs::kRetryAfterPrefPath,
                                   PrefRegistry::LOSSY_PREF);
}

PrefetchOriginDecider::PrefetchOriginDecider(PrefService* pref_service,
                                             base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {
  DCHECK(pref_service);
  DCHECK(clock);

  LoadFromPrefs();
  if (ClearPastEntries()) {
    SaveToPrefs();
  }
}

PrefetchOriginDecider::~PrefetchOriginDecider() = default;

void PrefetchOriginDecider::OnBrowsingDataCleared() {
  origin_retry_afters_.clear();
  SaveToPrefs();
}

bool PrefetchOriginDecider::IsOriginOutsideRetryAfterWindow(
    const GURL& url) const {
  url::Origin origin = url::Origin::Create(url);

  auto iter = origin_retry_afters_.find(origin);
  if (iter == origin_retry_afters_.end()) {
    return true;
  }

  return iter->second < clock_->Now();
}

void PrefetchOriginDecider::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  // Ignore negative times.
  if (retry_after.is_negative()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.RetryAfter",
                             retry_after, base::Seconds(1), base::Days(7), 100);

  origin_retry_afters_.emplace(url::Origin::Create(url),
                               clock_->Now() + retry_after);
  SaveToPrefs();
}

void PrefetchOriginDecider::LoadFromPrefs() {
  origin_retry_afters_.clear();

  const base::Value::Dict& dictionary =
      pref_service_->GetDict(prefetch::prefs::kRetryAfterPrefPath);

  for (auto element : dictionary) {
    GURL url_origin(element.first);
    if (!url_origin.is_valid()) {
      // This may happen in the case of corrupted prefs, or otherwise. Handle
      // gracefully.
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    std::optional<base::Time> retry_after = base::ValueToTime(element.second);
    if (!retry_after) {
      // This may happen in the case of corrupted prefs, or otherwise. Handle
      // gracefully.
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    url::Origin origin = url::Origin::Create(url_origin);
    origin_retry_afters_.emplace(origin, retry_after.value());
  }
}

void PrefetchOriginDecider::SaveToPrefs() const {
  base::Value::Dict dictionary;
  for (const auto& element : origin_retry_afters_) {
    std::string key = element.first.GetURL().spec();
    base::Value value = base::TimeToValue(element.second);
    dictionary.Set(std::move(key), std::move(value));
  }
  pref_service_->SetDict(prefetch::prefs::kRetryAfterPrefPath,
                         std::move(dictionary));
}

bool PrefetchOriginDecider::ClearPastEntries() {
  std::vector<url::Origin> to_remove;
  for (const auto& entry : origin_retry_afters_) {
    if (entry.second < clock_->Now()) {
      to_remove.push_back(entry.first);
    }
  }

  if (to_remove.empty()) {
    return false;
  }

  for (const auto& rm : to_remove) {
    origin_retry_afters_.erase(rm);
  }
  return true;
}
