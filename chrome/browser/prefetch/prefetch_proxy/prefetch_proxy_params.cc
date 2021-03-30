// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/common/chrome_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

const char kIsolatedPrerenderLimitNSPSubresourcesCmdLineFlag[] =
    "isolated-prerender-max-subresource-per-prerender";

const char kIsolatedPrerenderEnableNSPCmdLineFlag[] =
    "isolated-prerender-nsp-enabled";

bool PrefetchProxyIsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatePrerenders);
}

GURL PrefetchProxyProxyHost() {
  // Command line overrides take priority.
  std::string cmd_line_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "isolated-prerender-tunnel-proxy");
  if (!cmd_line_value.empty()) {
    GURL cmd_line_url(cmd_line_value);
    if (cmd_line_url.is_valid()) {
      return cmd_line_url;
    }
    LOG(ERROR) << "--isolated-prerender-tunnel-proxy value is invalid";
  }

  GURL url(base::GetFieldTrialParamValueByFeature(features::kIsolatePrerenders,
                                                  "proxy_host"));
  if (url.is_valid() && url.SchemeIs(url::kHttpsScheme)) {
    return url;
  }
  return GURL("https://tunnel.googlezip.net/");
}

std::string PrefetchProxyProxyHeaderKey() {
  std::string header = base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerenders, "proxy_header_key");
  if (!header.empty()) {
    return header;
  }
  return "chrome-tunnel";
}

bool PrefetchProxyOnlyForLiteMode() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "lite_mode_only", true);
}

bool PrefetchProxyNoStatePrefetchSubresources() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kIsolatedPrerenderEnableNSPCmdLineFlag) ||
         base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "do_no_state_prefetch", false);
}

base::Optional<size_t> PrefetchProxyMaximumNumberOfPrefetches() {
  if (!PrefetchProxyIsEnabled()) {
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

base::Optional<size_t> PrefetchProxyMaximumNumberOfNoStatePrefetchAttempts() {
  if (!PrefetchProxyIsEnabled() ||
      !PrefetchProxyNoStatePrefetchSubresources()) {
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

size_t PrefetchProxyMainframeBodyLengthLimit() {
  return 1024 * base::GetFieldTrialParamByFeatureAsInt(
                    features::kIsolatePrerenders,
                    "max_mainframe_body_length_kb", 5 * 1024);
}

size_t PrefetchProxyMaximumNumberOfConcurrentPrefetches() {
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerenders, "max_concurrent_prefetches", 1));
}

base::TimeDelta PrefetchProxyProbeTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          features::kIsolatePrerendersMustProbeOrigin, "probe_timeout_ms",
          10 * 1000 /* 10 seconds */));
}

bool PrefetchProxyCloseIdleSockets() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "close_idle_sockets", true);
}

base::TimeDelta PrefetchProxyTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                             "prefetch_timeout_ms",
                                             10 * 1000 /* 10 seconds */));
}

bool PrefetchProxyProbingEnabled() {
  return base::FeatureList::IsEnabled(
      features::kIsolatePrerendersMustProbeOrigin);
}

bool PrefetchProxyCanaryCheckEnabled() {
  if (!base::FeatureList::IsEnabled(
          features::kIsolatePrerendersMustProbeOrigin)) {
    return false;
  }

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kIsolatePrerendersMustProbeOrigin, "do_canary", true);
}

GURL PrefetchProxyTLSCanaryCheckURL() {
  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "tls_canary_url"));
  if (url.is_valid()) {
    return url;
  }
  return GURL("http://tls.tunnel.check.googlezip.net/connect");
}

GURL PrefetchProxyDNSCanaryCheckURL() {
  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "dns_canary_url"));
  if (url.is_valid()) {
    return url;
  }
  return GURL("http://dns.tunnel.check.googlezip.net/connect");
}

base::TimeDelta PrefetchProxyCanaryCheckCacheLifetime() {
  return base::TimeDelta::FromHours(base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerendersMustProbeOrigin, "canary_cache_hours", 24));
}

bool PrefetchProxyMustHTTPProbeInsteadOfTLS() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kIsolatePrerendersMustProbeOrigin, "replace_tls_with_http",
      false);
}

size_t PrefetchProxyMaxSubresourcesPerPrerender() {
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

bool PrefetchProxyStartsSpareRenderer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             "isolated-prerender-start-spare-renderer") ||
         base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "start_spare_renderer", false);
}

bool PrefetchProxyUseSpeculationRules() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             "isolated-prerender-use-speculation-rules") ||
         base::GetFieldTrialParamByFeatureAsBool(
             features::kIsolatePrerenders, "use_speculation_rules", false);
}

bool PrefetchProxyShouldPrefetchPosition(size_t position) {
  std::string csv = base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerenders, "prefetch_positions");
  if (csv.empty()) {
    return true;
  }

  // Using a static set that is parsed from |csv| causes tests to fail when the
  // tests share the same process. This approach is faster than having to parse
  // each value as a number then check for contains.
  return base::Contains(base::SplitString(csv, ",", base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_NONEMPTY),
                        base::NumberToString(position));
}

base::TimeDelta PrefetchProxyMaxRetryAfterDelta() {
  int max_seconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerenders, "max_retry_after_duration_secs",
      1 * 60 * 60 * 24 * 7 /* 1 week */);
  return base::TimeDelta::FromSeconds(max_seconds);
}

bool PrefetchProxySendDecoyRequestForIneligiblePrefetch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "prefetch-proxy-never-send-decoy-requests-for-testing")) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "prefetch-proxy-always-send-decoy-requests-for-testing")) {
    return true;
  }

  double probability = base::GetFieldTrialParamByFeatureAsDouble(
      features::kIsolatePrerenders, "ineligible_decoy_request_probability",
      1.0);

  // Clamp to [0.0, 1.0].
  probability = std::max(0.0, probability);
  probability = std::min(1.0, probability);

  // RandDouble returns [0.0, 1.0) so don't use <= here since that may return
  // true when the probability is supposed to be 0 (i.e.: always false).
  return base::RandDouble() < probability;
}
