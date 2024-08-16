// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace content_settings {
class CookieSettings;
}

class DIPSServiceImpl;
class GURL;

class RedirectHeuristicTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RedirectHeuristicTabHelper>,
      public RedirectChainDetector::Observer {
 public:
  ~RedirectHeuristicTabHelper() override;

  static std::set<std::string> AllSitesFollowingFirstParty(
      content::WebContents* web_contents,
      const GURL& first_party_url);

 private:
  explicit RedirectHeuristicTabHelper(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<RedirectHeuristicTabHelper>;

  // Record a RedirectHeuristic event for a cookie access, if eligible. This
  // applies when the tracking site has appeared previously in the current
  // redirect context.
  void MaybeRecordRedirectHeuristic(
      const ukm::SourceId& first_party_source_id,
      const content::CookieAccessDetails& details);
  void RecordRedirectHeuristic(
      const ukm::SourceId& first_party_source_id,
      const ukm::SourceId& third_party_source_id,
      const content::CookieAccessDetails& details,
      const size_t sites_passed_count,
      bool is_current_interaction,
      std::optional<base::Time> last_user_interaction_time);

  // Create all eligible RedirectHeuristic grants for the current redirect
  // chain. This may create a storage access grant for any site in the redirect
  // chain on the last committed site, if it meets the criteria.
  void CreateAllRedirectHeuristicGrants(const GURL& first_party_url);
  void CreateRedirectHeuristicGrant(const GURL& url,
                                    const GURL& first_party_url,
                                    base::TimeDelta grant_duration,
                                    bool has_interaction);

  // Start WebContentsObserver overrides:
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  // Start RedirectChainDetector::Observer overrides:
  void OnNavigationCommitted() override;

  raw_ptr<RedirectChainDetector> detector_;
  raw_ptr<DIPSServiceImpl> dips_service_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};
  std::optional<base::Time> last_commit_timestamp_;

  base::ScopedObservation<RedirectChainDetector,
                          RedirectChainDetector::Observer>
      obs_{this};
  base::WeakPtrFactory<RedirectHeuristicTabHelper> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_
