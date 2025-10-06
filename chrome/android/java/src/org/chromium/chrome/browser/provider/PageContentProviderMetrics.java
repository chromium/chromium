// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.ukm.UkmRecorder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class PageContentProviderMetrics {

    @IntDef({Format.TEXT, Format.PROTO})
    @interface Format {
        int TEXT = 0;
        int PROTO = 1;
    }

    @IntDef({RequestType.QUERY, RequestType.OPEN_FILE})
    @interface RequestType {
        int QUERY = 0;
        int OPEN_FILE = 1;
    }

    @IntDef({
        PageContentProviderEvent.GET_CONTENT_URI_FAILED,
        PageContentProviderEvent.REQUEST_STARTED,
        PageContentProviderEvent.REQUEST_FAILED_CURRENT_TAB_CHANGED,
        PageContentProviderEvent.REQUEST_FAILED_INVALID_URL,
        PageContentProviderEvent.REQUEST_FAILED_INVALID_ID,
        PageContentProviderEvent.REQUEST_FAILED_TO_GET_CURRENT_TAB,
        PageContentProviderEvent.REQUEST_SUCCEEDED_RETURNED_EXTRACTED,
        PageContentProviderEvent.REQUEST_FAILED_EMPTY_RESULT,
        PageContentProviderEvent.REQUEST_FAILED_INTERRUPTED,
        PageContentProviderEvent.REQUEST_FAILED_TIMED_OUT,
        PageContentProviderEvent.REQUEST_FAILED_EXCEPTION,
        PageContentProviderEvent.GET_CONTENT_URI_FAILED,
        PageContentProviderEvent.GET_CONTENT_URI_SUCCESS,
        PageContentProviderEvent.URI_INVALIDATED_NEW_REQUEST,
        PageContentProviderEvent.URI_INVALIDATED_TIMEOUT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PageContentProviderEvent {

        int GET_CONTENT_URI_FAILED = 0;
        int REQUEST_STARTED = 1;
        int REQUEST_FAILED_CURRENT_TAB_CHANGED = 2;
        int REQUEST_FAILED_INVALID_URL = 3;
        int REQUEST_FAILED_INVALID_ID = 4;
        int REQUEST_FAILED_TO_GET_CURRENT_TAB = 5;
        int REQUEST_SUCCEEDED_RETURNED_EXTRACTED = 7;
        int REQUEST_FAILED_EMPTY_RESULT = 9;
        int URI_INVALIDATED_TIMEOUT = 12;
        int REQUEST_FAILED_INTERRUPTED = 13;
        int REQUEST_FAILED_TIMED_OUT = 14;
        int REQUEST_FAILED_EXCEPTION = 15;
        int GET_CONTENT_URI_SUCCESS = 16;
        int URI_INVALIDATED_NEW_REQUEST = 17;
        int NUM_ENTRIES = 18;
    }

    public static void recordPageProviderEvent(@PageContentProviderEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AssistContent.WebPageContentProvider.Events",
                event,
                PageContentProviderEvent.NUM_ENTRIES);
    }

    public static void recordPageProviderEvent(
            @RequestType int requestType, @Format int format, @PageContentProviderEvent int event) {
        var histogramName =
                concatenateTypeAndFormatToHistogramName(
                        "Android.AssistContent.WebPageContentProvider.Events", requestType, format);
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, event, PageContentProviderEvent.NUM_ENTRIES);
    }

    public static void recordPageContentRequestedUkm(Tab tab) {
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;
        new UkmRecorder(tab.getWebContents(), "Android.AssistContent.PageContextRequest")
                .addBooleanMetric("PageContextRequested")
                .record();
    }

    public static void recordUrlAttachedToAssistContent(boolean urlAttached) {
        RecordHistogram.recordBooleanHistogram("Android.AssistContent.AttachedUrl", urlAttached);
    }

    public static void recordPdfStructuredDataAttachedToAssistContent(
            boolean pdfStructuredDataAttached) {
        RecordHistogram.recordBooleanHistogram(
                "Android.AssistContent.StructuredDataAttachedSuccess.Pdf",
                pdfStructuredDataAttached);
    }

    public static void recordEnterpriseInfoCacheStateForWebAssistContent(
            boolean isEnterpriseInfoCached) {
        RecordHistogram.recordBooleanHistogram(
                "Android.AssistContent.IsEnterpriseInfoCached", isEnterpriseInfoCached);
    }

    public static void recordWebStructuredDataAttachedToAssistContent(
            Tab tab, boolean webStructuredDataAttached) {
        RecordHistogram.recordBooleanHistogram(
                "Android.AssistContent.WebPage", webStructuredDataAttached);
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;
        new UkmRecorder(tab.getWebContents(), "Android.AssistContent.Request")
                .addMetric("WebPageStructuredDataAttached", webStructuredDataAttached ? 1 : 0)
                .record();
    }

    public static void recordCreateToExtractionStartLatency(
            @RequestType int requestType, @Format int format, long duration) {
        var histogramName =
                concatenateTypeAndFormatToHistogramName(
                        "Android.AssistContent.WebPageContentProvider.Latency.CreateToExtractionStart",
                        requestType,
                        format);
        RecordHistogram.recordMediumTimesHistogram(histogramName, duration);
    }

    static void recordExtractionStartToEndLatency(
            @RequestType int requestType, @Format int format, long duration) {
        var histogramName =
                concatenateTypeAndFormatToHistogramName(
                        "Android.AssistContent.WebPageContentProvider.Latency.ExtractionStartToEnd",
                        requestType,
                        format);
        RecordHistogram.recordMediumTimesHistogram(histogramName, duration);
    }

    static void recordTotalLatency(
            @RequestType int requestType, @Format int format, long duration) {
        var histogramName =
                concatenateTypeAndFormatToHistogramName(
                        "Android.AssistContent.WebPageContentProvider.Latency.TotalLatency",
                        requestType,
                        format);
        RecordHistogram.recordMediumTimesHistogram(histogramName, duration);
    }

    @VisibleForTesting
    static String concatenateTypeAndFormatToHistogramName(
            String histogram, @RequestType int requestType, @Format int format) {
        return histogram
                + '.'
                + (requestType == RequestType.QUERY ? "Query" : "OpenFile")
                + '.'
                + (format == Format.TEXT ? "Text" : "Proto");
    }
}
