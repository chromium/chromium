// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

/** Instrumentation tests for {@link NavigationRecorder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationRecorderTest {
    private static final String TAG = "NavRecorderTest";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Mock private NavigationRecorder.Natives mNavigationRecorderJniMock;

    private EmbeddedTestServer mTestServer;
    private String mNavUrl;
    private Tab mInitialTab;
    private Profile mProfile;
    private int mSurfaceId;
    private CallbackHelper mReportCompleteCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mTestServer = mActivityTestRule.getTestServer();
        mNavUrl = mTestServer.getURL("/chrome/test/data/android/google.html");
        NavigationRecorderJni.setInstanceForTesting(mNavigationRecorderJniMock);
        mReportCompleteCallback = new CallbackHelper();

        doAnswer(
                        invocation -> {
                            mReportCompleteCallback.notifyCalled();
                            return null;
                        })
                .when(mNavigationRecorderJniMock)
                .reportOpenVisitComplete(any(), anyInt(), anyLong());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mInitialTab = mActivityTestRule.getActivity().getActivityTab();
                    // Add logging to debug flaky test: crbug.com/40822096.
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
                    mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
                    mSurfaceId = 1;
                });
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWithBack() throws Exception {
        loadUrlAndRecordVisit(mNavUrl, mProfile, mSurfaceId);

        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mInitialTab.goBack();
                });
        mReportCompleteCallback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenHidden() throws Exception {
        loadUrlAndRecordVisit(mNavUrl, mProfile, mSurfaceId);

        mActivityTestRule.loadUrlInNewTab(null);
        mReportCompleteCallback.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testRecordVisitInCurrentTabEndsWhenURLTyped() throws Exception {
        loadUrlAndRecordVisit(mNavUrl, mProfile, mSurfaceId);

        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/simple.html"));
        mReportCompleteCallback.waitForCallback(0);
    }

    /** Loads the provided URL in the current tab and sets up navigation recording for it. */
    private void loadUrlAndRecordVisit(final String url, Profile profile, int sourceId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadUrlResult result = mInitialTab.loadUrl(new LoadUrlParams(url));
                    Log.e(TAG, "loadUrl status=" + result.tabLoadStatus);
                    NavigationRecorder.record(mInitialTab, profile, sourceId);
                });
    }
}
