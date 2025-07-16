// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"

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
    const std::string& main_domain,
    const affiliations::AffiliatedFacets& results,
    bool success) {
  if (!success) {
    return {main_domain};
  }
  std::vector<std::string> domains = {main_domain};
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
    : on_cross_origin_navigation_detected_(
          std::move(on_cross_origin_navigation_detected)) {
  Observe(web_contents);
  std::string main_domain =
      affiliations::GetExtendedTopLevelDomain(web_contents->GetURL(), {});
  affiliated_domains_.insert(main_domain);

  affiliation_service->GetPSLExtensions(
      base::BindOnce(&CrossOriginNavigationObserver::OnPSLExtensionsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
  affiliation_service->GetAffiliationsAndBranding(
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          web_contents->GetURL().GetWithEmptyPath().spec()),
      base::BindOnce(&ParseFacets, main_domain)
          .Then(base::BindOnce(
              &CrossOriginNavigationObserver::OnAffiliationsReceived,
              weak_ptr_factory_.GetWeakPtr())));
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

bool CrossOriginNavigationObserver::IsSameOrAffiliatedDomain(const GURL& url) {
  return affiliated_domains_.contains(
      affiliations::GetExtendedTopLevelDomain(url, psl_extension_list_));
}

void CrossOriginNavigationObserver::OnPSLExtensionsReceived(
    std::vector<std::string> psl_extension_list) {
  psl_extension_list_ = base::flat_set<std::string>(
      std::make_move_iterator(psl_extension_list.begin()),
      std::make_move_iterator(psl_extension_list.end()));
}

void CrossOriginNavigationObserver::OnAffiliationsReceived(
    std::vector<std::string> affiliations) {
  affiliated_domains_ =
      base::flat_set<std::string>(std::make_move_iterator(affiliations.begin()),
                                  std::make_move_iterator(affiliations.end()));
}
