// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/page_load_metrics/jni_headers/PageLoadMetrics_jni.h"

AndroidPageLoadMetricsObserver::AndroidPageLoadMetricsObserver() {
  network_quality_tracker_ = g_browser_process->network_quality_tracker();
  DCHECK(network_quality_tracker_);
}

AndroidPageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  ReportNewNavigation(navigation_handle->GetNavigationId());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class uses the event OnLoadedResource, but only uses the one with
  // network::mojom::RequestDestination::kDocument, which never occur in
  // FencedFrames' navigation. So, we can use STOP_OBSERVING.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  ReportNewNavigation(navigation_handle->GetNavigationId());
  return CONTINUE_OBSERVING;
}

void AndroidPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // NavigationHandle for the activation contains the source of
  // `activation_start` tick as NavigationStart.
  ReportActivation(navigation_handle->GetNavigationId(),
                   navigation_handle->NavigationStart());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  ReportBufferedMetrics(timing);

  // We continue observing after being backgrounded, in case we are foregrounded
  // again without being killed. In those cases we may still report non-buffered
  // metrics such as FCP after being re-foregrounded.
  return CONTINUE_OBSERVING;
}

AndroidPageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportBufferedMetrics(timing);
  return CONTINUE_OBSERVING;
}

void AndroidPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO(http://crbug.com/1363952): Java side WebContents insntace obtained
  // via GetJavaWebContents() was already released here, and newly created wrong
  // instance will be obtained in the following calls.
  ReportBufferedMetrics(timing);
}

void AndroidPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportFirstContentfulPaint(GetDelegate().GetNavigationStart(),
                             *timing.paint_timing->first_contentful_paint);
}

void AndroidPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportFirstInputDelay(*timing.interactive_timing->first_input_delay);
}

void AndroidPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportFirstMeaningfulPaint(GetDelegate().GetNavigationStart(),
                             *timing.paint_timing->first_meaningful_paint);
}

void AndroidPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportLoadEventStart(GetDelegate().GetNavigationStart(),
                       *timing.document_timing->load_event_start);
}

void AndroidPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (extra_request_complete_info.request_destination ==
      network::mojom::RequestDestination::kDocument) {
    DCHECK(!did_dispatch_on_main_resource_);
    if (did_dispatch_on_main_resource_) {
      // We are defensive for the case of something strange happening and return
      // in order not to post multiple times.
      return;
    }
    did_dispatch_on_main_resource_ = true;

    const net::LoadTimingInfo& timing =
        *extra_request_complete_info.load_timing_info;
    int64_t domain_lookup_start =
        timing.connect_timing.domain_lookup_start.since_origin()
            .InMilliseconds();
    int64_t domain_lookup_end =
        timing.connect_timing.domain_lookup_end.since_origin().InMilliseconds();
    int64_t connect_start =
        timing.connect_timing.connect_start.since_origin().InMilliseconds();
    int64_t connect_end =
        timing.connect_timing.connect_end.since_origin().InMilliseconds();
    int64_t request_start =
        timing.request_start.since_origin().InMilliseconds();
    int64_t send_start = timing.send_start.since_origin().InMilliseconds();
    int64_t send_end = timing.send_end.since_origin().InMilliseconds();
    ReportLoadedMainResource(domain_lookup_start, domain_lookup_end,
                             connect_start, connect_end, request_start,
                             send_start, send_end);
  }
}

void AndroidPageLoadMetricsObserver::ReportNewNavigation(
    int64_t navigation_id) {
  DCHECK_GE(navigation_id, 0);
  navigation_id_ = navigation_id;
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onNewNavigation(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jboolean>(GetDelegate().IsFirstNavigationInWebContents()),
      static_cast<jboolean>(IsPrerendering()));

  int64_t http_rtt = network_quality_tracker_->GetHttpRTT().InMilliseconds();
  int64_t transport_rtt =
      network_quality_tracker_->GetTransportRTT().InMilliseconds();
  ReportNetworkQualityEstimate(
      network_quality_tracker_->GetEffectiveConnectionType(), http_rtt,
      transport_rtt);
}

void AndroidPageLoadMetricsObserver::ReportActivation(
    int64_t activating_navigation_id,
    base::TimeTicks activation_start_tick) {
  DCHECK_GE(activating_navigation_id, 0);
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onActivation(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(activating_navigation_id),
      activation_start_tick.ToUptimeMicros());
  navigation_id_ = activating_navigation_id;
}

void AndroidPageLoadMetricsObserver::ReportBufferedMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This method may be invoked multiple times. Make sure that if we already
  // reported, we do not report again.
  if (reported_buffered_metrics_)
    return;
  reported_buffered_metrics_ = true;

  // Buffered metrics aren't available until after the navigation commits.
  if (!GetDelegate().DidCommit())
    return;

  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t navigation_start_tick =
      GetDelegate().GetNavigationStart().ToUptimeMicros();
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime()) {
    Java_PageLoadMetrics_onLargestContentfulPaint(
        env, java_web_contents, static_cast<jlong>(navigation_id_),
        static_cast<jlong>(navigation_start_tick),
        static_cast<jlong>(largest_contentful_paint.Time()->InMilliseconds()),
        static_cast<jlong>(largest_contentful_paint.Size()));
  }
  Java_PageLoadMetrics_onLayoutShiftScore(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jfloat>(GetDelegate()
                              .GetMainFrameRenderData()
                              .layout_shift_score_before_input_or_scroll),
      static_cast<jfloat>(GetDelegate().GetPageRenderData().layout_shift_score),
      static_cast<jboolean>(IsPrerendering()));
}

void AndroidPageLoadMetricsObserver::ReportNetworkQualityEstimate(
    net::EffectiveConnectionType connection_type,
    int64_t http_rtt_ms,
    int64_t transport_rtt_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onNetworkQualityEstimate(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jint>(connection_type), static_cast<jlong>(http_rtt_ms),
      static_cast<jlong>(transport_rtt_ms),
      static_cast<jboolean>(IsPrerendering()));
}

void AndroidPageLoadMetricsObserver::ReportFirstContentfulPaint(
    base::TimeTicks navigation_start_tick,
    base::TimeDelta first_contentful_paint) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstContentfulPaint(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      navigation_start_tick.ToUptimeMicros(),
      static_cast<jlong>(first_contentful_paint.InMilliseconds()));
}

void AndroidPageLoadMetricsObserver::ReportFirstMeaningfulPaint(
    base::TimeTicks navigation_start_tick,
    base::TimeDelta first_meaningful_paint) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstMeaningfulPaint(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      navigation_start_tick.ToUptimeMicros(),
      static_cast<jlong>(first_meaningful_paint.InMilliseconds()));
}

void AndroidPageLoadMetricsObserver::ReportLoadEventStart(
    base::TimeTicks navigation_start_tick,
    base::TimeDelta load_event_start) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onLoadEventStart(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      navigation_start_tick.ToUptimeMicros(),
      static_cast<jlong>(load_event_start.InMilliseconds()),
      static_cast<jboolean>(IsPrerendering()));
}

void AndroidPageLoadMetricsObserver::ReportLoadedMainResource(
    int64_t dns_start_ms,
    int64_t dns_end_ms,
    int64_t connect_start_ms,
    int64_t connect_end_ms,
    int64_t request_start_ms,
    int64_t send_start_ms,
    int64_t send_end_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onLoadedMainResource(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(dns_start_ms), static_cast<jlong>(dns_end_ms),
      static_cast<jlong>(connect_start_ms), static_cast<jlong>(connect_end_ms),
      static_cast<jlong>(request_start_ms), static_cast<jlong>(send_start_ms),
      static_cast<jlong>(send_end_ms), static_cast<jboolean>(IsPrerendering()));
}

void AndroidPageLoadMetricsObserver::ReportFirstInputDelay(
    base::TimeDelta first_input_delay) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstInputDelay(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(first_input_delay.InMilliseconds()));
}

bool AndroidPageLoadMetricsObserver::IsPrerendering() {
  // So that the isPrerendering argument works to realize STOP_OBSERVING on
  // OnPrerenderStart().
  return GetDelegate().GetPrerenderingState() !=
         page_load_metrics::PrerenderingState::kNoPrerendering;
}
