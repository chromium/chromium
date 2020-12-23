// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link TabModelImpl}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabModelImplTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void createTabs(int tabsCount, boolean isIncognito, String url) {
        TabModel tabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);
        int oldTabsCount = tabModel.getCount();

        for (int i = 0; i < tabsCount; i++) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), isIncognito, url == null);

            if (url != null) mActivityTestRule.loadUrl(url);
        }

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModel.getCount() - oldTabsCount, Matchers.is(tabsCount));
        });
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromColdStart() {
        TabModel normalTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromColdStart_WithIncognitoTabs() throws Exception {
        createTabs(1, true, "about:blank");
        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                ()
                        -> TabStateExtractor.from(mActivityTestRule.getActivity().getActivityTab())
                                   .contentsState
                        != null);

        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityOnBlankPage();

        TabModel normalTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        // Tab count is 2, because startMainActivityOnBlankPage() is called twice.
        assertEquals(2, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        // No incognito tabs are restored from a cold start.
        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    public void
    validIndexAfterRestored_FromPreviousActivity() {
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel = newActivity.getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel = newActivity.getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromPreviousActivity_WithIncognitoTabs() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getActivityTab().setIsTabSaveEnabled(false);
        });
        createTabs(1, true, "about:blank");

        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                ()
                        -> TabStateExtractor.from(mActivityTestRule.getActivity().getActivityTab())
                                   .contentsState
                        != null);

        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel = newActivity.getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel = newActivity.getTabModelSelector().getModel(true);
        assertEquals(1, incognitoTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }
}
