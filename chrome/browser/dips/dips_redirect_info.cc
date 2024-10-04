// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_redirect_info.h"

#include "base/rand_util.h"

DIPSRedirectChainInfo::DIPSRedirectChainInfo(const UrlAndSourceId& initial_url,
                                             const UrlAndSourceId& final_url,
                                             size_t length,
                                             bool is_partial_chain)
    : chain_id(static_cast<int32_t>(base::RandUint64())),
      initial_url(initial_url),
      initial_site(GetSiteForDIPS(initial_url.url)),
      final_url(final_url),
      final_site(GetSiteForDIPS(final_url.url)),
      initial_and_final_sites_same(initial_site == final_site),
      length(length),
      is_partial_chain(is_partial_chain) {}

DIPSRedirectChainInfo::DIPSRedirectChainInfo(const DIPSRedirectChainInfo&) =
    default;

DIPSRedirectChainInfo::~DIPSRedirectChainInfo() = default;

DIPSRedirectInfo::DIPSRedirectInfo(const UrlAndSourceId& url,
                                   DIPSRedirectType redirect_type,
                                   SiteDataAccessType access_type,
                                   base::Time time,
                                   bool was_response_cached,
                                   int response_code,
                                   base::TimeDelta server_bounce_delay)
    : DIPSRedirectInfo(url,
                       redirect_type,
                       access_type,
                       time,
                       /*client_bounce_delay=*/base::TimeDelta(),
                       /*has_sticky_activation=*/false,
                       /*web_authn_assertion_request_succeeded=*/false,
                       was_response_cached,
                       response_code,
                       server_bounce_delay) {
  // This constructor should only be called for server-side redirects;
  // client-side redirects should call the constructor with extra arguments.
  DCHECK_EQ(redirect_type, DIPSRedirectType::kServer);
}

DIPSRedirectInfo::DIPSRedirectInfo(const UrlAndSourceId& url,
                                   DIPSRedirectType redirect_type,
                                   SiteDataAccessType access_type,
                                   base::Time time,
                                   base::TimeDelta client_bounce_delay,
                                   bool has_sticky_activation,
                                   bool web_authn_assertion_request_succeeded)
    : DIPSRedirectInfo(url,
                       redirect_type,
                       access_type,
                       time,
                       client_bounce_delay,
                       has_sticky_activation,
                       web_authn_assertion_request_succeeded,
                       /*was_response_cached=*/false,
                       /*response_code=*/0,
                       /*server_bounce_delay=*/base::TimeDelta()) {
  // This constructor should only be called for client-side redirects.
  DCHECK_EQ(redirect_type, DIPSRedirectType::kClient);
}

DIPSRedirectInfo::DIPSRedirectInfo(const UrlAndSourceId& url,
                                   DIPSRedirectType redirect_type,
                                   SiteDataAccessType access_type,
                                   base::Time time,
                                   base::TimeDelta client_bounce_delay,
                                   bool has_sticky_activation,
                                   bool web_authn_assertion_request_succeeded,
                                   bool was_response_cached,
                                   int response_code,
                                   base::TimeDelta server_bounce_delay)
    : url(url),
      site(GetSiteForDIPS(url.url)),
      redirect_type(redirect_type),
      access_type(access_type),
      time(time),
      client_bounce_delay(client_bounce_delay),
      has_sticky_activation(has_sticky_activation),
      web_authn_assertion_request_succeeded(
          web_authn_assertion_request_succeeded),
      was_response_cached(was_response_cached),
      response_code(response_code),
      server_bounce_delay(server_bounce_delay) {}

DIPSRedirectInfo::DIPSRedirectInfo(const DIPSRedirectInfo&) = default;

DIPSRedirectInfo::~DIPSRedirectInfo() = default;
