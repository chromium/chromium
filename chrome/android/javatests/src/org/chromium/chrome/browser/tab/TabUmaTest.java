// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Tab-related histogram collection.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabUmaTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Verify that Tab.StatusWhenSwitchedBackToForeground is correctly recording lazy loads.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStatusWhenSwitchedToLazyLoads() throws ExecutionException {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createTabForLazyLoad(false,
                        mActivityTestRule.getActivity().getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID,
                        new LoadUrlParams(mTestUrl));
                bgTab.initialize(null, null, new TabDelegateFactory(), true, false);
                return bgTab;
            }
        });

        String histogram = "Tab.StatusWhenSwitchedBackToForeground";
        HistogramDelta lazyLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_STATUS_LAZY_LOAD_FOR_BG_TAB);
        Assert.assertEquals(0, lazyLoadCount.getDelta()); // Sanity check.

        // Show the tab and verify that one sample was recorded in the lazy load bucket.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_USER);
            }
        });
        Assert.assertEquals(1, lazyLoadCount.getDelta());

        // Show the tab again and verify that we didn't record another sample.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_USER);
            }
        });
        Assert.assertEquals(1, lazyLoadCount.getDelta());
    }

    /**
     * Verify that Tab.BackgroundLoadStatus is correctly recorded.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabBackgroundLoadStatus() throws ExecutionException {
        String histogram = "Tab.BackgroundLoadStatus";
        HistogramDelta shownLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_SHOWN);
        HistogramDelta lostLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_LOST);
        HistogramDelta skippedLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_BACKGROUND_LOAD_SKIPPED);
        Assert.assertEquals(0, shownLoadCount.getDelta());
        Assert.assertEquals(0, lostLoadCount.getDelta());
        Assert.assertEquals(0, skippedLoadCount.getDelta());

        // Test a live tab created in background and shown.
        final Tab liveBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createLiveTab(Tab.INVALID_TAB_ID, false,
                        mActivityTestRule.getActivity().getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID, true);
                bgTab.initialize(null, null, new TabDelegateFactory(), true, false);
                bgTab.loadUrl(new LoadUrlParams(mTestUrl));
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        Assert.assertEquals(1, shownLoadCount.getDelta());
        Assert.assertEquals(0, lostLoadCount.getDelta());
        Assert.assertEquals(0, skippedLoadCount.getDelta());

        // Test a live tab killed in background before shown.
        final Tab killedBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createLiveTab(Tab.INVALID_TAB_ID, false,
                        mActivityTestRule.getActivity().getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID, true);
                bgTab.initialize(null, null, new TabDelegateFactory(), true, false);
                bgTab.loadUrl(new LoadUrlParams(mTestUrl));
                // Simulate the renderer being killed by the OS.
                ChromeTabUtils.simulateRendererKilledForTesting(bgTab, false);
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        Assert.assertEquals(1, shownLoadCount.getDelta());
        Assert.assertEquals(1, lostLoadCount.getDelta());
        Assert.assertEquals(0, skippedLoadCount.getDelta());

        // Test a tab created in background but not loaded eagerly.
        final Tab frozenBgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                Tab bgTab = Tab.createTabForLazyLoad(false,
                        mActivityTestRule.getActivity().getWindowAndroid(),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, Tab.INVALID_TAB_ID,
                        new LoadUrlParams(mTestUrl));
                bgTab.initialize(null, null, new TabDelegateFactory(), true, false);
                bgTab.show(TabSelectionType.FROM_USER);
                return bgTab;
            }
        });
        Assert.assertEquals(1, shownLoadCount.getDelta());
        Assert.assertEquals(1, lostLoadCount.getDelta());
        Assert.assertEquals(1, skippedLoadCount.getDelta());

        // Show every tab again and make sure we didn't record more samples - this metric should be
        // recorded only on first display.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                liveBgTab.show(TabSelectionType.FROM_USER);
                killedBgTab.show(TabSelectionType.FROM_USER);
                frozenBgTab.show(TabSelectionType.FROM_USER);
            }
        });
        Assert.assertEquals(1, shownLoadCount.getDelta());
        Assert.assertEquals(1, lostLoadCount.getDelta());
        Assert.assertEquals(1, skippedLoadCount.getDelta());
    }
}
