// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Tests for ChromeTabCreator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ChromeTabCreatorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = sTestServerRule.getServer();
    }

    /** Verify that tabs opened in background on low-end are loaded lazily. */
    @Test
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackgroundLowEnd() throws ExecutionException {
        final Tab fgTab = sActivityTestRule.getActivity().getActivityTab();
        final Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Tab>() {
                            @Override
                            public Tab call() {
                                return sActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createNewTab(
                                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                                TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                                fgTab);
                            }
                        });

        // Verify that the background tab is not loading.
        Assert.assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        ChromeTabUtils.waitForTabPageLoaded(
                bgTab,
                mTestServer.getURL(TEST_PATH),
                new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    TabModelUtils.setIndex(
                                            sActivityTestRule.getActivity().getCurrentTabModel(),
                                            indexOf(bgTab));
                                });
                    }
                });
        Assert.assertNotNull(bgTab.getView());
    }

    /** Verify that tabs opened in background on regular devices are loaded eagerly. */
    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackground() throws ExecutionException {
        final Tab fgTab = sActivityTestRule.getActivity().getActivityTab();
        Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Tab>() {
                            @Override
                            public Tab call() {
                                return sActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createNewTab(
                                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                                TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                                fgTab);
                            }
                        });

        // Verify that the background tab is loaded.
        Assert.assertNotNull(bgTab.getView());
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(TEST_PATH));

        // Both foreground and background do not request desktop sites.
        Assert.assertFalse(
                "Should not request desktop sites by default.",
                fgTab.getWebContents().getNavigationController().getUseDesktopUserAgent());
        Assert.assertFalse(
                "Should not request desktop sites by default.",
                bgTab.getWebContents().getNavigationController().getUseDesktopUserAgent());
    }

    /** Verify that the spare WebContents is used. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabTakesSpareWebContents() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        Tab currentTab = sActivityTestRule.getActivity().getActivityTab();
                        WarmupManager.getInstance()
                                .createSpareWebContents(sActivityTestRule.getProfile(false));
                        Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents());
                        sActivityTestRule
                                .getActivity()
                                .getCurrentTabCreator()
                                .createNewTab(
                                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                        TabLaunchType.FROM_EXTERNAL_APP,
                                        currentTab);
                        Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
                    }
                });
    }

    /** Verify that the tab position is set using the intent. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabTakesPositonIndex() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        Tab currentTab = sActivityTestRule.getActivity().getActivityTab();
                        Tab tabOne =
                                sActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createNewTab(
                                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                                TabLaunchType.FROM_EXTERNAL_APP,
                                                currentTab);
                        Tab tabTwo =
                                sActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createNewTab(
                                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                                TabLaunchType.FROM_LINK,
                                                null,
                                                createIntent(/* tabIndex= */ 0)); // At the start.
                        Assert.assertFalse(
                                "The second/last tab should be the first in the list.",
                                0 == indexOf(tabTwo));
                        Assert.assertFalse(
                                "The current tab should now be the second in the list.",
                                1 == indexOf(currentTab));
                        Assert.assertFalse(
                                "The first tab should now be the third in the list.",
                                2 == indexOf(tabOne));
                    }
                });
    }

    /** Verify that tabs opened in background when launch type is FROM_SYNC_BACKGROUND. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabWithSyncBackgroundFrozen() throws ExecutionException {
        final String url = mTestServer.getURL(TEST_PATH);
        final String title = "BAR";
        final Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Tab tab =
                                    sActivityTestRule
                                            .getActivity()
                                            .getCurrentTabCreator()
                                            .createNewTab(
                                                    new LoadUrlParams(url),
                                                    title,
                                                    TabLaunchType.FROM_SYNC_BACKGROUND,
                                                    null,
                                                    TabModel.INVALID_TAB_INDEX);
                            return tab;
                        });
        Assert.assertEquals(title, ChromeTabUtils.getTitleOnUiThread(bgTab));

        // Verify that the background tab is not loading.
        Assert.assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        Runnable loadPage =
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModelUtils.setIndex(
                                        sActivityTestRule.getActivity().getCurrentTabModel(),
                                        indexOf(bgTab));
                            });
                };
        ChromeTabUtils.waitForTabPageLoaded(bgTab, url, loadPage);
        Assert.assertNotNull(bgTab.getView());

        // Title should change when the page loads.
        Assert.assertNotEquals(title, ChromeTabUtils.getTitleOnUiThread(bgTab));
    }

    private Intent createIntent(int tabIndex) {
        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_TAB_INDEX, tabIndex);
        return intent;
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return sActivityTestRule.getActivity().getCurrentTabModel().indexOf(tab);
    }
}
