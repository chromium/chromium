# Interaction to Next Paint

[Interaction to Next Paint](https://web.dev/inp) is an experimental metric that
reports the 98th percentile interaction duration over the course of a page view.
We are evaluating it in the hopes that it one day replaces
[First Input Delay](https://web.dev/fid)

This document details:
* [Where it is computed in the renderer](#Computation-in-Renderer)
* [How it is reported in trace events and web performance APIs](#Reporting-in-web-performance-APIs-and-trace-events)
* [How values from different frames are merged](#Merging-multiple-frames)
* [How it is reported to UKM/UMA](#Reporting-in-UKM-and-UMA)

## Computation in Renderer

Individual interactions are timed in the renderer. To understand interactions
better, it might be helpful to start with the  documentation on how interactions
are defined in our
[blog post about defining the metric](https://web.dev/better-responsiveness-metric/#group-events-into-interactions).

The code works like this:

* Interactions are created in the [`EventTiming`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/event_timing)
  class. Its [`Create()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/event_timing.cc;l=95;drc=b41db61995ded8bd8ee37dfba0c09d7c17d78e55;bpv=1;bpt=1)
  method is called from event dispatch of various events.
* When the `EventTiming` destructor is called, it calls
  [`WindowPerformance::RegisterEventTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;drc=b41db61995ded8bd8ee37dfba0c09d7c17d78e55;bpv=1;bpt=1;l=381)
  which in turn registers a callback for the presentation timestamp.
* The presentation timestamp callback is
  [`WindowPerformance::ReportEventTimings()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=441?q=WindowPerformance::ReportEventTimings&ss=chromium)
  It does the following:
  * Sets the `duration` and other timestamps of the EventTiming
  * Calls
    [`WindowPerformance::SetInteractionIdAndRecordLatency`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=562;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1?q=WindowPerformance::ReportEventTimings&ss=chromium)
    which sets the interaction id.
  * Calls
    [`ResponsivenessMetrics::RecordUserInteractionUKM()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/responsiveness_metrics.cc;l=186;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
    which calculates the maximum interaction duration and calls
    [`LocalFrameClientImpl::DidObserveUserInteraction()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/local_frame_client_impl.cc;l=712;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
    which kicks off [marshalling the interaction data to the browser](../../passing_data_from_renderer_to_browser.md).
     * *Note that `ResponsivenessMetrics::RecordUserInteractionUKM()` records a
       **separate** `Responsiveness.UserInteraction` UKM for individual interactions;
       the INP UKM is logged in the browser as described below.*

The data for the responsiveness metric is in the
[`ResponsivenessMetrics`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/responsiveness_metrics.cc;l=186;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
class; tracing back from that class can help answer questions about individual
interactions.

## Reporting in web performance APIs and trace events

* The [Event Timing API](https://w3c.github.io/event-timing/) reports timings of
  individual events, and reports interactionIds for events tied to interactions.
  These timings are reported through
  [`WindowPerformance::ReportEventTimings()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=441?q=WindowPerformance::ReportEventTimings&ss=chromium).
* Event traces are available in a few places:
  * [`ResponsivenessMetrics::RecordUserInteractionUKM()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/responsiveness_metrics.cc;l=186;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
    emits a trace event `Responsiveness.Renderer.UserInteraction` in the
    category `devtools.timeline` for each interaction.
  * [`WindowPerformance::NotifyAndAddEventTimingBuffer`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=562;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1?q=WindowPerformance::ReportEventTimings&ss=chromium)
    emits a trace event `EventTiming` in the category `devtools.timeline` for
    each event reported by the EventTiming API.

## Merging multiple frames

Data about each interaction in the main frame and all subframes is sent from
renderer to browser via the page load metrics infrastructure.
[You can read more about the data flow here](../../passing_data_from_renderer_to_browser.md).

[`PageLoadMetricsUpdateDispatcher::UpdatePageInputTiming`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=726;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
calls
[`ResponsivenessMetricsNormalization::AddNewUserInteractionLatencies`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/responsiveness_metrics_normalization.cc;l=61;drc=1d8b1965b96c021ee069a3ebda38be7aaf8a5786;bpv=1;bpt=1)
which aggregates the interactions across all subframes and calculates the 98th
percentile latency which is reported as INP. Metrics from all frames are treated
equally in the calculation.

## Reporting in UKM and UMA

All Core Web Vitals UKM are reported via
[PageLoadMetricsObserver](/chrome/browser/page_load_metrics/observers/README.md).
This ensures consistent reporting of only main frames, excluding error pages,
etc.

UKM for INP are:
* Most navigations: `PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration`
* BFCache navigations: `HistoryNavigation.UserInteractionLatencyAfterBackForwardCacheRestore.HighPercentile2.MaxEventDuration`
* Prerender2 activations: `PrerenderPageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration`

UMA for INP are:
* Most navigations: `PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration`
* BFCache navigations: `PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration.AfterBackForwardCacheRestore`
* Prerender2 activations: `PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration.Prerender`
