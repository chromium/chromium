// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;

/** Tests for Tab class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
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
        assertFalse(
                "The tab context cannot be an activity",
                mTab.getContentView().getContext() instanceof Activity);
        assertNotSame(
                "The tab context's theme should have been updated",
                mTab.getContentView().getContext().getTheme(),
                mActivityTestRule.getActivity().getApplication().getTheme());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTitleDelayUpdate() throws Throwable {
        final String oldTitle = "oldTitle";
        final String newTitle = "newTitle";

        mActivityTestRule.loadUrl(
                "data:text/html;charset=utf-8,<html><head><title>"
                        + oldTitle
                        + "</title></head><body/></html>");
        assertEquals(
                "title does not match initial title",
                oldTitle,
                ChromeTabUtils.getTitleOnUiThread(mTab));
        int currentCallCount = mOnTitleUpdatedHelper.getCallCount();
        mActivityTestRule.runJavaScriptCodeInCurrentTab("document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        assertEquals("title does not update", newTitle, ChromeTabUtils.getTitleOnUiThread(mTab));
    }

    /**
     * Verifies a Tab's contents is restored when the Tab is foregrounded after its contents have
     * been destroyed while backgrounded. Note that document mode is explicitly disabled, as the
     * document activity may be fully recreated if its contents is killed while in the background.
     */
    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabRestoredIfKilledWhileActivityStopped() throws Exception {
        // Ensure the tab is showing before stopping the activity.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTab.show(TabSelectionType.FROM_NEW, TabLoadIfNeededCaller.OTHER));

        assertFalse(mTab.needsReload());
        assertFalse(mTab.isHidden());
        assertFalse(isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ChromeApplicationTestUtils.fireHomeScreenIntent(
                ApplicationProvider.getApplicationContext());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.simulateRendererKilledForTesting(mTab));

        CriteriaHelper.pollUiThread(mTab::isHidden);
        assertTrue(mTab.needsReload());
        assertFalse(isShowingSadTab());

        ChromeApplicationTestUtils.launchChrome(ApplicationProvider.getApplicationContext());

        // The tab should be restored and visible.
        CriteriaHelper.pollUiThread(() -> !mTab.isHidden());
        assertFalse(mTab.needsReload());
        assertFalse(isShowingSadTab());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabAttachment() {
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());

        detachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertTrue(mTab.isDetached());

        attachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testNativePageTabAttachment() {
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        RecentTabsPageTestUtils.waitForRecentTabsPageLoaded(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());

        detachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertTrue(mTab.isDetached());

        attachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFrozenTabAttachment() {
        Tab tab = createSecondFrozenTab();
        assertNull(tab.getWebContents());
        assertFalse(tab.isDetached());

        detachOnUiThread(tab);
        assertNull(tab.getWebContents());
        assertTrue(tab.isDetached());

        attachOnUiThread(tab);
        assertNull(tab.getWebContents());
        assertFalse(tab.isDetached());
    }

    private void detachOnUiThread(Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContents webContents = tab.getWebContents();
                    if (webContents != null) webContents.setTopLevelNativeWindow(null);
                    tab.updateAttachment(/* window= */ null, /* tabDelegateFactory= */ null);
                });
    }

    private void attachOnUiThread(Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WindowAndroid window = mActivityTestRule.getActivity().getWindowAndroid();
                    WebContents webContents = tab.getWebContents();
                    if (webContents != null) webContents.setTopLevelNativeWindow(window);
                    tab.updateAttachment(window, /* tabDelegateFactory= */ null);
                });
    }

    private Tab createSecondFrozenTab() {
        Tab tab =
                mActivityTestRule.loadUrlInNewTab(
                        mActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/about.html"),
                        /* incognito= */ false);
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    mActivityTestRule.getActivity().getCurrentTabModel().closeTab(tab);
                    return mActivityTestRule
                            .getActivity()
                            .getCurrentTabCreator()
                            .createFrozenTab(state, tab.getId(), /* index= */ 1);
                });
    }
}
