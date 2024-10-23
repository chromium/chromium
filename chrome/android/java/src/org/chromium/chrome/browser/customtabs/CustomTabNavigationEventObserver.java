// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

import java.util.Optional;

/** An observer for firing navigation events on {@link CustomTabsCallback}. */
public class CustomTabNavigationEventObserver extends EmptyTabObserver {
    // An operation was aborted (due to user action). Should match the value in net_error_list.h.
    private static final int NET_ERROR_ABORTED = -3;

    private final CustomTabsSessionToken mSessionToken;
    private final CustomTabsConnection mConnection;
    private boolean mIsPrerender;

    // Cached values when prerendering, so that we don't send events for discarded prerenders.
    private boolean mPageLoadStarted;
    private boolean mPageLoadFinished;
    private Integer mPageLoadFailed;

    public CustomTabNavigationEventObserver(CustomTabsSessionToken session, boolean forPrerender) {
        mSessionToken = session;
        mConnection = CustomTabsConnection.getInstance();
        // Kill-switch for reporting events for prerendered navigations.
        mIsPrerender =
                forPrerender
                        && !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.CCT_REPORT_PRERENDER_EVENTS);
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        if (mIsPrerender) {
            mPageLoadStarted = true;
            return;
        }
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.NAVIGATION_STARTED);
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        if (mIsPrerender) {
            mPageLoadFinished = true;
            return;
        }
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.NAVIGATION_FINISHED);
    }

    @Override
    @SuppressWarnings("TraditionalSwitchExpression")
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (mIsPrerender) {
            mPageLoadFailed = errorCode;
            return;
        }
        int navigationEvent =
                errorCode == NET_ERROR_ABORTED
                        ? CustomTabsCallback.NAVIGATION_ABORTED
                        : CustomTabsCallback.NAVIGATION_FAILED;

        // For privacy reason, we do not pass all the error codes but choose a few safe ones.
        // See crbug/1501085 for more details.
        Optional<Integer> code =
                switch (errorCode) {
                    case NetError.ERR_INTERNET_DISCONNECTED:
                    case NetError.ERR_CONNECTION_TIMED_OUT:
                    case NetError.ERR_NAME_RESOLUTION_FAILED:
                        yield Optional.of(getReportErrorCode(errorCode));
                    default:
                        yield Optional.empty();
                };

        mConnection.notifyNavigationEvent(mSessionToken, navigationEvent, code);
        RecordHistogram.recordSparseHistogram("CustomTabs.PageNavigation.ErrorCode", errorCode);
    }

    static int getReportErrorCode(int code) {
        return -code + 100;
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        if (mIsPrerender) {
            mIsPrerender = false;
            if (mPageLoadStarted) onPageLoadStarted(null, null);
            if (mPageLoadFinished) onPageLoadFinished(null, null);
            if (mPageLoadFailed != null) onPageLoadFailed(null, mPageLoadFailed);
        }
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.TAB_SHOWN);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.TAB_HIDDEN);
    }
}
