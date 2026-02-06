// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ORIGIN_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ORIGIN_CHECKER_H_

#include "base/types/optional_ref.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace actor {

class OriginChecker {
 public:
  OriginChecker();
  ~OriginChecker();

  // Returns true iff navigation to `destination_origin` is allowed, either due
  // to Same-Origin-Policy or by a previous call to `AllowNavigation`.
  //
  // Must not be called if `actor::IsNavigationGatingEnabled()` returns false.
  bool IsNavigationAllowed(
      base::optional_ref<const url::Origin> initiator_origin,
      const url::Origin& destination_origin) const;

  // Returns true iff navigation to or interaction with `origin` has been
  // allowed (via a previous call to `AllowNavigationTo`) with
  // `is_user_confirmed` set to true.
  bool IsNavigationConfirmedByUser(const url::Origin& origin) const;

  // Adds the given origin to the set of origins to which the actor is allowed
  // to navigate. `is_user_confirmed` controls whether
  // `IsNavigationConfirmedByUser` will return true for this origin in the
  // future.
  void AllowNavigationTo(url::Origin origin, bool is_user_confirmed);
  // Adds the given origins to the set of origins to which the actor is allowed
  // to navigate. The origins are considered non-user-confirmed.
  void AllowNavigationTo(const absl::flat_hash_set<url::Origin>& origins);

  // Records histograms with size metrics for each set of origins. Callers
  // should ensure that this method is only called in cases that are documented
  // in the histograms' entries in histograms.xml.
  void RecordSizeMetrics() const;

 private:
  struct OriginState {
    // Whether the user has explicitly confirmed navigation to this origin.
    bool is_user_confirmed = false;
  };

  // The set of origins which the browser is allowed to navigate to under actor
  // control. Note that presence in this map does *not* imply that the actor may
  // navigate without confirming with the user first. This set can have origins
  // added to it by the server actions or by confirming the new origin with the
  // model or user. Sensitive origins that are on the optimization guide
  // blocklist are not exempt by this set.
  absl::flat_hash_map<url::Origin, OriginState> allowed_navigation_origins_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ORIGIN_CHECKER_H_
