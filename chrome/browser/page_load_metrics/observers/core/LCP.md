# Largest Contentful Paint

[Largest Contentful Paint](https://web.dev/lcp) is a [Core Web Vital](https://web.dev/vitals) metric that reports the time until the largest image or text element is painted to the screen, as a heuristic for page load time. For [Prerender2](https://docs.google.com/document/d/1P2VKCLpmnNm_cRAjUeE-bqLL0bslL_zKqiNeCzNom_w/edit?usp=sharing), it measures the time of the activation start to the time when the activated page paints the largest content.

This document details:
* [Where it is computed in the renderer](#Computation-in-Renderer)
* [How it is reported in trace events and web performance APIs](#Reporting-in-web-performance-APIs-and-trace-events)
* [How values from different frames are merged](#Merging-multiple-frames)
* [How it is reported to UKM/UMA](#Reporting-in-UKM-and-UMA)

## Computation in Renderer

Since largest contentful paint can report either the largest text block or the
largest image, there are parallel classes in the renderer which track images and
text.

* [`PaintTimingDetector`](/third_party/blink/renderer/core/paint/timing/paint_timing_detector.h)
  provides common infrastructure between image and text paint tracking. Its
  `NotifyTextPaint` and `NotifyImagePaint` methods are called by the paint code
  as text and image paints occur. It tracks the largest image and text paint
  size and time, and calls
  [`DocumentLoader::DidChangePerformanceTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/document_loader.cc;l=639;drc=bff7fcba732aa420926466bf53dbd1d9504ba22f)
  which eventually results in a mojo IPC being scheduled in
  [`PageTimingMetricsSender::Update`()](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/page_timing_metrics_sender.cc;l=103;drc=054e08864177603f17edbc111db7ebc8586906bd;bpv=1;bpt=1?q=page_timing_metrics_sender.h&ss=chromium%2Fchromium%2Fsrc).
  (More info on the data flow
  [here](../../passing_data_from_renderer_to_browser.md)).

  In this codepath,
  [`MetricsRenderFrameObserver::GetTiming()`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/metrics_render_frame_observer.cc;l=5;drc=bff7fcba732aa420926466bf53dbd1d9504ba22f;bpv=1;bpt=1?q=metrics_render_frame_&ss=chromium%2Fchromium%2Fsrc)
  gets the values to report to the browser process about the largest contentful
  image and text from `WebPerformance`, which gets them from
  [`LargestContentfulPaintCalculator`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.cc;l=73;drc=11b5b115b432e67d5c2a572d11cebe6a81ced05e).
* [`ImagePaintTimingDetector`](/third_party/blink/renderer/core/paint/image_paint_timing_detector.cc)
  manages records of image paints in a frame and tracks the largest.
* [`TextPaintTimingDetector`](/third_party/blink/renderer/core/paint/text_paint_timing_detector.h)
  manages records of text paints in a frame and tracks the largest.
* [`LargestContentfulPaintCalculator`](/third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h)
  reports new largest image and text paints to performance timeline and tracing.

## Reporting in web performance APIs and trace events

* The [Largest Contentful Paint API](https://wicg.github.io/largest-contentful-paint/)
  reports each candidate (the largest contentful paint *so far*) as it is
  painted.

  [`LargestContentfulPaintCalculator::UpdateLargestContentfulPaintCandidate()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.cc;l=25;drc=bff7fcba732aa420926466bf53dbd1d9504ba22f;bpv=1;bpt=1)
  is called from the paint timing detector each time a new largest text
  or image candidate is found. If the candidate is larger than the previous
  largest candidate of either type, either `UpdateLargestContentfulImage()` or
  `UpdateLargestContentfulText()` is called, depending on the type of the
  new largest candidate. Both methods call
  [`WindowPerformance::OnLargestContentfulPaintUpdated()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/timing/window_performance.cc;l=655;drc=bff7fcba732aa420926466bf53dbd1d9504ba22f),
  which report the timing via the API.
* Each of the above methods also emits a trace event with the following
  categories: "loading,rail,devtools.timeline". The title of the trace event is
  "largestContentfulPaint::Candidate".

## Merging multiple frames

The data is sent from renderer to browser via the page load metrics
infrastructure.
[You can read more about the data flow here](../../passing_data_from_renderer_to_browser.md).

In the browser, PageLoadMetricsObservers can call
`GetDelegate().GetLargestContentfulPaintHandler().MergeMainFrameAndSubFrames()`.

[`LargestContentfulPaintHandler::MergeMainFrameAndSubframes()`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.cc;l=225;drc=bff7fcba732aa420926466bf53dbd1d9504ba22f;bpv=1;bpt=1)
returns the largest text or image candidate across the main frame and all
subframes.

## Reporting in UKM and UMA

All Core Web Vitals UKM are reported via
[PageLoadMetricsObserver](/chrome/browser/page_load_metrics/observers/README.md).
This ensures consistent reporting of only main frames, excluding error pages,
etc.

UKM for LCP are:
* Most navigations: `PageLoad.PaintTiming.NavigationToLargestContentfulPaint2`
* BFCache navigations:
  `HistoryNavigation.NavigationToFirstPaintAfterBackForwardCacheRestore`
* Prerender2 activations:
  `PrerenderPageLoad.Timing.ActivationToLargestContentfulPaint`

UMA for LCP are:
* Most navigations: `PageLoad.PaintTiming.NavigationToLargestContentfulPaint2`
* BFCache navigations:
  `PageLoad.PaintTiming.NavigationToFirstPaint.BFCachePolyfillSecond`
* Prerender2 activations:
  `PageLoad.Clients.Prerender.PaintTiming.ActivationToLargestContentfulPaint2.{SpeculationRule, Embedder_DirectURLInput, Embedder_DefaultSearchEngine}`
