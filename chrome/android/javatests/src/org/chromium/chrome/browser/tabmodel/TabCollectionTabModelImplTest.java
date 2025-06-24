// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.List;

/** Integration test for {@link TabCollectionTabModelImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@DoNotBatch(reason = "Tab closure is not implemented yet.")
@EnableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
public class TabCollectionTabModelImplTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private String mTestUrl;
    private WebPageStation mPage;
    private TabModelSelector mTabModelSelector;
    private TabModel mRegularModel;
    private TabCollectionTabModelImpl mCollectionModel;

    @Before
    public void setUp() throws Exception {
        mTestUrl = mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/ok.txt");
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mRegularModel = mTabModelSelector.getModel(/* incognito= */ false);
        if (mRegularModel instanceof TabCollectionTabModelImpl collectionModel) {
            mCollectionModel = collectionModel;
        }
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.runOnTabStateInitialized(
                            mTabModelSelector,
                            (unused) -> {
                                helper.notifyCalled();
                            });
                });
        helper.waitForOnly();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testInitialState() {
        assertTrue(mCollectionModel.isActiveModel());
        assertTrue(mCollectionModel.isInitializationComplete());
        assertTrue(mCollectionModel.isTabModelRestored());

        assertEquals(1, mCollectionModel.getCount());
        assertEquals(0, mCollectionModel.index());

        Tab currentTab = mCollectionModel.getCurrentTabSupplier().get();
        assertNotNull(currentTab);
        assertEquals(currentTab, mCollectionModel.getTabAt(0));
        assertEquals(0, mCollectionModel.indexOf(currentTab));
    }

    @Test
    @MediumTest
    public void testMoveTabCompatTest() {
        moveTabCompatTest();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
    public void testMoveTabCompatTest_Legacy() {
        moveTabCompatTest();
    }

    private void moveTabCompatTest() {
        Tab tab0 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularModel.getCurrentTabSupplier().get());
        Tab tab1 = mActivityTestRule.loadUrlInNewTab(mTestUrl, /* incognito= */ false);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mTestUrl, /* incognito= */ false);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 1);
        assertTabsInOrderAre(List.of(tab1, tab0, tab2));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 3);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 1);
        assertTabsInOrderAre(List.of(tab1, tab0, tab2));

        moveTab(tab0, 0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, -1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
    }

    private void assertTabsInOrderAre(List<Tab> tabs) {
        assertEquals(
                "Mismatched tab count",
                (long) tabs.size(),
                (long) ThreadUtils.runOnUiThreadBlocking(mRegularModel::getCount));
        for (int i = 0; i < tabs.size(); i++) {
            Tab expected = tabs.get(i);
            Tab actual = getTabAt(i);
            assertEquals(
                    "Mismatched tabs at "
                            + i
                            + " expected, "
                            + expected.getId()
                            + " was, "
                            + actual.getId(),
                    expected,
                    actual);
        }
    }

    private Tab getTabAt(int index) {
        return ThreadUtils.runOnUiThreadBlocking(() -> mRegularModel.getTabAt(index));
    }

    private void moveTab(Tab tab, int index) {
        ThreadUtils.runOnUiThreadBlocking(() -> mRegularModel.moveTab(tab.getId(), index));
    }
}
