// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

import java.util.Optional;

import javax.inject.Inject;

/** An observer for firing navigation events on {@link CustomTabsCallback}. */
@ActivityScope
public class CustomTabNavigationEventObserver extends EmptyTabObserver {
    // An operation was aborted (due to user action). Should match the value in net_error_list.h.
    private static final int NET_ERROR_ABORTED = -3;

    private final CustomTabsSessionToken mSessionToken;
    private final CustomTabsConnection mConnection;

    @Inject
    public CustomTabNavigationEventObserver(
            BrowserServicesIntentDataProvider intentDataProvider, CustomTabsConnection connection) {
        mSessionToken = intentDataProvider.getSession();
        mConnection = connection;
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.NAVIGATION_STARTED);
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.NAVIGATION_FINISHED);
    }

    @Override
    @SuppressWarnings("TraditionalSwitchExpression")
    public void onPageLoadFailed(Tab tab, int errorCode) {
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
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.TAB_SHOWN);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        mConnection.notifyNavigationEvent(mSessionToken, CustomTabsCallback.TAB_HIDDEN);
    }
}
