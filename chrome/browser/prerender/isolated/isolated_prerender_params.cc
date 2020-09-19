// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/common/chrome_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

const char kIsolatedPrerenderLimitNSPSubresourcesCmdLineFlag[] =
    "isolated-prerender-max-subresource-per-prerender";

const char kIsolatedPrerenderEnableNSPCmdLineFlag[] =
    "isolated-prerender-nsp-enabled";

bool IsolatedPrerenderIsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatePrerenders);
}

bool IsolatedPrerenderNoStatePrefetchSubresources() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kIsolatedPrerenderEnableNSPCmdLineFlag) ||
         base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "do_no_state_prefetch", false);
}

base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches() {
  if (!IsolatedPrerenderIsEnabled()) {
    return 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "isolated-prerender-unlimited-prefetches")) {
    return base::nullopt;
  }

  int max = base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                                   "max_srp_prefetches", 1);
  if (max < 0) {
    return base::nullopt;
  }
  return max;
}

base::Optional<size_t>
IsolatedPrerenderMaximumNumberOfNoStatePrefetchAttempts() {
  if (!IsolatedPrerenderIsEnabled() ||
      !IsolatedPrerenderNoStatePrefetchSubresources()) {
    return 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "isolated-prerender-unlimited-nsp")) {
    return base::nullopt;
  }

  int max = base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                                   "max_nsp", 1);
  if (max < 0) {
    return base::nullopt;
  }
  return max;
}

size_t IsolatedPrerenderMainframeBodyLengthLimit() {
  return 1024 * base::GetFieldTrialParamByFeatureAsInt(
                    features::kIsolatePrerenders,
                    "max_mainframe_body_length_kb", 5 * 1024);
}

size_t IsolatedPrerenderMaximumNumberOfConcurrentPrefetches() {
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerenders, "max_concurrent_prefetches", 1));
}

base::TimeDelta IsolatedPrerenderProbeTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          features::kIsolatePrerendersMustProbeOrigin, "probe_timeout_ms",
          10 * 1000 /* 10 seconds */));
}

bool IsolatedPrerenderCloseIdleSockets() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "close_idle_sockets", true);
}

base::TimeDelta IsolatedPrefetchTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                             "prefetch_timeout_ms",
                                             10 * 1000 /* 10 seconds */));
}

bool IsolatedPrerenderProbingEnabled() {
  return base::FeatureList::IsEnabled(
      features::kIsolatePrerendersMustProbeOrigin);
}

bool IsolatedPrerenderCanaryCheckEnabled() {
  if (!base::FeatureList::IsEnabled(
          features::kIsolatePrerendersMustProbeOrigin)) {
    return false;
  }

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kIsolatePrerendersMustProbeOrigin, "do_canary", true);
}

GURL IsolatedPrerenderTLSCanaryCheckURL() {
  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "tls_canary_url"));
  if (url.is_valid()) {
    return url;
  }
  return GURL("http://tls.tunnel.check.googlezip.net/connect");
}

GURL IsolatedPrerenderDNSCanaryCheckURL() {
  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "dns_canary_url"));
  if (url.is_valid()) {
    return url;
  }
  return GURL("http://dns.tunnel.check.googlezip.net/connect");
}

base::TimeDelta IsolatedPrerenderCanaryCheckCacheLifetime() {
  return base::TimeDelta::FromHours(base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerendersMustProbeOrigin, "canary_cache_hours", 24));
}

bool IsolatedPrerenderMustHTTPProbeInsteadOfTLS() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kIsolatePrerendersMustProbeOrigin, "replace_tls_with_http",
      false);
}

size_t IsolatedPrerenderMaxSubresourcesPerPrerender() {
  std::string cmd_line_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kIsolatedPrerenderLimitNSPSubresourcesCmdLineFlag);
  size_t cmd_line_parsed;
  if (!cmd_line_value.empty() &&
      base::StringToSizeT(cmd_line_value, &cmd_line_parsed)) {
    return cmd_line_parsed;
  }

  return base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerenders, "max_subresource_count_per_prerender", 50);
}
