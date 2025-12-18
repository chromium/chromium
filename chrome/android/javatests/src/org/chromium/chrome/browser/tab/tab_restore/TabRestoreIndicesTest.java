// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import android.view.KeyEvent;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.KeyUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for tab restoration using keyboard shortcuts. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This test relies on native initialization")
@EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
public class TabRestoreIndicesTest {
    private static final String PAGE_0 = "/chrome/test/data/android/about.html";
    private static final String PAGE_1 = "/chrome/test/data/android/simple.html";
    private static final String PAGE_2 = "/chrome/test/data/android/google.html";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private ChromeTabbedActivity mActivity;
    private WebPageStation mPage;
    private TabModel mTabModel;
    private GURL mUrl0;
    private GURL mUrl1;
    private GURL mUrl2;

    @Before
    public void setUp() {
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        mPage = mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTabModel = mActivity.getTabModelSelector().getModel(false);

        String url0 = mActivityTestRule.getTestServer().getURL(PAGE_0);
        String url1 = mActivityTestRule.getTestServer().getURL(PAGE_1);
        String url2 = mActivityTestRule.getTestServer().getURL(PAGE_2);

        mPage = mPage.loadWebPageProgrammatically(url0);
        mPage = mPage.openFakeLinkToWebPage(url1);
        mPage = mPage.openFakeLinkToWebPage(url2);

        mUrl0 = new GURL(url0);
        mUrl1 = new GURL(url1);
        mUrl2 = new GURL(url2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals("Should have 3 tabs initially", 3, mTabModel.getCount());
                    assertEquals("Tab 0 URL", mUrl0, mTabModel.getTabAt(0).getUrl());
                    assertEquals("Tab 1 URL", mUrl1, mTabModel.getTabAt(1).getUrl());
                    assertEquals("Tab 2 URL", mUrl2, mTabModel.getTabAt(2).getUrl());
                });
    }

    @Test
    @MediumTest
    public void testRestoreNonUndoableTab() {
        List<Tab> tabs = getTabs();
        final Tab tab0 = tabs.get(0);
        final Tab tab2 = tabs.get(2);
        final Tab tabToClose = tabs.get(1);

        closeTab(tabToClose);
        waitForTabCount(/* count= */ 2);

        restoreTab(/* expectedCount= */ 3, /* restoredIndex= */ 1, mUrl1);
        assertTabSame(0, tab0);
        assertRestoredTab(1, mUrl1, /* expectedPinned= */ false);
        assertTabSame(2, tab2);
    }

    @Test
    @MediumTest
    public void testRestoreNonUndoableTabs_Multiple() {
        List<Tab> tabs = getTabs();
        final Tab tab0 = tabs.get(0);
        final Tab tab1 = tabs.get(1);
        final Tab tab2 = tabs.get(2);

        closeTab(tab1);
        closeTab(tab2);
        waitForTabCount(/* count= */ 1);

        restoreTab(/* expectedCount= */ 2, /* restoredIndex= */ 1, mUrl2);
        restoreTab(/* expectedCount= */ 3, /* restoredIndex= */ 1, mUrl1);
        assertTabSame(0, tab0);
        assertRestoredTab(1, mUrl1, /* expectedPinned= */ false);
        assertRestoredTab(2, mUrl2, /* expectedPinned= */ false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testRestoreNonUndoableTab_Pinned() {
        List<Tab> tabs = getTabs();
        final Tab tab0 = tabs.get(0);
        final Tab tab1 = tabs.get(1);
        final Tab tab2 = tabs.get(2);

        pinTab(tab0);

        closeTab(tab0);
        waitForTabCount(/* count= */ 2);

        restoreTab(/* expectedCount= */ 3, /* restoredIndex= */ 0, mUrl0);
        assertRestoredTab(0, mUrl0, /* expectedPinned= */ true);
        assertTabSame(1, tab1);
        assertTabSame(2, tab2);

        Tab newTab0 = getTabs().get(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Tab tab : mTabModel) {
                        if (!tab.getIsPinned()) {
                            mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);
                        }
                    }
                });

        closeTab(tab1);
        waitForTabCount(/* count= */ 2);

        restoreTab(/* expectedCount= */ 3, /* restoredIndex= */ 1, mUrl1);
        assertTabSame(0, newTab0);
        assertRestoredTab(1, mUrl1, /* expectedPinned= */ true);
        assertTabSame(2, tab2);

        unpinTabs();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testRestoreNonUndoableTabs_Multiple_Pinned() {
        List<Tab> tabs = getTabs();
        final Tab tab0 = tabs.get(0);
        final Tab tab1 = tabs.get(1);
        final Tab tab2 = tabs.get(2);

        pinTab(tab0);
        pinTab(tab1);

        closeTab(tab0);
        closeTab(tab1);
        waitForTabCount(/* count= */ 1);

        restoreTab(/* expectedCount= */ 2, /* restoredIndex= */ 0, mUrl1);
        restoreTab(/* expectedCount= */ 3, /* restoredIndex= */ 0, mUrl0);
        assertRestoredTab(0, mUrl0, /* expectedPinned= */ true);
        assertRestoredTab(1, mUrl1, /* expectedPinned= */ true);
        assertTabSame(2, tab2);

        unpinTabs();
    }

    private void assertTabSame(int index, Tab expectedTab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertSame(
                            "Tab at index " + index + " should be the same object",
                            expectedTab,
                            mTabModel.getTabAt(index));
                });
    }

    private void assertRestoredTab(int index, GURL expectedUrl, boolean expectedPinned) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mTabModel.getTabAt(index);
                    assertEquals(
                            "Tab at index " + index + " has incorrect URL",
                            expectedUrl,
                            tab.getUrl());
                    assertEquals(
                            "Tab at index " + index + " has incorrect pinned state",
                            expectedPinned,
                            tab.getIsPinned());
                });
    }

    private void closeTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });
    }

    private void pinTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);
                });
    }

    private void waitForTabCount(int count) {
        CriteriaHelper.pollUiThread(
                () -> mTabModel.getCount() == count, "Tab count should be " + count);
    }

    private void restoreTab(int expectedCount, int restoredIndex, GURL restoredUrl) {
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(),
                mActivity.getWindow().getDecorView(),
                KeyEvent.KEYCODE_T,
                KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
        waitForTabCount(expectedCount);
        ChromeTabUtils.waitForTabPageLoaded(
                ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(restoredIndex)),
                restoredUrl.getSpec());
    }

    private void unpinTabs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Tab tab : mTabModel) {
                        mTabModel.unpinTab(tab.getId());
                    }
                });
    }

    private List<Tab> getTabs() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> TabModelUtils.convertTabListToListOfTabs(mTabModel));
    }
}
