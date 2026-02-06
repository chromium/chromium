// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/origin_checker.h"

#include <algorithm>

#include "base/containers/map_util.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_util.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace actor {

namespace {
// Helper to determine if `reference_origin` being allowlisted allows navigation
// to `destination_origin`. See note on `kGlicNavigationGatingUseSiteNotOrigin`.
bool IsSameForNewOriginNavigationGating(const url::Origin& reference_origin,
                                        const url::Origin& destination_origin) {
  if (kGlicNavigationGatingUseSiteNotOrigin.Get()) {
    return net::SchemefulSite::IsSameSite(reference_origin, destination_origin);
  }

  return reference_origin.IsSameOriginWith(destination_origin);
}
}  // namespace

OriginChecker::OriginChecker() = default;
OriginChecker::~OriginChecker() = default;

bool OriginChecker::IsNavigationAllowed(
    base::optional_ref<const url::Origin> initiator_origin,
    const url::Origin& destination_origin) const {
  CHECK(IsNavigationGatingEnabled());

  return (initiator_origin && IsSameForNewOriginNavigationGating(
                                  *initiator_origin, destination_origin)) ||
         std::ranges::any_of(allowed_navigation_origins_,
                             [&](const auto& pair) {
                               const url::Origin& origin = pair.first;
                               return IsSameForNewOriginNavigationGating(
                                   origin, destination_origin);
                             });
}

bool OriginChecker::IsNavigationConfirmedByUser(
    const url::Origin& origin) const {
  const auto* state = base::FindOrNull(allowed_navigation_origins_, origin);
  return state && state->is_user_confirmed;
}

void OriginChecker::AllowNavigationTo(url::Origin origin,
                                      bool is_user_confirmed) {
  const auto [it, inserted] = allowed_navigation_origins_.emplace(
      origin, OriginState{is_user_confirmed});
  if (!inserted) {
    it->second.is_user_confirmed =
        it->second.is_user_confirmed || is_user_confirmed;
  }
}

void OriginChecker::AllowNavigationTo(
    const absl::flat_hash_set<url::Origin>& origins) {
  std::ranges::transform(
      origins,
      std::inserter(allowed_navigation_origins_,
                    allowed_navigation_origins_.end()),
      [](const auto& origin) {
        return std::make_pair(origin, OriginState{/*is_user_confirmed=*/false});
      });
}

void OriginChecker::RecordSizeMetrics() const {
  RecordActorNavigationGatingListSize(
      allowed_navigation_origins_.size(),
      std::ranges::count_if(allowed_navigation_origins_, [](const auto& pair) {
        return pair.second.is_user_confirmed;
      }));
}

}  // namespace actor
