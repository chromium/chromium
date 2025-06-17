// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroidJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for {@link TabModelImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabModelImplTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureDevicesDispatcherAndroidJni;

    private String mTestUrl;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mTestUrl = mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/ok.txt");
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @Test
    @SmallTest
    public void validIndexAfterRestored_FromColdStart() {
        TabModel normalTabModel = mPage.getTabModelSelector().getModel(false);
        assertEquals(1, normalTabModel.getCount());
        assertNotEquals(TabModel.INVALID_TAB_INDEX, normalTabModel.index());

        TabModel incognitoTabModel = mPage.getTabModelSelector().getModel(true);
        assertEquals(0, incognitoTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, incognitoTabModel.index());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/410945407")
    public void validIndexAfterRestored_FromColdStart_WithIncognitoTabs() {
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, List.of(mTestUrl));

        ApplicationTestUtils.finishActivity(mPage.getActivity());

        mActivityTestRule.getActivityTestRule().startMainActivityOnBlankPage();

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
    @DisabledTest(message = "https://crbug.com/1448777")
    public void validIndexAfterRestored_FromPreviousActivity() {
        mActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
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
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, List.of(mTestUrl));

        mActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
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
    public void testTabRemover_RemoveTab() {
        mPage = Journeys.createRegularTabsWithWebPages(mPage, List.of(mTestUrl));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals(2, tabModel.getCount());

                    Tab tab1 = tabModel.getTabAt(1);
                    assertNotNull(tab1);

                    tabModel.getTabRemover().removeTab(tab1, /* allowDialog= */ false);
                    assertEquals(1, tabModel.getCount());

                    assertFalse(tab1.isClosing());
                    assertFalse(tab1.isDestroyed());

                    // Reattach to avoid leak.
                    tabModel.addTab(
                            tab1,
                            TabModel.INVALID_TAB_INDEX,
                            TabLaunchType.FROM_REPARENTING,
                            TabCreationState.LIVE_IN_BACKGROUND);
                });
    }

    @Test
    @SmallTest
    public void testTabRemover_CloseTabs() {
        mPage = Journeys.createRegularTabsWithWebPages(mPage, List.of(mTestUrl));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals(2, tabModel.getCount());

                    Tab tab1 = tabModel.getTabAt(1);
                    assertNotNull(tab1);

                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab1).allowUndo(false).build(),
                                    /* allowDialog= */ true);
                    assertEquals(1, tabModel.getCount());

                    assertTrue(tab1.isDestroyed());
                });
    }

    @Test
    @SmallTest
    public void testOpenTabProgrammatically() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelJniBridge tabModel =
                            (TabModelJniBridge)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelSelector()
                                            .getModel(false);
                    assertEquals(1, tabModel.getCount());

                    GURL url = new GURL("https://www.chromium.org");
                    tabModel.openTabProgrammatically(url, 0);
                    assertEquals(2, tabModel.getCount());

                    Tab tab = tabModel.getTabAt(0);
                    assertNotNull(tab);
                    assertEquals(url, tab.getUrl());
                });
    }

    @Test
    @SmallTest
    public void testGetAllTabs() {
        RegularNewTabPageStation secondTab = mPage.openNewTabFast();
        secondTab.openNewTabFast();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelJniBridge tabModel =
                            (TabModelJniBridge)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelSelector()
                                            .getModel(false);

                    assertEquals(3, tabModel.getCount());
                    Tab[] tabs = tabModel.getAllTabs();
                    assertEquals(3, tabs.length);
                });
    }

    @Test
    @SmallTest
    public void testIterator() {
        RegularNewTabPageStation secondTab = mPage.openNewTabFast();
        secondTab.openNewTabFast();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelJniBridge tabModel =
                            (TabModelJniBridge)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelSelector()
                                            .getModel(false);

                    assertEquals(3, tabModel.getCount());
                    Tab[] tabs = tabModel.getAllTabs();
                    assertEquals(3, tabs.length);

                    int i = 0;
                    for (Tab tab : tabModel) {
                        assertEquals(tabs[i], tab);
                        i++;
                    }
                });
    }

    @Test
    @SmallTest
    public void testFreezeTabOnCloseIfCapturingForMedia() {
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(
                mMediaCaptureDevicesDispatcherAndroidJni);
        when(mMediaCaptureDevicesDispatcherAndroidJni.isCapturingAudio(any())).thenReturn(true);

        mPage = Journeys.createRegularTabsWithWebPages(mPage, List.of(mTestUrl));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals(2, tabModel.getCount());
                    Tab tab = tabModel.getTabAt(1);
                    assertFalse(tab.isFrozen());
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).build(),
                                    /* allowDialog= */ false);

                    // Tab should be frozen as a result.
                    assertTrue(tab.isFrozen());
                });
    }
}
