// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_REDIRECT_INFO_H_
#define CHROME_BROWSER_DIPS_DIPS_REDIRECT_INFO_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

#include "chrome/browser/dips/dips_utils.h"

// Properties of a redirect chain common to all the URLs within the chain.
struct DIPSRedirectChainInfo {
 public:
  DIPSRedirectChainInfo(const GURL& initial_url,
                        const GURL& final_url,
                        int length);
  ~DIPSRedirectChainInfo();

  const GURL initial_url;
  // The eTLD+1 of initial_url, cached.
  const std::string initial_site;
  const GURL final_url;
  // The eTLD+1 of final_url, cached.
  const std::string final_site;
  // initial_site == final_site, cached.
  const bool initial_and_final_sites_same;
  const int length;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  absl::optional<DIPSCookieMode> cookie_mode;
};

// Properties of one URL within a redirect chain.
struct DIPSRedirectInfo {
 public:
  // Constructor for server-side redirects.
  DIPSRedirectInfo(const GURL& url,
                   DIPSRedirectType redirect_type,
                   CookieAccessType access_type,
                   int index,
                   ukm::SourceId source_id,
                   base::Time time);
  // Constructor for client-side redirects.
  DIPSRedirectInfo(const GURL& url,
                   DIPSRedirectType redirect_type,
                   CookieAccessType access_type,
                   int index,
                   ukm::SourceId source_id,
                   base::Time time,
                   base::TimeDelta client_bounce_delay,
                   bool has_sticky_activation);
  ~DIPSRedirectInfo();

  // These properties are required for all redirects:

  const GURL url;
  const DIPSRedirectType redirect_type;
  CookieAccessType access_type;  // may be updated by late cookie notifications
  // Index of this URL within the overall chain.
  const int index;
  const ukm::SourceId source_id;
  const base::Time time;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  absl::optional<bool> has_interaction;

  // The following properties are only applicable for client-side redirects:

  // For client redirects, the time between the previous page committing
  // and the redirect navigation starting. (For server redirects, zero)
  const base::TimeDelta client_bounce_delay;
  // For client redirects, whether the user ever interacted with the page.
  const bool has_sticky_activation;
};

// a movable DIPSRedirectInfo, essentially
using DIPSRedirectInfoPtr = std::unique_ptr<DIPSRedirectInfo>;

// a movable DIPSRedirectChainInfo, essentially
using DIPSRedirectChainInfoPtr = std::unique_ptr<DIPSRedirectChainInfo>;

using DIPSRedirectChainHandler =
    base::RepeatingCallback<void(std::vector<DIPSRedirectInfoPtr>,
                                 DIPSRedirectChainInfoPtr)>;

#endif  // CHROME_BROWSER_DIPS_DIPS_REDIRECT_INFO_H_
