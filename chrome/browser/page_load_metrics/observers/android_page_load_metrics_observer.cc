// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/PageLoadMetrics_jni.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/gurl.h"

AndroidPageLoadMetricsObserver::AndroidPageLoadMetricsObserver() {
  network_quality_tracker_ = g_browser_process->network_quality_tracker();
  DCHECK(network_quality_tracker_);
}

AndroidPageLoadMetricsObserver::ObservePolicy
AndroidPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();
  ReportNewNavigation();
  int64_t http_rtt = network_quality_tracker_->GetHttpRTT().InMilliseconds();
  int64_t transport_rtt =
      network_quality_tracker_->GetTransportRTT().InMilliseconds();
  ReportNetworkQualityEstimate(
      network_quality_tracker_->GetEffectiveConnectionType(), http_rtt,
      transport_rtt);

  return CONTINUE_OBSERVING;
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

void AndroidPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, GetDelegate());
}

void AndroidPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ReportBufferedMetrics(timing);
}

void AndroidPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_contentful_paint_ms =
      timing.paint_timing->first_contentful_paint->InMilliseconds();
  ReportFirstContentfulPaint(
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds(),
      first_contentful_paint_ms);
}

void AndroidPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_input_delay_ms =
      timing.interactive_timing->first_input_delay->InMilliseconds();
  ReportFirstInputDelay(first_input_delay_ms);
}

void AndroidPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t first_meaningful_paint_ms =
      timing.paint_timing->first_meaningful_paint->InMilliseconds();
  ReportFirstMeaningfulPaint(
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds(),
      first_meaningful_paint_ms);
}

void AndroidPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t load_event_start_ms =
      timing.document_timing->load_event_start->InMilliseconds();
  ReportLoadEventStart(
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds(),
      load_event_start_ms);
}

void AndroidPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (extra_request_complete_info.resource_type ==
      content::ResourceType::kMainFrame) {
    DCHECK(!did_dispatch_on_main_resource_);
    if (did_dispatch_on_main_resource_) {
      // We are defensive for the case of something strange happening and return
      // in order not to post multiple times.
      return;
    }
    did_dispatch_on_main_resource_ = true;

    const net::LoadTimingInfo& timing =
        *extra_request_complete_info.load_timing_info;
    int64_t dns_start =
        timing.connect_timing.dns_start.since_origin().InMilliseconds();
    int64_t dns_end =
        timing.connect_timing.dns_end.since_origin().InMilliseconds();
    int64_t connect_start =
        timing.connect_timing.connect_start.since_origin().InMilliseconds();
    int64_t connect_end =
        timing.connect_timing.connect_end.since_origin().InMilliseconds();
    int64_t request_start =
        timing.request_start.since_origin().InMilliseconds();
    int64_t send_start = timing.send_start.since_origin().InMilliseconds();
    int64_t send_end = timing.send_end.since_origin().InMilliseconds();
    ReportLoadedMainResource(dns_start, dns_end, connect_start, connect_end,
                             request_start, send_start, send_end);
  }
}

void AndroidPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  largest_contentful_paint_handler_.RecordTiming(timing.paint_timing,
                                                 subframe_rfh);
}

void AndroidPageLoadMetricsObserver::ReportNewNavigation() {
  DCHECK_GE(navigation_id_, 0);
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onNewNavigation(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jboolean>(GetDelegate().IsFirstNavigationInWebContents()));
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
      (GetDelegate().GetNavigationStart() - base::TimeTicks()).InMicroseconds();
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      largest_contentful_paint_handler_.MergeMainFrameAndSubframes();
  if (!largest_contentful_paint.IsEmpty()) {
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
      static_cast<jfloat>(
          GetDelegate().GetPageRenderData().layout_shift_score));
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
      static_cast<jlong>(transport_rtt_ms));
}

void AndroidPageLoadMetricsObserver::ReportFirstContentfulPaint(
    int64_t navigation_start_tick,
    int64_t first_contentful_paint_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstContentfulPaint(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(navigation_start_tick),
      static_cast<jlong>(first_contentful_paint_ms));
}

void AndroidPageLoadMetricsObserver::ReportFirstMeaningfulPaint(
    int64_t navigation_start_tick,
    int64_t first_meaningful_paint_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstMeaningfulPaint(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(navigation_start_tick),
      static_cast<jlong>(first_meaningful_paint_ms));
}

void AndroidPageLoadMetricsObserver::ReportLoadEventStart(
    int64_t navigation_start_tick,
    int64_t load_event_start_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onLoadEventStart(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(navigation_start_tick),
      static_cast<jlong>(load_event_start_ms));
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
      static_cast<jlong>(send_end_ms));
}

void AndroidPageLoadMetricsObserver::ReportFirstInputDelay(
    int64_t first_input_delay_ms) {
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      GetDelegate().GetWebContents()->GetJavaWebContents();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageLoadMetrics_onFirstInputDelay(
      env, java_web_contents, static_cast<jlong>(navigation_id_),
      static_cast<jlong>(first_input_delay_ms));
}
