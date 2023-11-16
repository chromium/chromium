Soft navigation metrics are top-level UKM metrics for
[soft navigations](https://developer.chrome.com/blog/soft-navigations-experiment/).
Currently they are LCP, CLS and INP. The difference is that page load metrics
are taken over the page load period or the whole page lifespan while soft
navigation metrics are taken over the period between two soft navigations.
Therefore, page load metrics are reported once at the end of the page lifecycle,
while soft navigation metrics are also reported every time a soft navigation
begins. There may be multiple soft navigation events during the entire page life
cycle.

# What they measure

The LCP of a soft navigation measures the time interval from the time when a
user interacts to start the soft navigation to the time when the largest image
or text element is painted to the screen.

Similarly the CLS of a soft navigation is the largest burst of layout shift scores
for every unexpected layout shift that occurs during the time interval from the
time when that soft navigation happens to the time when the next soft navigation
happens or the page is terminated.

The INP of a soft navigation is calculated the same way as the page load INP,
differing only in that the former reports over the duration from the time when
that soft navigation happens to the time when the next soft navigation happens
or the page is terminated.

# Where they are computed in the renderer

Soft navigation metrics are calculated the same way as their page load counterpart.
See details in docs on their page load counterparts [`LCP`](/chrome/browser/page_load_metrics/observers/core/LCP.md),
[`CLS`](/chrome/browser/page_load_metrics/observers/core/CLS.md) and
[`INP`](/chrome/browser/page_load_metrics/observers/core/INP.md).

For soft navigation LCP, when a user interaction is initiated,
[`SoftNavigationHeuristics`](/third_party/blink/renderer/core/timing/soft_navigation_heuristics.h)
will inform [`PaintTimingDetector`](/third_party/blink/renderer/core/paint/timing/paint_timing_detector.h)
to reset and restart recording LCP which may have been stopped by a previous user
interaction happens. When a subsequent soft navigation happens, the LCP candidate
by that time is reported as the LCP of the soft navigation.

For soft navigation CLS, when a soft navigation happens or the page terminates,
the layout shifts that are aggregated up to that point, are reported as the CLS
of the soft navigation.

For soft navigation INP, similarly, when a soft navigation happens or the page
terminates, the user interaction latencies that are aggregated up to that point,
are reported as the INP of the soft navigation.

# How they are passed from renderer to browser

The soft navigation metrics are sent over to browser via the same mojo interface
and method, with an additional data struct [`SoftNavigationMetrics`](/components/page_load_metrics/common/page_load_metrics.mojom).
The data flow is the same as that of page load metrics. See details in docs on
their page load counterparts, [`LCP`](/chrome/browser/page_load_metrics/observers/core/LCP.md),
[`CLS`](/chrome/browser/page_load_metrics/observers/core/CLS.md) and
[`INP`](/chrome/browser/page_load_metrics/observers/core/INP.md).

One difference is that, unlike page load metrics which are reported in the UKM
only once at the end of the page load life cycle, soft navigation metrics are
reported multiple times, therefore it has to be ensured that each soft navigation
metrics are reported correctly in their corresponding soft navigation event.
For example, The final LCP candidate that should be the LCP of a soft navigation
could be overwritten by an LCP candidate of a subsequent soft navigation on both
the renderer and the browser side. To prevent this scenario, buffering of individual
metric at both sides are bypassed.

# How they are handled in browser
Soft navigation metrics, once arrived at the browser side, are handled by the same
 logic as their page load counterparts.

On the browser side, soft navigation metrics, along with page load metrics, are
received in [`MetricsWebContentsObserver::UpdateTiming`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/metrics_web_contents_observer.cc;l=1134). The data is routed through
[`PageLoadTracker::UpdateMetrics`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_tracker.cc;l=1477) to
[`PageLoadMetricsUpdateDispatcher::UpdateMetrics`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=423).This is where page load metrics are dispatched and recorded into the UKM. Soft
navigation metrics are also dispatched here. Specifically, individual user
interaction latency which is used to calculate INP is added in
[`PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationIntervalResponsivenessMetrics`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=627). Individual layout shift which
is used to calculate CLS is added in [`PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationIntervalLayoutShift`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=617). In
[`PageLoadMetricsUpdateDispatcher::UpdateSoftNavigation`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=614), the
[`PageLoadTracker::OnSoftNavigationChanged`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_tracker.cc;l=1113) is invoked. In this
function, the LCP candidate is updated in
[`LargestContentfulPaintHandler::UpdateSoftNavigationLargestContentfulPaint`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.cc;l=269;). Also,
[`UkmPageLoadMetricsObserver::OnSoftNavigationUpdated`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.cc;l=767)
where the soft navigation metrics are recorded in the UKM is also invoked here.

Unlike page load metrics, soft navigation metrics are only reported for main frame,
as soft navigation itself is only supported in main frame. Therefore, there’s no
merging of main frame and subframe for soft navigation metrics.

# How they are recorded in the UKM.
In UKM schema, a top-level event that is parallel to the PageLoad event,
[`SoftNavigation`](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/ukm/ukm.xml;l=24049) is added.
Each soft navigation corresponds to one SoftNavigation event.
The UKM source id that is used to record metrics into this event is generated and
stored when a soft navigation gets committed in
[`PageLoadTracker::DidCommitSameDocumentNavigation`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_tracker.cc;l=658).

Soft navigation metrics are recorded into the corresponding soft navigation event
at the time when a subsequent soft navigation is
[reported to the browser](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.cc;l=782). To tell that a new soft navigation
happens from all other updates via the mojo method UpdateTiming, a
[soft navigation count](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/common/page_load_metrics.mojom;l=584) is used. If the count is larger than the
one kept in `PageLoadTracker`, the metrics would be recorded into UKM in
`RecordSoftNavigationMetrics`.

Note that CLS and INP also are recorded for the time
interval from the page lifecycle begining to the time when the first soft
navigation happens. The recording happens in
`RecordResponsivenessMetricsBeforeSoftNavigationForMainFrame`
and `RecordLayoutShiftBeforeSoftNavigationForMainFrame` respectively. They
are recorded in the corresponding page load event though. The UKM entries are
`LayoutInstabilityBeforeSoftNavigation.MaxCumulativeShiftScore.MainFrame.SessionWindow.Gap1000ms.Max5000ms`
and `InteractiveTimingBeforeSoftNavigation.UserInteractionLatency.HighPercentile2.MaxEventDuration`

The UKM entries for soft navigation metrics are
* LCP `PaintTiming.LargestContentfulPaint`
* CLS `LayoutInstability.MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms`
* INP `InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration`