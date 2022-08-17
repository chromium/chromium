// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_RESULTS_CACHE_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_RESULTS_CACHE_H_

#include <map>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/autofill/core/common/signatures.h"

namespace url {
class Origin;
}  // namespace url

// A data structure that contains the signatures of forms that may trigger
// a FastCheckout flow on a given origin.
class FastCheckoutCapabilitiesResult {
 public:
  FastCheckoutCapabilitiesResult();
  explicit FastCheckoutCapabilitiesResult(
      base::span<const autofill::FormSignature> signatures);
  virtual ~FastCheckoutCapabilitiesResult();

  FastCheckoutCapabilitiesResult(const FastCheckoutCapabilitiesResult& other);

  bool SupportsForm(autofill::FormSignature form_signature) const;

 private:
  // The set of signatures supported. The number of entries is expected to be
  // `O(1)` and often zero.
  base::flat_set<autofill::FormSignature> form_signatures_;
};

// A cache of `CapabilitiesResult` entries that has both a maximum age and a
// maximum size.
class FastCheckoutCapabilitiesResultsCache {
 public:
  // The maximum number of cache entries.
  static constexpr size_t kMaxSize = 100u;
  // The lifetime of a cache entry - entries older than this are purged.
  static constexpr base::TimeDelta kLifetime = base::Minutes(10);

  FastCheckoutCapabilitiesResultsCache();
  virtual ~FastCheckoutCapabilitiesResultsCache();

  FastCheckoutCapabilitiesResultsCache(
      const FastCheckoutCapabilitiesResultsCache& other);

  // Adds a new `result` for `origin` to the cache. If the cache is already
  // full (i.e. it has `kMaxSize` entries), it removes the oldest entry. Does
  // nothing if an entry for `origin` already exists.
  void AddToCache(const url::Origin& origin,
                  const FastCheckoutCapabilitiesResult& result);

  // Returns whether an up-to-date entry for `origin` exists in the cache.
  bool ContainsOrigin(const url::Origin& origin);

  // Returns whether there is a cache entry that the form with `form_signature`
  // on `origin` is supported.
  bool ContainsTriggerForm(const url::Origin& origin,
                           autofill::FormSignature form_signature);

 private:
  // Removes the oldest cache entry. Assumes that the cache is non-empty.
  void RemoveOldestEntry();

  // Removes entries that are older than `kLifetime`.
  void RemoveStaleEntries();

  // The `CapabilitiesResult`s contained in the cache.
  std::map<url::Origin, FastCheckoutCapabilitiesResult> capabilities_;

  // The contained origins by their retrieval time. The container is ordered
  // ascendingly by retrieval time.
  base::queue<std::pair<url::Origin, base::TimeTicks>> retrieval_times_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_RESULTS_CACHE_H_
