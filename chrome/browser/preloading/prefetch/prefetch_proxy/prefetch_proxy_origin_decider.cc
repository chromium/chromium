// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"

#include <memory>
#include <vector>

#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// static
void PrefetchProxyOriginDecider::RegisterPrefs(PrefRegistrySimple* registry) {
  // Some loss in this pref (especially following a browser crash) is well
  // tolerated and helps ensure the pref service isn't slammed.
  registry->RegisterDictionaryPref(prefetch::prefs::kRetryAfterPrefPath,
                                   PrefRegistry::LOSSY_PREF);
}

PrefetchProxyOriginDecider::PrefetchProxyOriginDecider(
    PrefService* pref_service,
    base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {
  DCHECK(pref_service);
  DCHECK(clock);

  LoadFromPrefs();
  if (ClearPastEntries()) {
    SaveToPrefs();
  }
}

PrefetchProxyOriginDecider::~PrefetchProxyOriginDecider() = default;

void PrefetchProxyOriginDecider::OnBrowsingDataCleared() {
  origin_retry_afters_.clear();
  SaveToPrefs();
}

bool PrefetchProxyOriginDecider::IsOriginOutsideRetryAfterWindow(
    const GURL& url) const {
  url::Origin origin = url::Origin::Create(url);

  auto iter = origin_retry_afters_.find(origin);
  if (iter == origin_retry_afters_.end()) {
    return true;
  }

  return iter->second < clock_->Now();
}

void PrefetchProxyOriginDecider::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  // Ignore negative times.
  if (retry_after.is_negative()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.RetryAfter",
                             retry_after, base::Seconds(1), base::Days(7), 100);

  // Cap values at a maximum per experiment.
  if (retry_after > PrefetchProxyMaxRetryAfterDelta()) {
    retry_after = PrefetchProxyMaxRetryAfterDelta();
  }

  origin_retry_afters_.emplace(url::Origin::Create(url),
                               clock_->Now() + retry_after);
  SaveToPrefs();
}

void PrefetchProxyOriginDecider::LoadFromPrefs() {
  origin_retry_afters_.clear();

  const base::Value::Dict& dictionary =
      pref_service_->GetDict(prefetch::prefs::kRetryAfterPrefPath);

  for (auto element : dictionary) {
    GURL url_origin(element.first);
    if (!url_origin.is_valid()) {
      // This may happen in the case of corrupted prefs, or otherwise. Handle
      // gracefully.
      NOTREACHED();
      continue;
    }

    absl::optional<base::Time> retry_after = base::ValueToTime(element.second);
    if (!retry_after) {
      // This may happen in the case of corrupted prefs, or otherwise. Handle
      // gracefully.
      NOTREACHED();
      continue;
    }

    url::Origin origin = url::Origin::Create(url_origin);
    origin_retry_afters_.emplace(origin, retry_after.value());
  }
}

void PrefetchProxyOriginDecider::SaveToPrefs() const {
  base::DictionaryValue dictionary;
  for (const auto& element : origin_retry_afters_) {
    std::string key = element.first.GetURL().spec();
    base::Value value = base::TimeToValue(element.second);
    dictionary.SetKey(std::move(key), std::move(value));
  }
  pref_service_->Set(prefetch::prefs::kRetryAfterPrefPath, dictionary);
}

bool PrefetchProxyOriginDecider::ClearPastEntries() {
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
