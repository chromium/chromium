// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"

#include "base/containers/adapters.h"
#include "base/functional/concurrent_closures.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace {

std::vector<std::string> ParseFacets(
    const affiliations::AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> domains;
  std::ranges::transform(results, std::back_inserter(domains),
                         [](const affiliations::Facet& facet) {
                           return affiliations::GetExtendedTopLevelDomain(
                               GURL(facet.uri.potentially_invalid_spec()), {});
                         });
  return domains;
}

}  // namespace

CrossOriginNavigationObserver::CrossOriginNavigationObserver(
    content::WebContents* web_contents,
    affiliations::AffiliationService* affiliation_service,
    base::OnceClosure on_cross_origin_navigation_detected)
    : web_contents_(web_contents),
      on_cross_origin_navigation_detected_(
          std::move(on_cross_origin_navigation_detected)) {
  base::ConcurrentClosures concurrent;
  affiliation_service->GetPSLExtensions(
      base::BindOnce(&CrossOriginNavigationObserver::OnPSLExtensionsReceived,
                     weak_ptr_factory_.GetWeakPtr(), web_contents_->GetURL())
          .Then(concurrent.CreateClosure()));
  affiliation_service->GetAffiliationsAndBranding(
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          web_contents_->GetURL().GetWithEmptyPath().spec()),
      base::BindOnce(&ParseFacets)
          .Then(base::BindOnce(
              &CrossOriginNavigationObserver::OnAffiliationsReceived,
              weak_ptr_factory_.GetWeakPtr()))
          .Then(concurrent.CreateClosure()));
  std::move(concurrent)
      .Done(base::BindOnce(&CrossOriginNavigationObserver::OnReady,
                           weak_ptr_factory_.GetWeakPtr()));
}

CrossOriginNavigationObserver::~CrossOriginNavigationObserver() = default;

void CrossOriginNavigationObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.entry) {
    return;
  }
  if (IsSameOrAffiliatedDomain(load_details.entry->GetURL())) {
    return;
  }

  CHECK(on_cross_origin_navigation_detected_);
  std::move(on_cross_origin_navigation_detected_).Run();
}

bool CrossOriginNavigationObserver::IsSameOrAffiliatedDomain(
    const GURL& url) const {
  return affiliated_domains_.contains(
      affiliations::GetExtendedTopLevelDomain(url, psl_extension_list_));
}

void CrossOriginNavigationObserver::OnPSLExtensionsReceived(
    const GURL& initial_url,
    std::vector<std::string> psl_extension_list) {
  psl_extension_list_ = base::flat_set<std::string>(
      std::make_move_iterator(psl_extension_list.begin()),
      std::make_move_iterator(psl_extension_list.end()));

  std::string main_domain =
      affiliations::GetExtendedTopLevelDomain(initial_url, psl_extension_list_);
  affiliated_domains_.insert(main_domain);
}

void CrossOriginNavigationObserver::OnAffiliationsReceived(
    std::vector<std::string> affiliations) {
  affiliated_domains_.insert_range(
      base::RangeAsRvalues(std::move(affiliations)));
}

void CrossOriginNavigationObserver::OnReady() {
  Observe(web_contents_);
}
