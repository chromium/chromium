// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

/** Tests for {@link TabModelImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabModelImplTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;
    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Before
    public void setUp() {
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        mTestServer = sTestServerRule.getServer();
        mTestUrl = mTestServer.getURL("/chrome/test/data/android/ok.txt");

        mActivity = sActivityTestRule.getActivity();
        final Tab tab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(tab);
    }

    private void createTabs(int tabsCount, boolean isIncognito, String url) {
        for (int i = 0; i < tabsCount; i++) {
            ChromeTabUtils.fullyLoadUrlInNewTab(
                    InstrumentationRegistry.getInstrumentation(), mActivity, url, isIncognito);
        }
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromColdStart() {
        TabModel normalTabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromColdStart_WithIncognitoTabs() throws Exception {
        createTabs(1, true, mTestUrl);

        ApplicationTestUtils.finishActivity(sActivityTestRule.getActivity());

        sActivityTestRule.startMainActivityOnBlankPage();

        TabModel normalTabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        // Tab count is 2, because startMainActivityOnBlankPage() is called twice.
        assertEquals(2, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        // No incognito tabs are restored from a cold start.
        TabModel incognitoTabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1448777")
    public void validIndexAfterRestored_FromPreviousActivity() {
        sActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = sActivityTestRule.getActivity();
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
        createTabs(1, true, mTestUrl);

        sActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel = newActivity.getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel = newActivity.getTabModelSelector().getModel(true);
        assertEquals(1, incognitoTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    public void isTabInTabGroup_detectMergedTabs() throws Exception {
        createTabs(3, false, mTestUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    final Tab tab1 = tabModel.getTabAt(0);
                    final Tab tab2 = tabModel.getTabAt(1);
                    final Tab tab3 = tabModel.getTabAt(2);

                    assertFalse(TabModelImpl.isTabInTabGroup(tab1));
                    assertFalse(TabModelImpl.isTabInTabGroup(tab2));
                    assertFalse(TabModelImpl.isTabInTabGroup(tab3));

                    ChromeTabUtils.mergeTabsToGroup(tab2, tab3);

                    assertFalse(TabModelImpl.isTabInTabGroup(tab1));
                    assertTrue(TabModelImpl.isTabInTabGroup(tab2));
                    assertTrue(TabModelImpl.isTabInTabGroup(tab3));

                    tab1.setTabGroupId(new Token(1L, 2L));
                    assertTrue(TabModelImpl.isTabInTabGroup(tab1));

                    tab1.setTabGroupId(null);
                });
    }
}
