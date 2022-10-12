# Cumulative Layout Shift

[Cumulative Layout Shift](https://web.dev/cls) is a
[Core Web Vital](https://web.dev/vitals) metric that reports the largest burst
of layout shift scores for every unexpected layout shift that occurs during the
entire lifespan of a page.

This document details:
* [Where it is computed in the renderer](#Computation-in-Renderer)
* [How it is reported in trace events and web performance APIs](#Reporting-in-web-performance-APIs-and-trace-events)
* [How values from different frames are merged](#Merging-multiple-frames)
* [How it is reported to UKM/UMA](#Reporting-in-UKM-and-UMA)

## Computation in Renderer

Individual layout shifts are computed in the
[`LayoutShiftTracker`](/third_party/blink/renderer/core/layout/layout_shift_tracker.cc)
class.

* The paint code calls `LayoutShiftTracker::NotifyBoxPrePaint()` and
  `LayoutShiftTracker::NotifyTextPrePaint()` during prepaint to notify the class
  of potential layout shifts.
* The paint code calls `LayoutShiftTracker::NotifyPrePaintFinished()` to notify
  the class when each frame's prepaint is finished so that the layout shift
  score can be reported.
* Layout shifts within 500ms of a user input should be ignored; other areas in
  the chromium codebase that could cause such inputs call methods like
  `LayoutShiftTracker::NotifyInput()` and `LayoutShiftTracker::NotifyScroll()`
  to inform the class of these user inputs.

### Debugging Renderer Computation

The `LayoutShiftTracker` class has off-by-default logging which can help explain
 the details of how layout shift computations are performed. You can use these
 command line arguments to chromium to see the logs:

`--enable-logging=stderr --vmodule=layout_shift*=1`

There is a lot more helpful debugging info in the document
[Debugging CLS](https://bit.ly/debug-cls).

## Reporting in web performance APIs and trace events

Individual layout shifts from the renderer are reported in the
`LayoutShiftTracker::ReportShift()` method.

* `LayoutShiftTracker::SubmitPerformanceEntry()` is called from `ReportShift`
  and calls [`WindowPerformance::AddLayoutShiftEntry()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=0;bpt=0;l=621)
  to add the layout shift entry to the performance timeline, which reports these
  shifts via the
  [Layout Instability API](https://wicg.github.io/layout-instability/).
* Individual trace events are reported from `ReportShift` to the `loading`
  category. Trace events have the name `LayoutShift`.
* `ReportShift` also calls
  [`LocalFrameClientImpl::DidObserveLayoutShift()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/local_frame_client_impl.cc;l=730;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=1;bpt=1?q=metrics_render_frame_observer.cc&ss=chromium%2Fchromium%2Fsrc)
  which kicks off reporting to the
  [PageLoadMetricsObserver](/chrome/browser/page_load_metrics/observers/README.md)
  so that the values can be reported in UMA and UKM. The reporting happens via a
  call to
  [`MetricsRenderFrameObserver::DidObserveLayoutShift()`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/metrics_render_frame_observer.cc;l=145;bpv=1;bpt=1?q=metrics_render_frame_observer.cc&ss=chromium%2Fchromium%2Fsrc)
  which then calls
  [`PageTimingMetricsSender::DidObserveLayoutShift`()](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/page_timing_metrics_sender.cc;l=103;drc=054e08864177603f17edbc111db7ebc8586906bd?q=page_timing_metrics_sender.h&ss=chromium%2Fchromium%2Fsrc)
  to ensure the data is sent via mojo IPC to the renderer.

## Merging multiple frames

In the renderer, individual frame layout shifts are assigned a weighting factor
based on the percent of the viewport used by the frame in
[`LayoutShiftTracker::SubframeWeightingFactor()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/layout_shift_tracker.cc;l=478;bpv=1;bpt=1).

Weighted layout shifts from each frame are then reported to the browser in
[`FrameRenderDataUpdate.new_layout_shifts`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/common/page_load_metrics.mojom;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=1;bpt=1;l=310). The
[`LayoutShiftNormalization`](/components/page_load_metrics/browser/layout_shift_normalization.cc)
class manages the normalization of individual layout shifts into windows, so
that the session window with the highest layout shifts can be reported. Layout
shifts are added to the class from
[`PageLoadMetricsUpdateDispatcher`](/components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc).

## Reporting in UKM and UMA

All Core Web Vitals UKM are reported via
[PageLoadMetricsObserver](/chrome/browser/page_load_metrics/observers/README.md).
This ensures consistent reporting of only main frames, excluding error pages,
etc.

UKM for CLS are:
* Most navigations:
  `PageLoad.LayoutInstability.MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms`
* BFCache navigations:
  `HistoryNavigation.MaxCumulativeShiftScoreAfterBackForwardCacheRestore.SessionWindow.Gap1000ms.Max5000ms`
* Prerender2 activations:
  `PrerenderPageLoad.LayoutInstability.MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms`

UMA for CLS are:
* Most navigations:
  `PageLoad.LayoutInstability.MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms`
* BFCache navigations:
  `PageLoad.LayoutInstability.MaxCumulativeShiftScore.AfterBackForwardCacheRestore.SessionWindow.Gap1000ms.Max5000ms`
* Prerender2 activations:
  `PageLoad.Clients.Prerender.LayoutInstability.MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms2.{SpeculationRule, Embedder_DirectURLInput, Embedder_DefaultSearchEngine}`
