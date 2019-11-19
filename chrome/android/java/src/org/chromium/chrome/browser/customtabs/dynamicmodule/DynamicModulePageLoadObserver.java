// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.TimeUtilsJni;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

/**
 * Class to notify a CCT dynamic module of page load metrics.
 */
public class DynamicModulePageLoadObserver implements PageLoadMetrics.Observer {
    @Nullable
    private ActivityDelegate mActivityDelegate;
    private ActivityTabProvider mActivityTabProvider;

    // SystemClock.uptimeMillis() is used here as it (as of June 2017) uses the same system call
    // as all the native side of Chrome, that is clock_gettime(CLOCK_MONOTONIC). Meaning that
    // the offset relative to navigationStart is to be compared with a
    // SystemClock.uptimeMillis() value.
    private long mNativeTickOffsetUs;
    private final List<PageMetric> mPageMetricEvents = new ArrayList<>();

    private static class PageMetric {
        final String mMetricName;
        final long mNavigationStart;
        final long mOffset;
        final long mNavigationId;

        private PageMetric(String metricName, long navigationStart, long offset,
                long navigationId) {
            mMetricName = metricName;
            mNavigationStart = navigationStart;
            mOffset = offset;
            mNavigationId = navigationId;
        }
    }

    @Inject
    public DynamicModulePageLoadObserver(ActivityTabProvider activityTabProvider) {
        mActivityTabProvider = activityTabProvider;

        long nativeNowUs = TimeUtilsJni.get().getTimeTicksNowUs();
        long javaNowUs = SystemClock.uptimeMillis() * 1000;
        mNativeTickOffsetUs = nativeNowUs - javaNowUs;
    }

    /**
     * Set ActivityDelegate which will be notified on page load events.
     */
    public void setActivityDelegate(ActivityDelegate activityDelegate) {
        assert activityDelegate != null && mActivityDelegate == null;
        mActivityDelegate = activityDelegate;
        for (PageMetric metric: mPageMetricEvents) {
            mActivityDelegate.onPageMetricEvent(metric.mMetricName, metric.mNavigationStart,
                    metric.mOffset, metric.mNavigationId);
        }
        mPageMetricEvents.clear();
    }

    @Override
    public void onFirstContentfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long firstContentfulPaintMs) {
        notifyOnPageMetricEvent(webContents,
                PageLoadMetrics.FIRST_CONTENTFUL_PAINT,
                navigationStartTick, firstContentfulPaintMs, navigationId);
    }

    @Override
    public void onLoadEventStart(WebContents webContents, long navigationId,
            long navigationStartTick, long loadEventStartMs) {
        notifyOnPageMetricEvent(webContents,
                PageLoadMetrics.LOAD_EVENT_START,
                navigationStartTick, loadEventStartMs, navigationId);
    }

    private void notifyOnPageMetricEvent(WebContents webContents,
            String metricName, long navigationStartTick, long offset, long navigationId) {
        if (webContents != mActivityTabProvider.get().getWebContents()) return;
        long navigationStartMs = (navigationStartTick - mNativeTickOffsetUs) / 1000;

        if (mActivityDelegate == null) {
            mPageMetricEvents.add(
                    new PageMetric(metricName, navigationStartMs, offset, navigationId));
            return;
        }

        mActivityDelegate.onPageMetricEvent(metricName, navigationStartMs, offset, navigationId);
    }
}
