// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

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
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
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
    private TabModelJniBridge mTabModelJni;

    @Before
    public void setUp() {
        mTestUrl = mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/ok.txt");
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModelJni =
                (TabModelJniBridge)
                        mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
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
                    assertEquals(1, mTabModelJni.getCount());

                    GURL url = new GURL("https://www.chromium.org");
                    mTabModelJni.openTabProgrammatically(url, 0);
                    assertEquals(2, mTabModelJni.getCount());

                    Tab tab = mTabModelJni.getTabAt(0);
                    assertNotNull(tab);
                    assertEquals(url, tab.getUrl());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            tab.getTabLaunchTypeAtCreation());
                });
    }

    @Test
    @SmallTest
    public void testDuplicateTab() {
        String url = "https://www.chromium.org";
        WebPageStation page = mPage.loadWebPageProgrammatically(url);
        page.openNewTabFast();
        // 0:Tab0 (tabToDuplicate) | 1:Tab1

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(2, mTabModelJni.getCount());
                    int index = 0;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(index);
                    GURL gurl = new GURL(url);
                    assertEquals(gurl, tabToDuplicate.getUrl());
                    assertNull(tabToDuplicate.getTabGroupId());

                    mTabModelJni.duplicateTabForTesting(index);
                    // 0:Tab0 (tabToDuplicate) | 1:Tab2 (duplicatedTab) | 2:Tab1
                    assertEquals(3, mTabModelJni.getCount());

                    Tab duplicatedTab = mTabModelJni.getTabAt(index + 1);
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(gurl, duplicatedTab.getUrl());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                    assertNull(tabToDuplicate.getTabGroupId());
                    assertNull(duplicatedTab.getTabGroupId());
                });
    }

    @Test
    @SmallTest
    public void testDuplicateTab_InsideTabGroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(2, filter);
        // 0:Tab0 | Group0: 1:Tab1 (tabToDuplicate), 2:Tab2

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    int index = 1;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(index);
                    assertNotNull(tabToDuplicate.getTabGroupId());

                    mTabModelJni.duplicateTabForTesting(index);
                    // 0:Tab0 | Group0: 1:Tab1 (tabToDuplicate), 2:Tab3 (duplicatedTab), 3:Tab2
                    assertEquals(4, mTabModelJni.getCount());

                    Tab duplicatedTab = mTabModelJni.getTabAt(index + 1);
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(tabToDuplicate.getTabGroupId(), duplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                });

        Tab tabOutsideGroup = createTab();
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab3 , 3:Tab2 (tabToDuplicate) | 4:Tab4

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(5, mTabModelJni.getCount());
                    int index = 3;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(index);
                    assertNotNull(tabToDuplicate.getTabGroupId());

                    mTabModelJni.duplicateTabForTesting(index);
                    // 0:Tab0 | Group0: 1:Tab1, 2:Tab3 , 3:Tab2 (tabToDuplicate) | 4:Tab4
                    assertEquals(6, mTabModelJni.getCount());
                    assertEquals(tabOutsideGroup, mTabModelJni.getTabAt(5));

                    Tab duplicatedTab = mTabModelJni.getTabAt(index + 1);
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(tabToDuplicate.getTabGroupId(), duplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                });
    }

    @Test
    @SmallTest
    public void testMoveTabToIndex() {
        // Programmatically set up the tab state (PT is flaky)
        createTabs(2);
        // 0:Tab0 | 1:Tab1 | 2:Tab2

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    int oldIndex = 1;
                    int newIndex = 2;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // 0:Tab0 | 1:Tab2 | 2:Tab1

                    oldIndex = 2;
                    newIndex = 0;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // 0:Tab1 || 1:Tab0 | 2:Tab2
                });

        // Group tabs and add another tab.
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Tab> group0 = Arrays.asList(mTabModelJni.getAllTabs());
                    filter.mergeListOfTabsToGroup(group0, group0.get(0), false);
                });
        createTab();
        // Group0: 0:Tab1, 1:Tab0, 2:Tab2 | 3:Tab3

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(4, mTabModelJni.getCount());
                    int oldIndex = 3; // Single tab
                    int newIndex = 2; // Index for one of the tabs in the first tab group
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 1;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 0;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // 0:Tab3 | Group0: 1:Tab1, 2:Tab0, 3:Tab2
                });

        // Add a group with 2 tabs.
        createTabGroup(2, filter);
        // 0:Tab3 | Group0: 1:Tab1, 2:Tab0, 3:Tab2 | Group1: 4:Tab4, 5:Tab5

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(6, mTabModelJni.getCount());
                    int oldIndex = 0; // Single tab
                    int newIndex = 1; // First tab group index
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 2; // Second tab group index
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 3; // Last tab group index
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // Group0: 0:Tab1, 1:Tab0, 2:Tab2 | 3:Tab3 | Group1: 4:Tab4, 5:Tab5

                    oldIndex = 3; // Single tab
                    newIndex = 4; // Index for one of the tabs in the second tab group
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 5; // Index for one of the tabs in the second tab group
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // Group0: 0:Tab1, 1:Tab0, 2:Tab2 | Group1: 3:Tab4, 4:Tab5 | 5:Tab3

                    oldIndex = 5;
                    newIndex = 4;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ false);

                    newIndex = 3;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // Group0: 0:Tab1, 1:Tab0, 2:Tab2 | 3:Tab3 | Group1: 4:Tab4, 5:Tab5
                });
    }

    @Test
    @SmallTest
    public void testMoveTabToIndex_InsideTabGroup() {
        // Programmatically set up the tab state (PT is flaky)
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(3, filter);
        createTab();
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab2, 3:Tab3 | 4:Tab4

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(5, mTabModelJni.getCount());

                    int oldIndex = 2;
                    int newIndex = 3;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ true);
                    // 0:Tab0 | Group0: 1:Tab1, 2:Tab3, 3:Tab2 | 4:Tab4

                    oldIndex = 3;
                    newIndex = 1;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ true);
                    // 0:Tab0 | Group0: 1:Tab2, 2:Tab1, 3:Tab3 | 4:Tab4

                    oldIndex = 2;
                    newIndex = 0; // Outside tab group
                    int expectedIndex = 1; // First index of the tab group
                    assertMoveTabToIndex(
                            oldIndex, newIndex, expectedIndex, /* movingInsideGroup= */ true);
                    // 0:Tab0 | Group0: 1:Tab1, 2:Tab2 , 3:Tab3 | 4:Tab4

                    oldIndex = 3;
                    newIndex = 4; // Outside tab group
                    expectedIndex = 3; // Last index of the tab group
                    assertMoveTabToIndex(
                            oldIndex, newIndex, expectedIndex, /* movingInsideGroup= */ true);
                    // 0:Tab0 | Group0: 1:Tab1, 2:Tab2 , 3:Tab3 | 4:Tab4
                });
    }

    @Test
    @SmallTest
    public void testMoveTabToIndex_TabGroupOf1() {
        // Programmatically set up the tab state (PT is flaky)
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(1, filter);
        createTabGroup(1, filter);
        createTab();

        // 0:Tab0 | Group0: 1:Tab1 | Group1: 2:Tab2 | 3:Tab3
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(4, mTabModelJni.getCount());

                    int oldIndex = 1;
                    int newIndex = 0;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ true);

                    newIndex = 2;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ true);

                    oldIndex = 2;
                    newIndex = 1;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ true);

                    newIndex = 3;
                    // No-op
                    assertMoveTabToIndex(
                            oldIndex, newIndex, oldIndex, /* movingInsideGroup= */ true);

                    oldIndex = 0;
                    newIndex = 1;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // Group0: 0:Tab1 | 1:Tab0 | Group1: 2:Tab2 | 3:Tab3

                    oldIndex = 3;
                    newIndex = 2;
                    assertMoveTabToIndex(
                            oldIndex, newIndex, newIndex, /* movingInsideGroup= */ false);
                    // Group0: 0:Tab1 | 1:Tab0 | 2:Tab3 | Group1: 3:Tab2
                });
    }

    @Test
    @SmallTest
    public void testGetAllTabs() {
        RegularNewTabPageStation secondTab = mPage.openNewTabFast();
        secondTab.openNewTabFast();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab[] tabs = mTabModelJni.getAllTabs();
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
                    assertEquals(3, mTabModelJni.getCount());
                    Tab[] tabs = mTabModelJni.getAllTabs();
                    assertEquals(3, tabs.length);

                    int i = 0;
                    for (Tab tab : mTabModelJni) {
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

    private void assertMoveTabToIndex(
            int oldIndex, int newIndex, int expectedIndex, boolean movingInsideGroup) {
        Tab oldIndexTab = mTabModelJni.getTabAt(oldIndex);
        assert movingInsideGroup || oldIndexTab.getTabGroupId() == null
                : "This is not a single tab movement";
        mTabModelJni.moveTabToIndex(oldIndex, newIndex);
        assertEquals(oldIndexTab, mTabModelJni.getTabAt(expectedIndex));
    }

    private void createTabGroup(int numberOfTabs, TabGroupModelFilter filter) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < numberOfTabs; i++) tabs.add(createTab());
        ThreadUtils.runOnUiThreadBlocking(
                () -> filter.mergeListOfTabsToGroup(tabs, tabs.get(0), false));
    }

    private Tab createTab() {
        return ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                "about:blank",
                /* incognito= */ false);
    }

    private void createTabs(int n) {
        for (int i = 0; i < n; i++) createTab();
    }
}
