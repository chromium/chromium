// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for ChromeTabCreator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ChromeTabCreatorTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String TEST_PATH_2 = "/chrome/test/data/android/simple.html";

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();
        mPage = mActivityTestRule.startOnBlankPage();
        IntentUtils.setForceIsTrustedIntentForTesting(/* isTrusted= */ true);
    }

    /** Verify that tabs opened in background on low-end are loaded lazily. */
    @Test
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackgroundLowEnd() {
        final Tab fgTab = mPage.loadedTabElement.value();
        final Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            fgTab);
                        });

        // Verify that the background tab is not loading.
        assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        ChromeTabUtils.waitForTabPageLoaded(
                bgTab,
                mTestServer.getURL(TEST_PATH),
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModelUtils.setIndex(
                                        mActivityTestRule.getActivity().getCurrentTabModel(),
                                        indexOf(bgTab));
                            });
                });
        assertNotNull(bgTab.getView());
    }

    /** Verify that tabs opened in background on regular devices are loaded eagerly. */
    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackground() {
        final Tab fgTab = mPage.loadedTabElement.value();
        Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            fgTab);
                        });

        // Verify that the background tab is loaded.
        assertNotNull(bgTab.getView());
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(TEST_PATH));

        // Both foreground and background do not request desktop sites.
        assertFalse(
                "Should not request desktop sites by default.",
                fgTab.getWebContents().getNavigationController().getUseDesktopUserAgent());
        assertFalse(
                "Should not request desktop sites by default.",
                bgTab.getWebContents().getNavigationController().getUseDesktopUserAgent());
    }

    /** Verify that the tab position is set using the intent. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabTakesPositionIndex() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                    Tab tabOne =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_EXTERNAL_APP,
                                            currentTab);
                    Tab tabTwo =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_EXTERNAL_APP,
                                            null,
                                            createIntent(/* tabIndex= */ 0)); // At the start.
                    assertEquals(
                            "The second/last tab should be the first in the list.",
                            0,
                            indexOf(tabTwo));
                    assertEquals(
                            "The current tab should now be the second in the list.",
                            1,
                            indexOf(currentTab));
                    assertEquals(
                            "The first tab should now be the third in the list.",
                            2,
                            indexOf(tabOne));
                });
    }

    /** Verify that tabs opened in background when launch type is FROM_SYNC_BACKGROUND. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabWithSyncBackgroundFrozen() {
        final String url = mTestServer.getURL(TEST_PATH);
        final String title = "BAR";
        final Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Tab tab =
                                    mActivityTestRule
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
        assertEquals(title, ChromeTabUtils.getTitleOnUiThread(bgTab));

        // Verify that the background tab is not loading.
        assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        Runnable loadPage =
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModelUtils.setIndex(
                                        mActivityTestRule.getActivity().getCurrentTabModel(),
                                        indexOf(bgTab));
                            });
                };
        ChromeTabUtils.waitForTabPageLoaded(bgTab, url, loadPage);
        assertNotNull(bgTab.getView());

        // Title should change when the page loads.
        assertNotEquals(title, ChromeTabUtils.getTitleOnUiThread(bgTab));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testHistoryNavigationBackground() {
        final String url = mTestServer.getURL(TEST_PATH);
        final String url2 = mTestServer.getURL(TEST_PATH_2);
        mPage = mPage.loadWebPageProgrammatically(url);
        mPage = mPage.loadWebPageProgrammatically(url2);
        final ChromeTabbedActivity activity = mPage.getActivity();
        final TabModel tabModel = activity.getCurrentTabModel();
        final ObservableSupplier<Tab> currentTabSupplier = tabModel.getCurrentTabSupplier();
        final CallbackHelper createdCallback = new CallbackHelper();
        final AtomicReference<Boolean> wasSelected = new AtomicReference<>(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void didAddTab(
                                        Tab tab,
                                        @TabLaunchType int type,
                                        @TabCreationState int creationState,
                                        boolean markedForSelection) {
                                    createdCallback.notifyCalled();
                                    tabModel.removeObserver(this);
                                }

                                @Override
                                public void didSelectTab(
                                        Tab tab, @TabSelectionType int type, int lastId) {
                                    wasSelected.set(true);
                                }
                            });
                });
        final Tab parentTab = currentTabSupplier.get();
        final Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createTabWithHistory(
                                                parentTab,
                                                TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND));
        try {
            createdCallback.waitForCallback(null, 0, 1, 10, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            throw new AssertionError("Never received tab creation event", e);
        }
        assertFalse(
                "Expected new tab to be in the background (i.e. was never selected)",
                wasSelected.get());
        assertEquals(
                "Expected the new tab to have the same URL as its parent",
                parentTab.getUrl(),
                bgTab.getUrl());
        assertTrue("Expected the new tab to be able to go back", bgTab.canGoBack());

        assertEquals(
                "Expected the new tab to have the correct number of history entries",
                3,
                bgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryCount());

        assertEquals(
                "Expected the new tab's first history entry to be about:blank",
                new GURL("about:blank"),
                bgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(0)
                        .getUrl());
        assertEquals(
                "Expected the new tab's 2nd history entry to be url1",
                new GURL(url),
                bgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(1)
                        .getUrl());
        assertEquals(
                "Expected the new tab's 3nd history entry to be url2",
                new GURL(url2),
                bgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(2)
                        .getUrl());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testHistoryNavigationForeground() {
        final String url = mTestServer.getURL(TEST_PATH);
        final String url2 = mTestServer.getURL(TEST_PATH_2);
        mPage = mPage.loadWebPageProgrammatically(url);
        mPage = mPage.loadWebPageProgrammatically(url2);
        final Tab parentTab = mPage.loadedTabElement.value();
        final Tab fgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabCreator()
                                        .createTabWithHistory(
                                                parentTab,
                                                TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND));
        ObservableSupplier<Tab> currentTabSupplier =
                mActivityTestRule.getActivity().getCurrentTabModel().getCurrentTabSupplier();
        assertEquals(
                "Expected TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND to launch tab in fg",
                fgTab,
                currentTabSupplier.get());
        assertEquals(
                "Expected the new tab to have the same URL as its parent",
                parentTab.getUrl(),
                fgTab.getUrl());
        assertTrue("Expected the new tab to be able to go back", fgTab.canGoBack());

        assertEquals(
                "Expected the new tab to have the correct number of history entries",
                3,
                fgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryCount());

        assertEquals(
                "Expected the new tab's first history entry to be about:blank",
                new GURL("about:blank"),
                fgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(0)
                        .getUrl());
        assertEquals(
                "Expected the new tab's 2nd history entry to be url1",
                new GURL(url),
                fgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(1)
                        .getUrl());
        assertEquals(
                "Expected the new tab's 3nd history entry to be url2",
                new GURL(url2),
                fgTab.getWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getEntryAtIndex(2)
                        .getUrl());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabSameGroupAsParent_FromLongpressForegroundInGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                    Tab tabForGroup =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            currentTab);
                    ChromeTabUtils.mergeTabsToGroup(currentTab, tabForGroup);
                    Tab newTab =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                                            currentTab);
                    assertNotNull("Expected tab to have a tab group ID", newTab.getTabGroupId());
                    assertEquals(
                            "Expected tab to have the same tab group ID as its parent",
                            currentTab.getTabGroupId(),
                            newTab.getTabGroupId());
                });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabSameGroupAsParent_FromLongpressBackgroundInGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                    Tab tabForGroup =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            currentTab);
                    ChromeTabUtils.mergeTabsToGroup(currentTab, tabForGroup);
                    Tab newTab =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                                            currentTab);
                    assertNotNull("Expected tab to have a tab group ID", newTab.getTabGroupId());
                    assertEquals(
                            "Expected tab to have the same tab group ID as its parent",
                            currentTab.getTabGroupId(),
                            newTab.getTabGroupId());
                });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTab_ParentInGroup_FromLongpressBackground_OutsideGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                    Tab tabForGroup =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            currentTab);
                    ChromeTabUtils.mergeTabsToGroup(currentTab, tabForGroup);
                    Tab newTab =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            currentTab);
                    assertNull("Expected tab to not be in a group", newTab.getTabGroupId());
                });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTab_FromLongpressForeground_OutsideGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
                    Tab tabForGroup =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            currentTab);
                    ChromeTabUtils.mergeTabsToGroup(currentTab, tabForGroup);
                    Tab newTab =
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                            TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                                            currentTab);
                    assertNull("Expected tab to not be in a group", newTab.getTabGroupId());
                });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @RequiresRestart // Avoid having multiple windows mess up the other tests
    public void testCreateNewTabInNewWindow() {
        Tab currentTab = mActivityTestRule.getActivityTab();
        String testPath = mTestServer.getURL(TEST_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getCurrentTabCreator()
                                .createNewTab(
                                        new LoadUrlParams(testPath),
                                        TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW,
                                        currentTab));

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            CriteriaHelper.pollUiThread(
                    () ->
                            MultiWindowUtils.getInstanceCountWithFallback(
                                            PersistedInstanceType.ANY)
                                    == 2,
                    "Expected a new window to be created");
        } else {
            assertEquals(
                    "Expected a new tab to be created",
                    2,
                    getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel()));
        }
    }

    private Intent createIntent(int tabIndex) {
        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_TAB_INDEX, tabIndex);
        return intent;
    }

    /** Returns the index of the given tab in the current tab model. */
    private int indexOf(Tab tab) {
        return mActivityTestRule.getActivity().getCurrentTabModel().indexOf(tab);
    }
}
