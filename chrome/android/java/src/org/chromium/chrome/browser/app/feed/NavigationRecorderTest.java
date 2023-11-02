// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link NavigationRecorder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationRecorderTest {
    private static final String TAG = "NavRecorderTest";
    @Rule
    public ChromeTabbedActivityTestRule mTestSetupRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private String mNavUrl;
    private Tab mInitialTab;

    @Before
    public void setUp() {
        mTestServer = mTestSetupRule.getEmbeddedTestServerRule().getServer();
        mNavUrl = mTestServer.getURL("/chrome/test/data/android/google.html");
        mTestSetupRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mInitialTab = mTestSetupRule.getActivity().getActivityTab();
            // Add logging to debug flaky test: crbug.com/1297086.
            mInitialTab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    Log.e(TAG, "onPageLoadStarted " + url.getSpec());
                }
                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    Log.e(TAG, "onPageLoadFinished " + url.getSpec());
                }
                @Override
                public void onPageLoadFailed(Tab tab, int errorCode) {
                    Log.e(TAG, "onPageLoadFailed " + errorCode);
                }
            });
        });
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWithBack() throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(mNavUrl, new Callback<NavigationRecorder.VisitData>() {
            @Override
            public void onResult(NavigationRecorder.VisitData visit) {
                // When the tab is hidden we receive a notification with no end URL.
                assertEquals("about:blank", visit.endUrl.getSpec());
                callback.notifyCalled();
            }
        });

        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mInitialTab.goBack(); });

        callback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenHidden() throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(mNavUrl, new Callback<NavigationRecorder.VisitData>() {
            @Override
            public void onResult(NavigationRecorder.VisitData visit) {
                // When the tab is hidden we receive a notification with no end URL.
                assertNull(visit.endUrl);
                callback.notifyCalled();
            }
        });

        mTestSetupRule.loadUrlInNewTab(null);
        callback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenURLTyped() throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(mNavUrl, new Callback<NavigationRecorder.VisitData>() {
            @Override
            public void onResult(NavigationRecorder.VisitData visit) {
                // When the visit is hidden because of the transition type we get no URL.
                assertNull(visit.endUrl);
                callback.notifyCalled();
            }
        });

        mTestSetupRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/simple.html"));
        callback.waitForCallback(0);
    }

    /** Loads the provided URL in the current tab and sets up navigation recording for it. */
    private void loadUrlAndRecordVisit(
            final String url, Callback<NavigationRecorder.VisitData> visitCallback) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            int status = mInitialTab.loadUrl(new LoadUrlParams(url));
            Log.e(TAG, "loadUrl status=" + status);
            NavigationRecorder.record(mInitialTab, visitCallback);
        });
    }
}
