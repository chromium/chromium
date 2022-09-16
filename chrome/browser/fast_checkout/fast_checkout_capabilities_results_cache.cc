// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_results_cache.h"

#include <map>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/common/signatures.h"
#include "url/origin.h"

FastCheckoutCapabilitiesResult::FastCheckoutCapabilitiesResult() = default;

FastCheckoutCapabilitiesResult::FastCheckoutCapabilitiesResult(
    base::span<const autofill::FormSignature> signatures,
    bool supports_consentless_execution)
    : form_signatures_(signatures.begin(), signatures.end()),
      supports_consentless_execution_(supports_consentless_execution) {}

FastCheckoutCapabilitiesResult::~FastCheckoutCapabilitiesResult() = default;

FastCheckoutCapabilitiesResult::FastCheckoutCapabilitiesResult(
    const FastCheckoutCapabilitiesResult& other) = default;

bool FastCheckoutCapabilitiesResult::SupportsForm(
    autofill::FormSignature form_signature) const {
  return form_signatures_.contains(form_signature);
}

bool FastCheckoutCapabilitiesResult::SupportsConsentlessExecution() const {
  return supports_consentless_execution_;
}

FastCheckoutCapabilitiesResultsCache::FastCheckoutCapabilitiesResultsCache() =
    default;

FastCheckoutCapabilitiesResultsCache::~FastCheckoutCapabilitiesResultsCache() =
    default;

FastCheckoutCapabilitiesResultsCache::FastCheckoutCapabilitiesResultsCache(
    const FastCheckoutCapabilitiesResultsCache& other) = default;

void FastCheckoutCapabilitiesResultsCache::AddToCache(
    const url::Origin& origin,
    const FastCheckoutCapabilitiesResult& result) {
  RemoveStaleEntries();
  DCHECK(retrieval_times_.size() <= kMaxSize);
  if (retrieval_times_.size() == kMaxSize) {
    // TODO(crbug.com/1350456): Record a UMA metric to indicate cache overflow.
    RemoveOldestEntry();
  }

  if (ContainsOrigin(origin))
    return;

  capabilities_.emplace(origin, result);
  retrieval_times_.emplace(origin, base::TimeTicks::Now());
}

bool FastCheckoutCapabilitiesResultsCache::ContainsOrigin(
    const url::Origin& origin) {
  RemoveStaleEntries();
  return capabilities_.find(origin) != capabilities_.end();
}

bool FastCheckoutCapabilitiesResultsCache::ContainsTriggerForm(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  RemoveStaleEntries();
  std::map<url::Origin, FastCheckoutCapabilitiesResult>::iterator entry =
      capabilities_.find(origin);
  return (entry != capabilities_.end() &&
          entry->second.SupportsForm(form_signature));
}

bool FastCheckoutCapabilitiesResultsCache::SupportsConsentlessExecution(
    const url::Origin& origin) {
  RemoveStaleEntries();
  std::map<url::Origin, FastCheckoutCapabilitiesResult>::iterator entry =
      capabilities_.find(origin);
  return (entry != capabilities_.end() &&
          entry->second.SupportsConsentlessExecution());
}

void FastCheckoutCapabilitiesResultsCache::RemoveOldestEntry() {
  DCHECK(!retrieval_times_.empty());
  capabilities_.erase(retrieval_times_.front().first);
  retrieval_times_.pop();
}

void FastCheckoutCapabilitiesResultsCache::RemoveStaleEntries() {
  base::TimeTicks now = base::TimeTicks::Now();

  while (!retrieval_times_.empty() &&
         now - retrieval_times_.front().second > kLifetime) {
    RemoveOldestEntry();
  }
}
