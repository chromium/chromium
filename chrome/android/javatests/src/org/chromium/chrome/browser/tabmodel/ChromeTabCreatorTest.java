// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for ChromeTabCreator.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeTabCreatorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Verify that tabs opened in background on low-end are loaded lazily.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackgroundLowEnd() throws ExecutionException {
        final Tab fgTab = mActivityTestRule.getActivity().getActivityTab();
        final Tab bgTab = TestThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                return mActivityTestRule.getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, fgTab);
            }
        });

        // Verify that the background tab is not loading.
        Assert.assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(TEST_PATH), new Runnable() {
            @Override
            public void run() {
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    TabModelUtils.setIndex(
                            mActivityTestRule.getActivity().getCurrentTabModel(), indexOf(bgTab));
                });
            }
        });
        Assert.assertNotNull(bgTab.getView());
    }

    /**
     * Verify that tabs opened in background on regular devices are loaded eagerly.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackground() throws ExecutionException {
        final Tab fgTab = mActivityTestRule.getActivity().getActivityTab();
        Tab bgTab = TestThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                return mActivityTestRule.getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, fgTab);
            }
        });

        // Verify that the background tab is loaded.
        Assert.assertNotNull(bgTab.getView());
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(TEST_PATH));
    }

    /**
     * Verify that the spare WebContents is used.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabTakesSpareWebContents() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                WarmupManager.getInstance().createSpareWebContents(!WarmupManager.FOR_CCT);
                Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents());
                mActivityTestRule.getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                        TabLaunchType.FROM_EXTERNAL_APP, currentTab);
                Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return mActivityTestRule.getActivity().getCurrentTabModel().indexOf(tab);
    }
}
