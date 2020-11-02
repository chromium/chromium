// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PREFETCH_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PREFETCH_METRICS_COLLECTOR_H_

#include <stdint.h>
#include <map>
#include <set>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_prefetch_status.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_probe_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

// Collects metrics on every prefetched resource (mainframes and subresources),
// for recording in UKM. The passed UKM source id is the source that the events
// will be logged to. UKM is recorded on destruction of this class.
class IsolatedPrerenderPrefetchMetricsCollector
    : public base::RefCounted<IsolatedPrerenderPrefetchMetricsCollector> {
 public:
  IsolatedPrerenderPrefetchMetricsCollector(
      base::TimeTicks navigation_start_time,
      ukm::SourceId ukm_source_id);

  // Called when a mainframe resource is not eligible for prefetching. Note that
  // if a mainframe is given here, |OnSubresourceNotEligible| is not expected to
  // be called.
  void OnMainframeResourceNotEligible(const GURL& url,
                                      size_t prediction_position,
                                      IsolatedPrerenderPrefetchStatus status);

  // Called when a subresource is not eligible to be prefetched.
  void OnSubresourceNotEligible(const GURL& mainframe_url,
                                const GURL& subresource_url,
                                IsolatedPrerenderPrefetchStatus status);

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
                                        IsolatedPrerenderProbeResult result);

  // Called when a subresource is reused from the cache after a mainframe is
  // navigated to.
  void OnCachedSubresourceUsed(const GURL& mainframe_url,
                               const GURL& subresource_url);

 private:
  friend class RefCounted<IsolatedPrerenderPrefetchMetricsCollector>;
  ~IsolatedPrerenderPrefetchMetricsCollector();

  // Helper method that makes a corresponding entry in
  // |subresources_by_mainframe_|.
  void MapMainframeToSubresource(const GURL& mainframe_url,
                                 const GURL& subresource_url);

  // Helper method that gets the link position of the given mainframe |url|.
  base::Optional<size_t> GetLinkPositionOfMainframe(const GURL& url) const;

  // Helper method that gets the status of the given mainframe |url|.
  base::Optional<IsolatedPrerenderPrefetchStatus> GetStatusOfMainframe(
      const GURL& url) const;

  // Represents a single resource that was prefetched.
  struct PrefetchMetric {
    PrefetchMetric();
    PrefetchMetric(const PrefetchMetric& copy);
    ~PrefetchMetric();

    IsolatedPrerenderPrefetchStatus status =
        IsolatedPrerenderPrefetchStatus::kPrefetchNotStarted;

    // Whether the resource is a mainframe or subresource.
    bool is_mainframe = false;

    // The position in the navigation prediction of this resource's mainframe
    // url.
    base::Optional<size_t> link_position;

    // The amount of data that transited the network for the resource.
    base::Optional<int64_t> data_length;

    // The time between the start of the navigation and the start of the
    // resource fetch.
    base::Optional<base::TimeDelta> navigation_start_to_fetch_start;

    // The time duration that it took to fetch the resource.
    base::Optional<base::TimeDelta> fetch_duration;

    // Set if the mainframe link was clicked.
    base::Optional<bool> was_clicked;

    // How this resource's page was filtered on navigation, if at all.
    base::Optional<IsolatedPrerenderProbeResult> filtering_result;
  };

  const base::TimeTicks navigation_start_time_;

  const ukm::SourceId ukm_source_id_;

  // Maps a mainframe url to all of its known subresources.
  std::map<GURL, std::set<GURL>> subresources_by_mainframe_;

  // Holds all the metrics that will be recorded, indexed by their url.
  std::map<GURL, PrefetchMetric> resources_by_url_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderPrefetchMetricsCollector);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PREFETCH_METRICS_COLLECTOR_H_
