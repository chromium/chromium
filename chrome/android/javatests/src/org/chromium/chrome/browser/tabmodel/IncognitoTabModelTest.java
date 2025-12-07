// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/** Tests for IncognitoTabModel. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/439491767): Fix broken tests caused by desktop-like incognito window.
@DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
public class IncognitoTabModelTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock Callback<Tab> mTabSupplierObserver;
    @Mock Callback<Integer> mTabCountSupplierObserver;

    private TabModel mRegularTabModel;
    private TabModel mIncognitoTabModel;
    private WebPageStation mPage;

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
        mRegularTabModel = mPage.getTabModelSelector().getModel(false);
        mIncognitoTabModel = mPage.getTabModelSelector().getModel(true);
    }

    private class CloseAllDuringAddTabTabModelObserver implements TabModelObserver {
        @Override
        public void willAddTab(Tab tab, @TabLaunchType int type) {
            mIncognitoTabModel
                    .getTabRemover()
                    .closeTabs(TabClosureParams.closeAllTabs().build(), /* allowDialog= */ false);
        }
    }

    private void createTabOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabCreator(true)
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                });
    }

    private void removeTabOnUiThread() {
        Tab tab = mActivityTestRule.getActivityTab();
        assertTrue(tab.isIncognito());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.getTabRemover().removeTab(tab, /* allowDialog= */ false);
                    tab.destroy();
                });
    }

    /**
     * Verify that a close all operation that occurs while a tab is being added does not crash the
     * browser and results in 1 valid tab. This test simulates the case where the user selects
     * "Close all incognito tabs" then quickly clicks the "+" button to add a new incognito tab. See
     * crbug.com/496651.
     */
    @Test
    @SmallTest
    @Feature({"OffTheRecord"})
    public void testCloseAllDuringAddTabDoesNotCrash() {
        createTabOnUiThread();
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mIncognitoTabModel.addObserver(new CloseAllDuringAddTabTabModelObserver()));

        createTabOnUiThread();
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void testRecreateInIncognito() {
        createTabOnUiThread();
        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                () -> {
                    return TabStateExtractor.from(mActivityTestRule.getActivity().getActivityTab())
                                    .contentsState
                            != null;
                });
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(() -> newActivity.getTabModelSelector().isIncognitoSelected());
    }

    @Test
    @SmallTest
    public void testRemoveLastTab() throws TimeoutException {
        CallbackHelper didAddTabCallbackHelper = new CallbackHelper();
        CallbackHelper tabRemovedCallbackHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void tabRemoved(Tab tab) {
                                    tabRemovedCallbackHelper.notifyCalled();
                                }

                                @Override
                                public void didAddTab(
                                        Tab tab,
                                        @TabLaunchType int type,
                                        @TabCreationState int creationState,
                                        boolean markedForSelection) {
                                    didAddTabCallbackHelper.notifyCalled();
                                }
                            });
                });

        createTabOnUiThread();
        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                () -> {
                    return TabStateExtractor.from(mActivityTestRule.getActivity().getActivityTab())
                                    .contentsState
                            != null;
                });
        didAddTabCallbackHelper.waitForCallback(
                "TabModelObserver#didAddTab should have been called", 0);

        removeTabOnUiThread();
        tabRemovedCallbackHelper.waitForCallback(
                "TabModelObserver#tabRemoved should have been called", 0);
    }

    @Test
    @SmallTest
    public void testHideLastRegularTab_OnModelChange() {
        mActivityTestRule.skipWindowAndTabStateCleanup();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(mRegularTabModel)
                            .isSameInstanceAs(mActivityTestRule.getActivity().getCurrentTabModel());
                    // In setup we create a blank tab.
                    assertThat(mRegularTabModel.getCount()).isEqualTo(1);
                    assertThat(mIncognitoTabModel.getCount()).isEqualTo(0);

                    Tab mTab = mRegularTabModel.getTabAt(0);
                    assertFalse(mTab.isHidden());
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelectorSupplier()
                            .get()
                            .selectModel(true);
                    assertTrue(mTab.isHidden());
                });
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplierAddedBefore() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.getCurrentTabSupplier().addObserver(mTabSupplierObserver);
                });
        verifyNoInteractions(mTabSupplierObserver);

        createTabOnUiThread();
        verify(mTabSupplierObserver).onResult(notNull());

        removeTabOnUiThread();
        verify(mTabSupplierObserver).onResult(isNull());
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplierAddedAfter() {
        createTabOnUiThread();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.getCurrentTabSupplier().addObserver(mTabSupplierObserver);
                });
        verify(mTabSupplierObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onResult(notNull());

        removeTabOnUiThread();
        verify(mTabSupplierObserver).onResult(isNull());
    }

    @Test
    @SmallTest
    public void testTabCountSupplierAddedBefore() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.getTabCountSupplier().addObserver(mTabCountSupplierObserver);
                });
        verify(mTabCountSupplierObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onResult(0);

        createTabOnUiThread();
        verify(mTabCountSupplierObserver).onResult(1);

        removeTabOnUiThread();
        verify(mTabCountSupplierObserver, times(2)).onResult(0);
    }

    @Test
    @SmallTest
    public void testTabCountSupplierAddedAfter() {
        createTabOnUiThread();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.getTabCountSupplier().addObserver(mTabCountSupplierObserver);
                });
        verify(mTabCountSupplierObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onResult(1);

        removeTabOnUiThread();
        verify(mTabCountSupplierObserver).onResult(0);
    }
}
