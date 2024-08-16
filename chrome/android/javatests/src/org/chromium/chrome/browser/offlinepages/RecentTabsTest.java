// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/** Integration tests for the Last 1 feature of Offline Pages. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class RecentTabsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Ensure we start in an offline state.
                    NetworkChangeNotifier.forceConnectivityState(false);
                    if (!NetworkChangeNotifier.isInitialized()) {
                        NetworkChangeNotifier.init();
                    }
                });

        mOfflinePageBridge = OfflineTestUtil.getOfflinePageBridge();
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNPageSavedWhenTabSwitched() throws Exception {
        // The tab of interest.
        Tab tab = sActivityTestRule.loadUrlInNewTab(mTestPage);

        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        Assert.assertNull(OfflineTestUtil.getPageByClientId(firstTabClientId));

        // Note, that switching to a new tab must occur after the SnapshotController believes the
        // page quality is good enough.  With the debug flag, the delay after DomContentLoaded is 0
        // so we can definitely snapshot after onload (which is what |loadUrlInNewTab| waits for).

        // Switch to a new tab to cause the WebContents hidden event.
        sActivityTestRule.loadUrlInNewTab("about:blank");

        waitForPageWithClientId(firstTabClientId);
    }

    /**
     * Note: this test relies on a sleeping period because some of the taking actions are
     * complicated to track otherwise, so there is the possibility of flakiness. I chose 100ms from
     * local testing and I expect it to be "safe" but it flakiness is detected it might have to be
     * further increased.
     */
    @Test
    @CommandLineFlags.Add("short-offline-page-snapshot-delay-for-test")
    @MediumTest
    public void testLastNClosingTabIsNotSaved() throws Exception {
        // Create the tab of interest.
        final Tab tab = sActivityTestRule.loadUrlInNewTab(mTestPage);
        final ClientId firstTabClientId =
                new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tab.getId()));

        // The tab should be foreground and so no snapshot should exist.
        TabModelSelector tabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        Assert.assertEquals(tabModelSelector.getCurrentTab(), tab);
        Assert.assertFalse(tab.isHidden());
        Assert.assertNull(OfflineTestUtil.getPageByClientId(firstTabClientId));

        // The tab model is expected to support pending closures.
        final TabModel tabModel = tabModelSelector.getModelForTabId(tab.getId());
        Assert.assertTrue(tabModel.supportsPendingClosures());

        // Requests closing of the tab allowing for closure undo and checks it's actually closing.
        boolean closeTabReturnValue =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return tabModel.closeTabs(TabClosureParams.closeTab(tab).build());
                            }
                        });
        Assert.assertTrue(closeTabReturnValue);
        Assert.assertTrue(tab.isHidden());
        Assert.assertTrue(tab.isClosing());

        // Wait a bit and checks that no snapshot was created.
        Thread.sleep(100); // Note: Flakiness potential here.
        Assert.assertNull(OfflineTestUtil.getPageByClientId(firstTabClientId));

        // Undo the closure and make sure the tab is again the current one on foreground.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.cancelTabClosure(tab.getId());
                    int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                    TabModelUtils.setIndex(tabModel, tabIndex);
                });
        Assert.assertFalse(tab.isHidden());
        Assert.assertFalse(tab.isClosing());
        Assert.assertEquals(tabModelSelector.getCurrentTab(), tab);

        // Finally switch to a new tab and check that a snapshot is created.
        Tab newTab = sActivityTestRule.loadUrlInNewTab("about:blank");
        Assert.assertEquals(tabModelSelector.getCurrentTab(), newTab);
        Assert.assertTrue(tab.isHidden());
        waitForPageWithClientId(firstTabClientId);
    }

    private void waitForPageWithClientId(final ClientId clientId) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return OfflineTestUtil.getPageByClientId(clientId) != null;
                });
    }
}
