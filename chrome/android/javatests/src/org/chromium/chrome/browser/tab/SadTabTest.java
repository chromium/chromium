// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.filters.SmallTest;
import android.widget.Button;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.ExecutionException;

/**
 * Tests related to the sad tab logic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SadTabTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private static boolean isShowingSadTab(Tab tab) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> SadTab.isShowing(tab));
        } catch (ExecutionException e) {
            return false;
        }
    }

    /**
     * Verify that the sad tab is shown when the renderer crashes.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabShownWhenRendererProcessKilled() {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        Assert.assertTrue(isShowingSadTab(tab));
    }

    /**
     * Verify that the sad tab is not shown when the renderer crashes in the background or the
     * renderer was killed by the OS out-of-memory killer.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNotShownWhenRendererProcessKilledInBackround() {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, false);
        Assert.assertFalse(isShowingSadTab(tab));
    }

    /**
     * Verify that a tab navigating to a page that is killed in the background is reloaded.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabReloadAfterKill() throws Throwable {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String url1 = webServer.setEmptyResponse("/page1.html");
            mActivityTestRule.loadUrl(url1);
            Assert.assertFalse(tab.needsReload());
            simulateRendererKilled(tab, false);
            Assert.assertTrue(tab.needsReload());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Verify that a tab killed in the background is not reloaded if another load has started.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNoReloadAfterLoad() throws Throwable {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String url1 = webServer.setEmptyResponse("/page1.html");
            final String url2 = webServer.setEmptyResponse("/page2.html");
            mActivityTestRule.loadUrl(url1);
            Assert.assertFalse(tab.needsReload());
            simulateRendererKilled(tab, false);
            mActivityTestRule.loadUrl(url2);
            Assert.assertFalse(tab.needsReload());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Confirm that after a successive refresh of a failed tab that failed to load, change the
     * button from "Reload" to "Send Feedback". If reloaded a third time and it is successful it
     * reverts from "Send Feedback" to "Reload".
     * @throws InterruptedException
     * @throws IllegalArgumentException
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabPageButtonText() throws IllegalArgumentException, InterruptedException {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        Assert.assertTrue(isShowingSadTab(tab));
        String actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals("Expected the sad tab button to have the reload label",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);

        reloadSadTab(tab);
        Assert.assertTrue(isShowingSadTab(tab));
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the feedback label after the tab button "
                        + "crashes twice in a row.",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_send_feedback_label),
                actualText);
        mActivityTestRule.loadUrl("about:blank");
        Assert.assertFalse(
                "Expected about:blank to destroy the sad tab however the sad tab is still in "
                        + "view",
                isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the reload label after a successful load",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);
    }

    /**
     * Helper method that kills the renderer on a UI thread.
     */
    private static void simulateRendererKilled(final Tab tab, final boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (!visible) tab.hide(TabHidingType.CHANGED_TABS);
                ChromeTabUtils.simulateRendererKilledForTesting(tab, false);
            }
        });
    }

    /**
     * Helper method that reloads a tab with a SadTabView currently displayed.
     */
    private static void reloadSadTab(final Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SadTab sadTab = SadTab.from(tab);
                sadTab.removeIfPresent();
                sadTab.show();
            }
        });
    }

    /**
     * If there is a SadTabView, this method will get the button for the sad tab.
     * @param tab The tab that needs to contain a SadTabView.
     * @return Returns the button that is on the SadTabView, null if SadTabView.
     *         doesn't exist.
     */
    private static Button getSadTabButton(Tab tab) {
        return (Button) tab.getContentView().findViewById(R.id.sad_tab_button);
    }

}
