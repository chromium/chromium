// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import androidx.browser.customtabs.CustomTabsCallback;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.net.NetError;

import java.util.Optional;

/** Tests for some parts of {@link CustomTabsConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CustomTabNavigationEventObserverUnitTest {
    @Mock private CustomTabsConnection mConnection;
    @Mock private BrowserServicesIntentDataProvider mIntentData;

    private CustomTabNavigationEventObserver mObserver;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mObserver = new CustomTabNavigationEventObserver(mIntentData, mConnection);
    }

    @Test
    public void histogramAndfilteredErrorCodes() {
        // Only these 3 error codes are passed to callback.
        int err = NetError.ERR_INTERNET_DISCONNECTED;
        var histogramWatcher = buildHistogramWatcher(err);
        mObserver.onPageLoadFailed(null, err);
        verifyReportedErrorCode(err);
        histogramWatcher.assertExpected();

        err = NetError.ERR_CONNECTION_TIMED_OUT;
        histogramWatcher = buildHistogramWatcher(err);
        mObserver.onPageLoadFailed(null, err);
        verifyReportedErrorCode(err);
        histogramWatcher.assertExpected();

        err = NetError.ERR_NAME_RESOLUTION_FAILED;
        histogramWatcher = buildHistogramWatcher(err);
        mObserver.onPageLoadFailed(null, err);
        verifyReportedErrorCode(err);
        histogramWatcher.assertExpected();

        // The other error codes are ignored.
        err = NetError.ERR_PROXY_AUTH_REQUESTED;
        histogramWatcher = buildHistogramWatcher(err);
        mObserver.onPageLoadFailed(null, err);
        verify(mConnection)
                .notifyNavigationEvent(
                        any(), eq(CustomTabsCallback.NAVIGATION_FAILED), eq(Optional.empty()));
        histogramWatcher.assertExpected();
    }

    private HistogramWatcher buildHistogramWatcher(int err) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord("CustomTabs.PageNavigation.ErrorCode", err)
                .build();
    }

    private void verifyReportedErrorCode(int err) {
        int reportError = CustomTabNavigationEventObserver.getReportErrorCode(err);
        verify(mConnection)
                .notifyNavigationEvent(
                        any(),
                        eq(CustomTabsCallback.NAVIGATION_FAILED),
                        eq(Optional.of(reportError)));
    }
}
