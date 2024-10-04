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

struct UrlAndSourceId {
  GURL url;
  ukm::SourceId source_id;
};

// Properties of a redirect chain common to all the URLs within the chain.
struct DIPSRedirectChainInfo {
 public:
  DIPSRedirectChainInfo(const UrlAndSourceId& initial_url,
                        const UrlAndSourceId& final_url,
                        size_t length,
                        bool is_partial_chain);
  DIPSRedirectChainInfo(const DIPSRedirectChainInfo&);
  ~DIPSRedirectChainInfo();

  // A randomly-generated ID to associate redirects within the same chain for
  // metrics reporting.
  const int32_t chain_id;

  const UrlAndSourceId initial_url;
  // The eTLD+1 of initial_url, cached.
  const std::string initial_site;

  const UrlAndSourceId final_url;
  // The eTLD+1 of final_url, cached.
  const std::string final_site;

  // initial_site == final_site, cached.
  const bool initial_and_final_sites_same;
  const size_t length;
  // True if the chain is missing the end URL. This occurs when redirects are
  // trimmed from the front of the in-progress redirect chain.
  const bool is_partial_chain;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  std::optional<DIPSCookieMode> cookie_mode;
};

// Properties of one URL within a redirect chain.
struct DIPSRedirectInfo {
 public:
  // Constructor for server-side redirects.
  DIPSRedirectInfo(const UrlAndSourceId& url,
                   DIPSRedirectType redirect_type,
                   SiteDataAccessType access_type,
                   base::Time time,
                   bool was_response_cached,
                   int response_code,
                   base::TimeDelta server_bounce_delay);
  // Constructor for client-side redirects.
  DIPSRedirectInfo(const UrlAndSourceId& url,
                   DIPSRedirectType redirect_type,
                   SiteDataAccessType access_type,
                   base::Time time,
                   base::TimeDelta client_bounce_delay,
                   bool has_sticky_activation,
                   bool web_authn_assertion_request_succeeded);
  DIPSRedirectInfo(const UrlAndSourceId& url,
                   DIPSRedirectType redirect_type,
                   SiteDataAccessType access_type,
                   base::Time time,
                   base::TimeDelta client_bounce_delay,
                   bool has_sticky_activation,
                   bool web_authn_assertion_request_succeeded,
                   bool was_response_cached,
                   int response_code,
                   base::TimeDelta server_bounce_delay);
  DIPSRedirectInfo(const DIPSRedirectInfo&);
  ~DIPSRedirectInfo();

  // These properties are required for all redirects:

  const UrlAndSourceId url;
  const std::string site;  // the cached result of GetSiteForDIPS(url)
  const DIPSRedirectType redirect_type;
  SiteDataAccessType
      access_type;  // may be updated by late cookie notifications
  const base::Time time;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  std::optional<bool> has_interaction;
  std::optional<size_t> chain_index;
  // See DIPSRedirectChainInfo::chain_id.
  std::optional<int32_t> chain_id;
  std::optional<bool> has_3pc_exception;

  // The following properties are only applicable for client-side redirects:

  // For client redirects, the time between the previous page committing
  // and the redirect navigation starting. (For server redirects, zero)
  const base::TimeDelta client_bounce_delay;
  // For client redirects, whether the user ever interacted with the page during
  // this navigation.
  const bool has_sticky_activation;
  // For client redirects, whether the user ever triggered a web authn assertion
  // call.
  const bool web_authn_assertion_request_succeeded;

  // The following properties are only applicable for server-side redirects:
  const bool was_response_cached;
  const int response_code;
  const base::TimeDelta server_bounce_delay;
};

// a movable DIPSRedirectInfo, essentially
using DIPSRedirectInfoPtr = std::unique_ptr<DIPSRedirectInfo>;

// a movable DIPSRedirectChainInfo, essentially
using DIPSRedirectChainInfoPtr = std::unique_ptr<DIPSRedirectChainInfo>;

using DIPSRedirectChainHandler =
    base::RepeatingCallback<void(std::vector<DIPSRedirectInfoPtr>,
                                 DIPSRedirectChainInfoPtr)>;

#endif  // CHROME_BROWSER_DIPS_DIPS_REDIRECT_INFO_H_
