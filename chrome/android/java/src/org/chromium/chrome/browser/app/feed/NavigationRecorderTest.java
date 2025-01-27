// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for {@link NavigationRecorder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationRecorderTest {
    private static final String TAG = "NavRecorderTest";

    @ClassRule
    public static ChromeTabbedActivityTestRule sTestSetupRule = new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sTestSetupRule, false);

    private EmbeddedTestServer mTestServer;
    private String mNavUrl;
    private Tab mInitialTab;

    @Before
    public void setUp() {
        mTestServer = sTestSetupRule.getEmbeddedTestServerRule().getServer();
        mNavUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mInitialTab = sTestSetupRule.getActivity().getActivityTab();
                    // Add logging to debug flaky test: crbug.com/1297086.
                    mInitialTab.addObserver(
                            new EmptyTabObserver() {
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
        loadUrlAndRecordVisit(
                mNavUrl,
                new Callback<NavigationRecorder.VisitData>() {
                    @Override
                    public void onResult(NavigationRecorder.VisitData visit) {
                        // When the tab is hidden we receive a notification with no end URL.
                        assertEquals("about:blank", visit.endUrl.getSpec());
                        callback.notifyCalled();
                    }
                });

        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mInitialTab.goBack();
                });

        callback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenHidden() throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(
                mNavUrl,
                new Callback<NavigationRecorder.VisitData>() {
                    @Override
                    public void onResult(NavigationRecorder.VisitData visit) {
                        // When the tab is hidden we receive a notification with no end URL.
                        assertNull(visit.endUrl);
                        callback.notifyCalled();
                    }
                });

        sTestSetupRule.loadUrlInNewTab(null);
        callback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenURLTyped() throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(
                mNavUrl,
                new Callback<NavigationRecorder.VisitData>() {
                    @Override
                    public void onResult(NavigationRecorder.VisitData visit) {
                        // When the visit is hidden because of the transition type we get no URL.
                        assertNull(visit.endUrl);
                        callback.notifyCalled();
                    }
                });

        sTestSetupRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/simple.html"));
        callback.waitForCallback(0);
    }

    /** Loads the provided URL in the current tab and sets up navigation recording for it. */
    private void loadUrlAndRecordVisit(
            final String url, Callback<NavigationRecorder.VisitData> visitCallback) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadUrlResult result = mInitialTab.loadUrl(new LoadUrlParams(url));
                    Log.e(TAG, "loadUrl status=" + result.tabLoadStatus);
                    NavigationRecorder.record(mInitialTab, visitCallback);
                });
    }
}
