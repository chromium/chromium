// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to record UMA histograms for API calls crossing {@code
 * org.chromium.support_lib_boundary.web}.
 *
 * <p>{@code WebSurface} APIs are recorded separately under {@link WebComposeApiCall} because {@code
 * WebSurface} backs {@code androidx.web:web-compose}.
 */
@NullMarked
/* package */ final class WebApiCallLogger {
    private WebApiCallLogger() {}

    // Used to record the UMA histogram Android.WebView.AndroidX.Web.ApiCall. Since these values are
    // persisted to logs, they should never be renumbered or reused.
    // LINT.IfChange(WebApiCall)
    @IntDef({
        WebApiCall.BUILD_WEB_CONTENT,
        WebApiCall.WEB_CONTENT_EXECUTE_VIEW_FACTORY,
        WebApiCall.WEB_CONTENT_DESTROY,
        // Add new constants above. The final constant should have a trailing comma for cleaner
        // diffs.
        WebApiCall.COUNT, // Added to suppress WrongConstant in #recordWebApiCall.
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebApiCall {
        int BUILD_WEB_CONTENT = 0;
        int WEB_CONTENT_EXECUTE_VIEW_FACTORY = 1;
        int WEB_CONTENT_DESTROY = 2;
        // Remember to update AndroidXWebApiCall in enums.xml when adding new values here.
        int COUNT = 3;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:AndroidXWebApiCall)

    /* package */ static void recordWebApiCall(@WebApiCall int apiCall) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.AndroidX.Web.ApiCall", apiCall, WebApiCall.COUNT);
    }

    // Used to record the UMA histogram Android.WebView.AndroidX.WebCompose.ApiCall. Since these
    // values are persisted to logs, they should never be renumbered or reused.
    // LINT.IfChange(WebComposeApiCall)
    @IntDef({
        WebComposeApiCall.CREATE_WEB_SURFACE,
        WebComposeApiCall.WEB_SURFACE_SET_WEB_CONTENT,
        WebComposeApiCall.WEB_SURFACE_DESTROY,
        // Add new constants above. The final constant should have a trailing comma for cleaner
        // diffs.
        WebComposeApiCall.COUNT, // Added to suppress WrongConstant in #recordWebComposeApiCall.
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebComposeApiCall {
        int CREATE_WEB_SURFACE = 0;
        int WEB_SURFACE_SET_WEB_CONTENT = 1;
        int WEB_SURFACE_DESTROY = 2;
        // Remember to update AndroidXWebComposeApiCall in enums.xml when adding new values here.
        int COUNT = 3;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:AndroidXWebComposeApiCall)

    /* package */ static void recordWebComposeApiCall(@WebComposeApiCall int apiCall) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.AndroidX.WebCompose.ApiCall", apiCall, WebComposeApiCall.COUNT);
    }
}
