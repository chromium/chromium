// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/** Tests for IncognitoTabModel. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoTabModelTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock Callback<Tab> mTabSupplierObserver;
    @Mock Callback<Integer> mTabCountSupplierObserver;

    private TabModel mRegularTabModel;
    private TabModel mIncognitoTabModel;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mRegularTabModel =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get().getModel(false);
        mIncognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get().getModel(true);
    }

    private class CloseAllDuringAddTabTabModelObserver implements TabModelObserver {
        @Override
        public void willAddTab(Tab tab, @TabLaunchType int type) {
            mIncognitoTabModel.closeTabs(TabClosureParams.closeAllTabs().build());
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
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        assertTrue(tab.isIncognito());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIncognitoTabModel.removeTab(tab);
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
        Assert.assertEquals(1, mIncognitoTabModel.getCount());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mIncognitoTabModel.addObserver(new CloseAllDuringAddTabTabModelObserver()));

        createTabOnUiThread();
        Assert.assertEquals(1, mIncognitoTabModel.getCount());
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
                                        int type,
                                        int creationState,
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mRegularTabModel == mActivityTestRule.getActivity().getCurrentTabModel();
                    // In setup we create a blank tab.
                    assert mRegularTabModel.getCount() == 1;
                    assert mIncognitoTabModel.getCount() == 0;

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
