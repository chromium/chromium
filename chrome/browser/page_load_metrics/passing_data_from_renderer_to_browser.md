# How data is passed from renderer to browser for page load metrics observers

1. Blink's core rendering code hooks into a "detector" that implements the
   semantics of the metric.  Example:
   [`PaintTimingDetector`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/timing/paint_timing_detector.cc).
   (This is more of a convention and not a specific shared interface.)

2. The detector notifies the
   [LocalFrameClient](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/local_frame_client.h)
   that the metric's value has changed.  For some metrics, this uses the generic
   `DidChangePerformanceTiming` method, but other metrics have dedicated methods
   on this interface.

3. The `LocalFrameClientImpl` notifies the
   [`RenderFrameImpl`](https://source.chromium.org/chromium/chromium/src/+/main:content/renderer/render_frame_impl.h)
   that the metric's value has changed.

4. The `RenderFrameImpl` notifies `PageLoadMetrics`'
   [`MetricsRenderFrameObserver`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/metrics_render_frame_observer.h)
   (through a generic observer interface) that the metric's value has changed.

5. Sometimes, new data is passed in to `MetricsRenderFrameObserver` in the same
   method that notifies it of the change.
   
   But for metrics that rely on `DidChangePerformanceTiming`,
   `MetricsRenderFrameObserver` has to go back to Blink to ask for the actual
   data, by querying the [`WebPerformanceMetricsForReporting`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/web/web_performance_metrics_for_reporting.h)
   object, which in turn queries the detector from step 1.

6. `MetricsRenderFrameObserver` passes the new data into `PageLoadMetrics`'s
   [`PageTimingMetricsSender`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/renderer/page_timing_metrics_sender.h).

7. `PageTimingMetricsSender` buffers the data for up
   to 100 ms, then issues an IPC to the browser which is handled by
   [`MetricsWebContentsObserver`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/metrics_web_contents_observer.h).

8. `MetricsWebContentsObserver` passes the data to the
   [`PageLoadMetricsUpdateDispatcher`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h)
   for the currently tracked page load.

9. For some metrics, like FCP, `PageLoadMetricsUpdateDispatcher` "merges"
   updates from all frames (using `PageLoadTimingMerger`) into a single
   page-wide value, which it then passes up to its owner, the
   [`PageLoadTracker`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_tracker.h).
   
   But for other metrics, `PageLoadMetricsUpdateDispatcher` passes per-frame
   values up to `PageLoadTracker`.  This is the case for LCP, which is merged by
   a `PageLoadTracker`-owned object
   ([`LargestContentfulPaintHandler`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h))
   and not by `PageLoadMetricsUpdateDispatcher`.

10. `PageLoadTracker` broadcasts metric values (some page-wide, some
    frame-specific) to various
    [`PageLoadMetricsObserver`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/page_load_metrics_observer.h)
    implementers.  Some metrics pass through the generic `OnTimingUpdate`
    method, while others have dedicated methods on the observer interface.

11. `PageLoadMetricsObserver` implementations like
    [`UmaPageLoadMetricsObserver`](https://source.chromium.org/chromium/chromium/src/+/main:components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h)
    and [`UkmPageLoadMetricsObserver`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h)
    perform the final step of recording to UMA/UKM, calling into generic
    components like
    [`base::Histogram`](https://source.chromium.org/chromium/chromium/src/+/main:base/metrics/histogram.h)
    and
    [`UkmRecorder`](https://source.chromium.org/chromium/chromium/src/+/main:services/metrics/public/cpp/ukm_recorder.h).