# First Input Delay

[First Input Delay](https://web.dev/fid) is a [Core Web Vital](https://web.dev/vitals) metric that reports the delay between when the browser receives the first input event and when the renderer begins processing it. This is a measure of how much main thread business affects user interaction.

This document details:
* [Where it is computed in the renderer](#Computation-in-Renderer)
* [How it is reported in trace events and web performance APIs](#Reporting-in-web-performance-APIs-and-trace-events)
* [How values from different frames are merged](#Merging-multiple-frames)
* [How it is reported to UKM/UMA](#Reporting-in-UKM-and-UMA)

## Computation in Renderer

Each time an event is dispatched, the event dispatcher calls
[`EventTiming::Create()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/event_timing.cc;l=72;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=1;bpt=1)
which then calls the document's
[`InteractiveDetector::HandleForInputDelay()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/interactive_detector.cc;l=204;drc=054e08864177603f17edbc111db7ebc8586906bd),
which checks if this is the first event and stores the delay if so.

The `HandleForInputDelay` method calls
[`DocumentLoader::DidChangePerformanceTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/document_loader.cc;drc=977dc02c431b4979e34c7792bc3d646f649dacb4;l=757)
which eventually causes
[`MetricsRenderFrameObserver::DidChangePerformanceTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/metrics_render_frame_observer.cc;l=110?q=DidChangePerformanceTiming&ss=chromium%2Fchromium%2Fsrc&start=11)
to be called, ensuring the data is sent via mojo IPC to the browser, so that
[`PageLoadMetricsObserver`s](/chrome/browser/page_load_metrics/observers/README.md)
can merge frames and report the data to UKM.

## Reporting in web performance APIs and trace events

* First Input Delay is computed in the
  [Event Timing API](https://wicg.github.io/event-timing/) by subtracting the
  first event's `processingStart` from its `startTime`. An `EventTiming` object
  is created when the event dispatcher calls
  [`EventTiming::Create()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/event_timing.cc;l=72;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=1;bpt=1), and the full event
  latency is computed in its destructor, which then calls
  [`WindowPerformance::RegisterEventTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=379;drc=054e08864177603f17edbc111db7ebc8586906bd)
  so the data can be reported in the performance timeline.
* Trace events are only emitted for very long First Input Delays (greater than
  575ms) in `InteractiveDetector::HandlerForInputDelay()`.

## Merging multiple frames

If multiple frames on a page have user input, the delay for the first of these
inputs is reported as the first input delay. The merge occurs in
[`PageLoadTimingMerger::MergeInteractiveTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.cc;l=354;drc=054e08864177603f17edbc111db7ebc8586906bd).

## Reporting in UKM and UMA

All Core Web Vitals UKM are reported via
[PageLoadMetricsObserver](/chrome/browser/page_load_metrics/observers/README.md).
This ensures consistent reporting of only main frames, excluding error pages, etc.

UKM for FID are:
* Most navigations: `PageLoad.InteractiveTiming.FirstInputDelay4`
* BFCache navigations:
  `HistoryNavigation.FirstInputDelayAfterBackForwardCacheRestore`
* Prerender2 activations:
  `PrerenderPageLoad.InteractiveTiming.FirstInputDelay4`

UMA for FID are:
* Most navigations: `PageLoad.InteractiveTiming.FirstInputDelay4`
* BFCache navigations:
  `PageLoad.InteractiveTiming.FirstInputDelay.AfterBackForwardCacheRestore`
* Prerender2 activations:
  `PageLoad.Clients.Prerender.InteractiveTiming.FirstInputDelay4.{SpeculationRule, Embedder_DirectURLInput, Embedder_DefaultSearchEngine}`
