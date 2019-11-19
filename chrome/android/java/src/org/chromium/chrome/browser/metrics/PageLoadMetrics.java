// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;

/**
 * Receives the page load metrics updates from AndroidPageLoadMetricsObserver, and notifies the
 * observers.
 *
 * Threading: everything here must happen on the UI thread.
 */
public class PageLoadMetrics {
    public static final String FIRST_CONTENTFUL_PAINT = "firstContentfulPaint";
    public static final String LARGEST_CONTENTFUL_PAINT = "largestContentfulPaint";
    public static final String LARGEST_CONTENTFUL_PAINT_SIZE = "largestContentfulPaintSize";
    public static final String NAVIGATION_START = "navigationStart";
    public static final String LOAD_EVENT_START = "loadEventStart";
    public static final String FIRST_INPUT_DELAY = "firstInputDelay";
    public static final String LAYOUT_SHIFT_SCORE = "layoutShiftScore";
    public static final String LAYOUT_SHIFT_SCORE_BEFORE_INPUT_OR_SCROLL =
            "layoutShiftScoreBeforeInputOrScroll";
    public static final String DOMAIN_LOOKUP_START = "domainLookupStart";
    public static final String DOMAIN_LOOKUP_END = "domainLookupEnd";
    public static final String CONNECT_START = "connectStart";
    public static final String CONNECT_END = "connectEnd";
    public static final String REQUEST_START = "requestStart";
    public static final String SEND_START = "sendStart";
    public static final String SEND_END = "sendEnd";
    public static final String EFFECTIVE_CONNECTION_TYPE = "effectiveConnectionType";
    public static final String HTTP_RTT = "httpRtt";
    public static final String TRANSPORT_RTT = "transportRtt";

    /** Observer for page load metrics. */
    public interface Observer {
        /**
         * Called when the new navigation is started. It's guaranteed to be called before any other
         * function with the same navigationId.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param isFirstNavigationInWebContents whether this is the first nav in the WebContents.
         */
        default void onNewNavigation(WebContents webContents, long navigationId,
                boolean isFirstNavigationInWebContents) {}

        /**
         * Called when Network Quality Estimate is available, once per page load, when the
         * load is started. This is guaranteed to be called before any other metric event
         * below. If Chromium has just been started, this will likely be determined from
         * the current connection type rather than actual network measurements and so
         * probably similar to what the ConnectivityManager reports.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param effectiveConnectionType the effective connection type, see
         *     net::EffectiveConnectionType.
         * @param httpRttMs an estimate of HTTP RTT, in milliseconds. Will be zero if unknown.
         * @param transportRttMs an estimate of transport RTT, in milliseconds. Will be zero
         *     if unknown.
         */
        default void onNetworkQualityEstimate(WebContents webContents, long navigationId,
                int effectiveConnectionType, long httpRttMs, long transportRttMs) {}

        /**
         * Called when the first contentful paint page load metric is available.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param navigationStartTick Absolute navigation start time, as TimeTicks.
         * @param firstContentfulPaintMs Time to first contentful paint from navigation start.
         */
        default void onFirstContentfulPaint(WebContents webContents, long navigationId,
                long navigationStartTick, long firstContentfulPaintMs) {}

        /**
         * Called when the largest contentful paint page load metric is available.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param navigationStartTick Absolute navigation start time, as TimeTicks.
         * @param largestContentfulPaintMs Time to largest contentful paint from navigation start.
         * @param largestContentfulPaintSize Size of largest contentful paint, in CSS pixels.
         */
        default void onLargestContentfulPaint(WebContents webContents, long navigationId,
                long navigationStartTick, long largestContentfulPaintMs,
                long largestContentfulPaintSize) {}

        /**
         * Called when the first meaningful paint page load metric is available. See
         * FirstMeaningfulPaintDetector.cpp
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param navigationStartTick Absolute navigation start time, as TimeTicks.
         * @param firstMeaningfulPaintMs Time to first meaningful paint from navigation start.
         */
        default void onFirstMeaningfulPaint(WebContents webContents, long navigationId,
                long navigationStartTick, long firstMeaningfulPaintMs) {}

        /**
         * Called when the first input delay page load metric is available.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param firstInputDelayMs First input delay.
         */
        default void onFirstInputDelay(
                WebContents webContents, long navigationId, long firstInputDelayMs) {}

        /**
         * Called when the load event start metric is available.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param navigationStartTick Absolute navigation start time, as TimeTicks.
         * @param loadEventStartMs Time to load event start from navigation start.
         */
        default void onLoadEventStart(WebContents webContents, long navigationId,
                long navigationStartTick, long loadEventStartMs) {}

        /**
         * Called when the main resource is loaded.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         *
         * Remaining parameters are timing information in milliseconds from a common
         * arbitrary point (such as, but not guaranteed to be, system start).
         */
        default void onLoadedMainResource(WebContents webContents, long navigationId,
                long dnsStartMs, long dnsEndMs, long connectStartMs, long connectEndMs,
                long requestStartMs, long sendStartMs, long sendEndMs) {}

        /**
         * Called when the layout shift score is available.
         *
         * @param webContents the WebContents this metrics is related to.
         * @param navigationId the unique id of a navigation this metrics is related to.
         * @param layoutShiftScoreBeforeInputOrScroll the cumulative layout shift score, before user
         *         input or scroll.
         * @param layoutShiftScoreOverall the cumulative layout shift score over the lifetime of the
         *         web page.
         */
        default void onLayoutShiftScore(WebContents webContents, long navigationId,
                float layoutShiftScoreBeforeInputOrScroll, float layoutShiftScoreOverall) {}
    }

    private static ObserverList<Observer> sObservers;

    /** Adds an observer. */
    public static boolean addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) sObservers = new ObserverList<>();
        return sObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public static boolean removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return false;
        return sObservers.removeObserver(observer);
    }

    @CalledByNative
    static void onNewNavigation(
            WebContents webContents, long navigationId, boolean isFirstNavigationInWebContents) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onNewNavigation(webContents, navigationId, isFirstNavigationInWebContents);
        }
    }

    @CalledByNative
    static void onNetworkQualityEstimate(WebContents webContents, long navigationId,
            int effectiveConnectionType, long httpRttMs, long transportRttMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onNetworkQualityEstimate(
                    webContents, navigationId, effectiveConnectionType, httpRttMs, transportRttMs);
        }
    }

    @CalledByNative
    static void onFirstContentfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long firstContentfulPaintMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onFirstContentfulPaint(
                    webContents, navigationId, navigationStartTick, firstContentfulPaintMs);
        }
    }

    @CalledByNative
    static void onLargestContentfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long largestContentfulPaintMs,
            long largestContentfulPaintSize) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onLargestContentfulPaint(webContents, navigationId, navigationStartTick,
                    largestContentfulPaintMs, largestContentfulPaintSize);
        }
    }

    @CalledByNative
    static void onFirstMeaningfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long firstMeaningfulPaintMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onFirstMeaningfulPaint(
                    webContents, navigationId, navigationStartTick, firstMeaningfulPaintMs);
        }
    }

    @CalledByNative
    static void onFirstInputDelay(
            WebContents webContents, long navigationId, long firstInputDelayMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onFirstInputDelay(webContents, navigationId, firstInputDelayMs);
        }
    }

    @CalledByNative
    static void onLoadEventStart(WebContents webContents, long navigationId,
            long navigationStartTick, long loadEventStartMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onLoadEventStart(
                    webContents, navigationId, navigationStartTick, loadEventStartMs);
        }
    }

    @CalledByNative
    static void onLoadedMainResource(WebContents webContents, long navigationId, long dnsStartMs,
            long dnsEndMs, long connectStartMs, long connectEndMs, long requestStartMs,
            long sendStartMs, long sendEndMs) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onLoadedMainResource(webContents, navigationId, dnsStartMs, dnsEndMs,
                    connectStartMs, connectEndMs, requestStartMs, sendStartMs, sendEndMs);
        }
    }

    @CalledByNative
    static void onLayoutShiftScore(WebContents webContents, long navigationId,
            float layoutShiftScoreBeforeInputOrScroll, float layoutShiftScoreOverall) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onLayoutShiftScore(webContents, navigationId,
                    layoutShiftScoreBeforeInputOrScroll, layoutShiftScoreOverall);
        }
    }

    private PageLoadMetrics() {}
}
