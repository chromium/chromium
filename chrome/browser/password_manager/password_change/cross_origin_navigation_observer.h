// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents_observer.h"

namespace affiliations {
class AffiliationService;
}

class CrossOriginNavigationObserver : public content::WebContentsObserver {
 public:
  CrossOriginNavigationObserver(
      content::WebContents* web_contents,
      affiliations::AffiliationService* affiliation_service,
      base::OnceClosure on_cross_origin_navigation_detected);
  ~CrossOriginNavigationObserver() override;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  bool IsSameOrAffiliatedDomain(const GURL& url) const;

 private:
  void OnPSLExtensionsReceived(const GURL& initial_url,
                               std::vector<std::string> psl_extension_list);
  void OnAffiliationsReceived(std::vector<std::string> affiliations);

  void OnReady();

  const raw_ptr<content::WebContents> web_contents_;

  // PSL extension list. Necessary for converting `GURL`s into eTLD+1.
  base::flat_set<std::string> psl_extension_list_;

  // Domains affiliated with a domain which was used during initialization of
  // this class. Includes original domain as well.
  base::flat_set<std::string> affiliated_domains_;

  // Callback which is invoked when cross origin navigation is detected.
  base::OnceClosure on_cross_origin_navigation_detected_;

  base::WeakPtrFactory<CrossOriginNavigationObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CROSS_ORIGIN_NAVIGATION_OBSERVER_H_
