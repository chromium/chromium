// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PREFETCH_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PREFETCH_METRICS_COLLECTOR_H_

#include <stdint.h>
#include <map>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_probe_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

// Collects metrics on every prefetched resource (mainframes and subresources),
// for recording in UKM. The passed UKM source id is the source that the events
// will be logged to. UKM is recorded on destruction of this class.
class PrefetchProxyPrefetchMetricsCollector
    : public base::RefCounted<PrefetchProxyPrefetchMetricsCollector> {
 public:
  PrefetchProxyPrefetchMetricsCollector(base::TimeTicks navigation_start_time,
                                        ukm::SourceId ukm_source_id);

  PrefetchProxyPrefetchMetricsCollector(
      const PrefetchProxyPrefetchMetricsCollector&) = delete;
  PrefetchProxyPrefetchMetricsCollector& operator=(
      const PrefetchProxyPrefetchMetricsCollector&) = delete;

  // Called when a mainframe resource is not eligible for prefetching. Note that
  // if a mainframe is given here, |OnSubresourceNotEligible| is not expected to
  // be called.
  void OnMainframeResourceNotEligible(const GURL& url,
                                      size_t prediction_position,
                                      PrefetchProxyPrefetchStatus status);

  // Called when a mainframe resource is not eligible for prefetching, but was
  // sent as a privacy decoy. Note that if a mainframe is given here,
  // |OnSubresourceNotEligible| is not expected to be called.
  void OnDecoyPrefetchComplete(
      const GURL& url,
      size_t prediction_position,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status);

  // Called when a subresource is not eligible to be prefetched.
  void OnSubresourceNotEligible(const GURL& mainframe_url,
                                const GURL& subresource_url,
                                PrefetchProxyPrefetchStatus status);

  // Called when the prefetch of a mainframe completes.
  void OnMainframeResourcePrefetched(
      const GURL& url,
      size_t prediction_position,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status);

  // Called when the prefetch of a subresource completes.
  void OnSubresourcePrefetched(
      const GURL& mainframe_url,
      const GURL& subresource_url,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status);

  // Called when the |url| is navigated to.
  void OnMainframeNavigatedTo(const GURL& url);

  // Called when the mainframe resource for |url| might be used from cache,
  // depending on |probe_success|.
  void OnMainframeNavigationProbeResult(const GURL& url,
                                        PrefetchProxyProbeResult result);

  // Called when a subresource is reused from the cache after a mainframe is
  // navigated to.
  void OnCachedSubresourceUsed(const GURL& mainframe_url,
                               const GURL& subresource_url);

 private:
  friend class base::RefCounted<PrefetchProxyPrefetchMetricsCollector>;
  ~PrefetchProxyPrefetchMetricsCollector();

  // Helper method that makes a corresponding entry in
  // |subresources_by_mainframe_|.
  void MapMainframeToSubresource(const GURL& mainframe_url,
                                 const GURL& subresource_url);

  // Helper method that gets the link position of the given mainframe |url|.
  absl::optional<size_t> GetLinkPositionOfMainframe(const GURL& url) const;

  // Helper method that gets the status of the given mainframe |url|.
  absl::optional<PrefetchProxyPrefetchStatus> GetStatusOfMainframe(
      const GURL& url) const;

  // Represents a single resource that was prefetched.
  struct PrefetchMetric {
    PrefetchMetric();
    PrefetchMetric(const PrefetchMetric& copy);
    ~PrefetchMetric();

    PrefetchProxyPrefetchStatus status =
        PrefetchProxyPrefetchStatus::kPrefetchNotStarted;

    // Whether the resource is a mainframe or subresource.
    bool is_mainframe = false;

    // The position in the navigation prediction of this resource's mainframe
    // url.
    absl::optional<size_t> link_position;

    // The amount of data that transited the network for the resource.
    absl::optional<int64_t> data_length;

    // The time between the start of the navigation and the start of the
    // resource fetch.
    absl::optional<base::TimeDelta> navigation_start_to_fetch_start;

    // The time duration that it took to fetch the resource.
    absl::optional<base::TimeDelta> fetch_duration;

    // Set if the mainframe link was clicked.
    absl::optional<bool> was_clicked;

    // How this resource's page was filtered on navigation, if at all.
    absl::optional<PrefetchProxyProbeResult> filtering_result;
  };

  const base::TimeTicks navigation_start_time_;

  const ukm::SourceId ukm_source_id_;

  // Maps a mainframe url to all of its known subresources.
  std::map<GURL, std::set<GURL>> subresources_by_mainframe_;

  // Holds all the metrics that will be recorded, indexed by their url.
  std::map<GURL, PrefetchMetric> resources_by_url_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PREFETCH_METRICS_COLLECTOR_H_
