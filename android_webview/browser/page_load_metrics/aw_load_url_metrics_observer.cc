// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_load_url_metrics_observer.h"

#include "android_webview/browser/page_load_metrics/aw_load_url_metrics_state.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwLoadUrlMetricsObserver_jni.h"

namespace android_webview {

const char* AwLoadUrlMetricsObserver::GetObserverName() const {
  static const char kName[] = "AwLoadUrlMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwLoadUrlMetricsObserver::OnStart(content::NavigationHandle* navigation_handle,
                                  const GURL& currently_committed_url,
                                  bool started_in_foreground) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (!web_contents) {
    return STOP_OBSERVING;
  }

  // WebView's loadUrl() API triggers navigations with PAGE_TRANSITION_TYPED.
  if (!ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                    ui::PAGE_TRANSITION_TYPED)) {
    return STOP_OBSERVING;
  }

  LoadUrlMetricsState* state =
      LoadUrlMetricsState::FromWebContents(web_contents);
  if (!state) {
    return STOP_OBSERVING;
  }

  load_url_timestamp_ = state->load_url_timestamp();

  // Remove the state so it is only used for the first applicable navigation.
  web_contents->RemoveUserData(LoadUrlMetricsState::UserDataKey());

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwLoadUrlMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This metric is strictly for LoadUrl(), which applies to the main frame
  // of the WebView, so we do not want to record metrics for fenced frames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwLoadUrlMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwLoadUrlMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!load_url_timestamp_.has_value()) {
    return CONTINUE_OBSERVING;
  }

  const content::NavigationHandleTiming& timing =
      navigation_handle->GetNavigationHandleTiming();
  if (!timing.first_request_start_time.is_null()) {
    base::TimeDelta duration =
        timing.first_request_start_time - load_url_timestamp_.value();
    UMA_HISTOGRAM_TIMES("Android.WebView.PageLoad.LoadUrlToCommit",
                        duration);
  }

  return CONTINUE_OBSERVING;
}

void AwLoadUrlMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!load_url_timestamp_.has_value() || !timing.paint_timing ||
      !timing.paint_timing->first_contentful_paint.has_value()) {
    return;
  }

  base::TimeDelta duration =
      (GetDelegate().GetNavigationStart() - load_url_timestamp_.value()) +
      timing.paint_timing->first_contentful_paint.value();

  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Android.WebView.PageLoad.LoadUrlToFirstContentfulPaint", duration);
}

}  // namespace android_webview

static void JNI_AwLoadUrlMetricsObserver_SetPendingLoadUrlTimestamp(
    JNIEnv* env,
    int64_t uptime_millis,
    const base::android::JavaRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents) {
    return;
  }

  base::TimeTicks timestamp = base::TimeTicks::FromUptimeMillis(uptime_millis);
  // If `loadUrl` is called multiple times, the later call will cancel the
  // previous navigation. We remove the existing user data first to ensure
  // the new navigation doesn't incorrectly pick up a stale timestamp from a
  // previous aborted or blocked call.
  web_contents->RemoveUserData(
      android_webview::LoadUrlMetricsState::UserDataKey());
  android_webview::LoadUrlMetricsState::CreateForWebContents(web_contents,
                                                             timestamp);
}

DEFINE_JNI(AwLoadUrlMetricsObserver)
