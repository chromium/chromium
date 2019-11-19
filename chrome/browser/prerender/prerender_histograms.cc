// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_histograms.h"

#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/prerender_util.h"
#include "components/google/core/common/google_util.h"
#include "net/http/http_cache.h"

namespace prerender {

namespace {

std::string GetHistogramName(Origin origin, const std::string& name) {
  return ComposeHistogramName(PrerenderHistograms::GetHistogramPrefix(origin),
                              name);
}

const char* FirstContentfulPaintHiddenName(bool was_hidden) {
  return was_hidden ? ".Hidden" : ".Visible";
}

}  // namespace

PrerenderHistograms::PrerenderHistograms() {}

std::string PrerenderHistograms::GetHistogramPrefix(Origin origin) {
  switch (origin) {
    case ORIGIN_OMNIBOX:
      return "omnibox";
    case ORIGIN_NONE:
      return "none";
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return "websame";
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return "webcross";
    case ORIGIN_EXTERNAL_REQUEST:
      return "externalrequest";
    case ORIGIN_LINK_REL_NEXT:
      return "webnext";
    case ORIGIN_GWS_PRERENDER:
      return "gws";
    case ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER:
      return "externalrequestforced";
    case ORIGIN_NAVIGATION_PREDICTOR:
      return "navigationpredictor";
    default:
      NOTREACHED();
      break;
  }

  // Dummy return value to make the compiler happy.
  return "none";
}

void PrerenderHistograms::RecordFinalStatus(
    Origin origin,
    FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(GetHistogramName(origin, "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(ComposeHistogramName("", "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
}

void PrerenderHistograms::RecordNetworkBytesConsumed(
    Origin origin,
    int64_t prerender_bytes,
    int64_t profile_bytes) const {
  const int kHistogramMin = 1;
  const int kHistogramMax = 100000000;  // 100M.
  const int kBucketCount = 50;

  UMA_HISTOGRAM_CUSTOM_COUNTS("Prerender.NetworkBytesTotalForProfile",
                              profile_bytes,
                              kHistogramMin,
                              1000000000,  // 1G
                              kBucketCount);

  if (prerender_bytes == 0)
    return;

  base::UmaHistogramCustomCounts(GetHistogramName(origin, "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
  base::UmaHistogramCustomCounts(ComposeHistogramName("", "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
}

void PrerenderHistograms::RecordPrefetchFirstContentfulPaintTime(
    Origin origin,
    bool is_no_store,
    bool was_hidden,
    base::TimeDelta time,
    base::TimeDelta prefetch_age) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!prefetch_age.is_zero()) {
    DCHECK_NE(origin, ORIGIN_NONE);
    base::UmaHistogramCustomTimes(GetHistogramName(origin, "PrefetchAge"),
                                  prefetch_age,
                                  base::TimeDelta::FromMilliseconds(10),
                                  base::TimeDelta::FromMinutes(30), 50);
  }

  std::string histogram_base_name;
  if (prefetch_age.is_zero()) {
    histogram_base_name = "PrefetchTTFCP.Reference";
  } else {
    histogram_base_name = prefetch_age < base::TimeDelta::FromMinutes(
                                             net::HttpCache::kPrefetchReuseMins)
                              ? "PrefetchTTFCP.Warm"
                              : "PrefetchTTFCP.Cold";
  }

  histogram_base_name += is_no_store ? ".NoStore" : ".Cacheable";
  histogram_base_name += FirstContentfulPaintHiddenName(was_hidden);
  std::string histogram_name = GetHistogramName(origin, histogram_base_name);

  base::UmaHistogramCustomTimes(histogram_name, time,
                                base::TimeDelta::FromMilliseconds(10),
                                base::TimeDelta::FromMinutes(2), 50);
}

}  // namespace prerender
