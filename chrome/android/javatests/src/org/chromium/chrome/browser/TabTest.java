// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for Tab class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab tab) {
            mOnTitleUpdatedHelper.notifyCalled();
        }
    };

    private boolean isShowingSadTab() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> SadTab.isShowing(mTab));
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(mTabObserver));
        mOnTitleUpdatedHelper = new CallbackHelper();
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabContext() {
        Assert.assertFalse("The tab context cannot be an activity",
                mTab.getContentView().getContext() instanceof Activity);
        Assert.assertNotSame("The tab context's theme should have been updated",
                mTab.getContentView().getContext().getTheme(),
                mActivityTestRule.getActivity().getApplication().getTheme());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTitleDelayUpdate() throws Throwable {
        final String oldTitle = "oldTitle";
        final String newTitle = "newTitle";

        mActivityTestRule.loadUrl("data:text/html;charset=utf-8,<html><head><title>" + oldTitle
                + "</title></head><body/></html>");
        Assert.assertEquals("title does not match initial title", oldTitle,
                ChromeTabUtils.getTitleOnUiThread(mTab));
        int currentCallCount = mOnTitleUpdatedHelper.getCallCount();
        mActivityTestRule.runJavaScriptCodeInCurrentTab("document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals(
                "title does not update", newTitle, ChromeTabUtils.getTitleOnUiThread(mTab));
    }

    /**
     * Verifies a Tab's contents is restored when the Tab is foregrounded
     * after its contents have been destroyed while backgrounded.
     * Note that document mode is explicitly disabled, as the document activity
     * may be fully recreated if its contents is killed while in the background.
     */
    @Test
    @SmallTest
    @Feature({"Tab"})
    @DisabledTest(message = "https://crbug.com/1090378")
    public void testTabRestoredIfKilledWhileActivityStopped() throws Exception {
        // Ensure the tab is showing before stopping the activity.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTab.show(TabSelectionType.FROM_NEW, LoadIfNeededCaller.OTHER));

        Assert.assertFalse(mTab.needsReload());
        Assert.assertFalse(mTab.isHidden());
        Assert.assertFalse(isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ChromeApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.simulateRendererKilledForTesting(mTab));

        CriteriaHelper.pollUiThread(mTab::isHidden);
        Assert.assertTrue(mTab.needsReload());
        Assert.assertFalse(isShowingSadTab());

        ChromeApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());

        // The tab should be restored and visible.
        CriteriaHelper.pollUiThread(() -> !mTab.isHidden());
        Assert.assertFalse(mTab.needsReload());
        Assert.assertFalse(isShowingSadTab());
    }
}
