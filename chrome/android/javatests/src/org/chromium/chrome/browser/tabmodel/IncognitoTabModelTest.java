// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for IncognitoTabModel.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoTabModelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private TabModel mTabModel;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
    }

    private class CloseAllDuringAddTabTabModelObserver extends EmptyTabModelObserver {
        @Override
        public void willAddTab(Tab tab, @TabLaunchType int type) {
            mTabModel.closeAllTabs();
        }
    }

    private void createTabOnUiThread() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabCreator(true).createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
        });
    }

    /**
     * Verify that a close all operation that occurs while a tab is being added does not crash the
     * browser and results in 1 valid tab. This test simulates the case where the user selects
     * "Close all incognito tabs" then quickly clicks the "+" button to add a new incognito tab.
     * See crbug.com/496651.
     */
    @Test
    @SmallTest
    @Feature({"OffTheRecord"})
    @RetryOnFailure
    public void testCloseAllDuringAddTabDoesNotCrash() {
        createTabOnUiThread();
        Assert.assertEquals(1, mTabModel.getCount());
        mTabModel.addObserver(new CloseAllDuringAddTabTabModelObserver());
        createTabOnUiThread();
        Assert.assertEquals(1, mTabModel.getCount());
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void testRecreateInIncognito() {
        createTabOnUiThread();
        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(() -> {
            return TabState.from(mActivityTestRule.getActivity().getActivityTab()).contentsState
                    != null;
        });
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(() -> newActivity.getTabModelSelector().isIncognitoSelected());
    }
}
