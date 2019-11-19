// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Bundle;

import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import androidx.browser.customtabs.CustomTabsSessionToken;

/**
 * Notifies the provided {@link CustomTabsConnection} of page load metrics, such as time until first
 * contentful paint.
 */
public class PageLoadMetricsObserver implements PageLoadMetrics.Observer {
    private final CustomTabsConnection mConnection;
    private final CustomTabsSessionToken mSession;
    private final Tab mTab;
    private Long mNavigationId;

    public PageLoadMetricsObserver(CustomTabsConnection connection,
            CustomTabsSessionToken session, Tab tab) {
        mConnection = connection;
        mSession = session;
        mTab = tab;
    }

    @Override
    public void onNewNavigation(
            WebContents webContents, long navigationId, boolean isFirstNavigationInWebContents) {
        if (isFirstNavigationInWebContents && webContents == mTab.getWebContents()) {
            assert mNavigationId == null;
            mNavigationId = navigationId;
        }
    }

    @Override
    public void onNetworkQualityEstimate(WebContents webContents, long navigationId,
            int effectiveConnectionType, long httpRttMs, long transportRttMs) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        Bundle args = new Bundle();
        args.putLong(PageLoadMetrics.EFFECTIVE_CONNECTION_TYPE, effectiveConnectionType);
        args.putLong(PageLoadMetrics.HTTP_RTT, httpRttMs);
        args.putLong(PageLoadMetrics.TRANSPORT_RTT, transportRttMs);
        args.putBoolean(CustomTabsConnection.DATA_REDUCTION_ENABLED,
                DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
        mConnection.notifyPageLoadMetrics(mSession, args);
    }

    @Override
    public void onFirstContentfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long firstContentfulPaintMs) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        mConnection.notifySinglePageLoadMetric(mSession, PageLoadMetrics.FIRST_CONTENTFUL_PAINT,
                navigationStartTick, firstContentfulPaintMs);
    }

    @Override
    public void onLargestContentfulPaint(WebContents webContents, long navigationId,
            long navigationStartTick, long largestContentfulPaintMs,
            long largestContentfulPaintSize) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        Bundle args = mConnection.createBundleWithNavigationStartAndPageLoadMetric(
                PageLoadMetrics.LARGEST_CONTENTFUL_PAINT, navigationStartTick,
                largestContentfulPaintMs);
        args.putLong(PageLoadMetrics.LARGEST_CONTENTFUL_PAINT_SIZE, largestContentfulPaintSize);
        mConnection.notifyPageLoadMetrics(mSession, args);
    }

    @Override
    public void onLoadEventStart(WebContents webContents, long navigationId,
            long navigationStartTick, long loadEventStartMs) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        mConnection.notifySinglePageLoadMetric(
                mSession, PageLoadMetrics.LOAD_EVENT_START, navigationStartTick, loadEventStartMs);
    }

    @Override
    public void onLoadedMainResource(WebContents webContents, long navigationId,
            long dnsStartMs, long dnsEndMs, long connectStartMs, long connectEndMs,
            long requestStartMs, long sendStartMs, long sendEndMs) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        Bundle args = new Bundle();
        args.putLong(PageLoadMetrics.DOMAIN_LOOKUP_START, dnsStartMs);
        args.putLong(PageLoadMetrics.DOMAIN_LOOKUP_END, dnsEndMs);
        args.putLong(PageLoadMetrics.CONNECT_START, connectStartMs);
        args.putLong(PageLoadMetrics.CONNECT_END, connectEndMs);
        args.putLong(PageLoadMetrics.REQUEST_START, requestStartMs);
        args.putLong(PageLoadMetrics.SEND_START, sendStartMs);
        args.putLong(PageLoadMetrics.SEND_END, sendEndMs);
        mConnection.notifyPageLoadMetrics(mSession, args);
    }

    @Override
    public void onFirstInputDelay(
            WebContents webContents, long navigationId, long firstInputDelayMs) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        Bundle args = new Bundle();
        args.putLong(PageLoadMetrics.FIRST_INPUT_DELAY, firstInputDelayMs);
        mConnection.notifyPageLoadMetrics(mSession, args);
    }

    @Override
    public void onLayoutShiftScore(WebContents webContents, long navigationId,
            float layoutShiftScoreBeforeInputOrScroll, float layoutShiftScoreOverall) {
        if (!shouldNotifyPageLoadMetrics(webContents, navigationId)) return;

        Bundle args = new Bundle();
        args.putFloat(PageLoadMetrics.LAYOUT_SHIFT_SCORE, layoutShiftScoreOverall);
        args.putFloat(PageLoadMetrics.LAYOUT_SHIFT_SCORE_BEFORE_INPUT_OR_SCROLL,
                layoutShiftScoreBeforeInputOrScroll);
        mConnection.notifyPageLoadMetrics(mSession, args);
    }

    private boolean shouldNotifyPageLoadMetrics(WebContents webContents, long navigationId) {
        return webContents == mTab.getWebContents() && null != mNavigationId
                && navigationId == mNavigationId;
    }
}
