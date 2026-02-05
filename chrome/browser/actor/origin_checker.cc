// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/origin_checker.h"

#include <algorithm>

#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_util.h"
#include "net/base/schemeful_site.h"

namespace actor {

namespace {
// Helper to determine if `reference_origin` being allowlisted allows navigation
// to `navigation_url`. See note on `kGlicNavigationGatingUseSiteNotOrigin`.
bool IsSameForNewOriginNavigationGating(const url::Origin& reference_origin,
                                        const GURL& navigation_url) {
  if (kGlicNavigationGatingUseSiteNotOrigin.Get()) {
    return net::SchemefulSite::IsSameSite(reference_origin.GetURL(),
                                          navigation_url);
  }

  return reference_origin.IsSameOriginWith(navigation_url);
}
}  // namespace

OriginChecker::OriginChecker() = default;
OriginChecker::~OriginChecker() = default;

bool OriginChecker::IsNavigationAllowed(
    base::optional_ref<const url::Origin> initiator_origin,
    const GURL& url) const {
  CHECK(IsNavigationGatingEnabled());

  return (initiator_origin &&
          IsSameForNewOriginNavigationGating(*initiator_origin, url)) ||
         std::ranges::any_of(
             allowed_navigation_origins_, [&](const auto& origin) {
               return IsSameForNewOriginNavigationGating(origin, url);
             });
}

bool OriginChecker::IsNavigationConfirmedByUser(
    const url::Origin& origin) const {
  return user_confirmed_origins_.contains(origin);
}

void OriginChecker::AllowNavigationTo(url::Origin origin,
                                      bool is_user_confirmed) {
  if (is_user_confirmed) {
    user_confirmed_origins_.insert(origin);
  }
  allowed_navigation_origins_.insert(std::move(origin));
}

void OriginChecker::AllowNavigationTo(
    const absl::flat_hash_set<url::Origin>& origins) {
  std::ranges::copy(origins, std::inserter(allowed_navigation_origins_,
                                           allowed_navigation_origins_.end()));
}

void OriginChecker::RecordSizeMetrics() const {
  RecordActorNavigationGatingListSize(allowed_navigation_origins_.size(),
                                      user_confirmed_origins_.size());
}

}  // namespace actor
