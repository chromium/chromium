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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
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
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroidJni;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.ArrayList;
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

    @Mock private TabModelObserver mTabModelObserver;

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
        String url = "https://www.chromium.org/chromium-projects/";
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

                    mTabModelJni.duplicateTabForTesting(tabToDuplicate);
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

                    mTabModelJni.duplicateTabForTesting(tabToDuplicate);
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

                    mTabModelJni.duplicateTabForTesting(tabToDuplicate);
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
    public void testDuplicateTab_PinnedTab() {
        String url = "https://www.chromium.org/chromium-projects/";
        GURL gurl1 = new GURL("https://www.example.com/");
        mPage.openNewTabFast().loadWebPageProgrammatically(url);
        // 0:Tab0 | 1:Tab1 (tabToDuplicate)

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(2, mTabModelJni.getCount());
                    Tab tab0 = mTabModelJni.getTabAt(/* index= */ 0);
                    Tab tab1 = mTabModelJni.getTabAt(/* index= */ 1);

                    GURL gurl0 = new GURL(url);
                    assertEquals(gurl0, tab1.getUrl());
                    assertNull(tab1.getTabGroupId());
                    assertFalse(tab1.getIsPinned());

                    mTabModelJni.pinTab(tab1);
                    mTabModelJni.pinTab(tab0);
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(1, mTabModelJni.indexOf(tab0));
                    assertTrue(tab1.getIsPinned());
                    assertTrue(tab0.getIsPinned());
                    // [0:Tab1 (tabToDuplicate)] | [1:Tab0]

                    mTabModelJni.duplicateTabForTesting(tab1);
                    assertEquals(3, mTabModelJni.getCount());
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(2, mTabModelJni.indexOf(tab0));
                    // [0:Tab1 (tabToDuplicate)] | [1:Tab2 (duplicatedTab)] | [2:Tab0]

                    // Assert duplicatedTab (tab2)
                    Tab tab2 = mTabModelJni.getTabAt(/* index= */ 1);
                    assertEquals(tab1.getId(), tab2.getParentId());
                    assertEquals(gurl0, tab2.getUrl());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            tab2.getTabLaunchTypeAtCreation());
                    assertTrue(tab2.getIsPinned());
                    assertNull(tab2.getTabGroupId());
                    assertNull(tab1.getTabGroupId());

                    mTabModelJni.openTabProgrammatically(gurl1, 3);
                    assertEquals(4, mTabModelJni.getCount());
                    Tab tab3 = mTabModelJni.getTabAt(3);
                    assertEquals(gurl1, tab3.getUrl());
                    // [0:Tab1] | [1:Tab2] | [2:Tab0] | 3:Tab3

                    mTabModelJni.unpinTab(tab1);
                    assertEquals(2, mTabModelJni.indexOf(tab1));
                    // [0:Tab2] | [1:Tab0] | 2:Tab1 | 3:Tab3

                    mTabModelJni.duplicateTabForTesting(tab2);
                    assertEquals(5, mTabModelJni.getCount());
                    assertEquals(0, mTabModelJni.indexOf(tab2));
                    assertEquals(2, mTabModelJni.indexOf(tab0));
                    assertEquals(3, mTabModelJni.indexOf(tab1));
                    assertEquals(4, mTabModelJni.indexOf(tab3));
                    // [0:Tab2 (tabToDuplicate)] | [1:Tab4 (duplicatedTab)] | [2:Tab0] | 3:Tab1
                    // | 4:Tab3

                    // Assert duplicatedTab (tab4)
                    Tab tab4 = mTabModelJni.getTabAt(/* index= */ 1);
                    assertEquals(tab2.getId(), tab4.getParentId());
                    assertEquals(gurl0, tab4.getUrl());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            tab2.getTabLaunchTypeAtCreation());
                    assertTrue(tab4.getIsPinned());
                    assertNull(tab4.getTabGroupId());
                    assertNull(tab2.getTabGroupId());

                    // Clean-up (otherwise next tests will fail)
                    mTabModelJni.unpinTab(tab2);
                    mTabModelJni.unpinTab(tab4);
                    mTabModelJni.unpinTab(tab0);
                });
    }

    @Test
    @SmallTest
    public void testPinUnpinTab() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab0 = mTabModelJni.getTabAt(/* index= */ 0);
                    Tab tab1 = mTabModelJni.getTabAt(/* index= */ 1);
                    Tab tab2 = mTabModelJni.getTabAt(/* index= */ 2);
                    assertFalse(tab0.getIsPinned());
                    assertFalse(tab1.getIsPinned());
                    assertFalse(tab2.getIsPinned());

                    mTabModelJni.pinTab(tab1);
                    assertTrue(tab1.getIsPinned());
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(1, mTabModelJni.indexOf(tab0));
                    assertEquals(2, mTabModelJni.indexOf(tab2));

                    mTabModelJni.pinTab(tab2);
                    assertTrue(tab2.getIsPinned());
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(1, mTabModelJni.indexOf(tab2));
                    assertEquals(2, mTabModelJni.indexOf(tab0));

                    mTabModelJni.unpinTab(tab1);
                    assertFalse(tab1.getIsPinned());
                    assertEquals(0, mTabModelJni.indexOf(tab2));
                    assertEquals(1, mTabModelJni.indexOf(tab1));
                    assertEquals(2, mTabModelJni.indexOf(tab0));

                    // Clean-up (otherwise next tests will fail)
                    mTabModelJni.unpinTab(tab2);
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
                    List<Tab> group0 = mTabModelJni.getAllTabs();
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
    public void testMoveGroupToIndex() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        List<Tab> g0 = createTabGroup(3, filter); // 1 2 3
        createTab(); // 4
        List<Tab> g1 = createTabGroup(2, filter); // 5 6
        // 0:Tab0 | G0(1:Tab1, 2:Tab2, 3:Tab3) | 4:Tab4 | G1(5:Tab5, 6:Tab6)

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(7, mTabModelJni.getCount());

                    // Requested index is inside tab group (left to right, insert at the end)
                    int requestedIndex = 3;
                    int firstValidIndex = 4;
                    assertMoveTabGroup(g1, requestedIndex, firstValidIndex);
                    // 0:Tab0 | G0(1:Tab1, 2:Tab2, 3:Tab3) | G1(4:Tab5, 5:Tab6) | 6:Tab4

                    // No-op (right to left)
                    requestedIndex = 2;
                    assertMoveTabGroup(g1, requestedIndex, firstValidIndex);

                    // No-op (right to left)
                    requestedIndex = 3;
                    assertMoveTabGroup(g1, requestedIndex, firstValidIndex);

                    // Simple move (left to right)
                    requestedIndex = 1;
                    firstValidIndex = 1;
                    assertMoveTabGroup(g1, requestedIndex, firstValidIndex);
                    // 0:Tab0 | G1(1:Tab5, 2:Tab6) | G0(3:Tab1, 4:Tab2, 5:Tab3) | 6:Tab4

                    // Requested index is inside group (left to right, insert at the end)
                    requestedIndex = 5;
                    firstValidIndex = 4;
                    assertMoveTabGroup(g1, requestedIndex, firstValidIndex);
                    // 0:Tab0 | G0(1:Tab1, 2:Tab2, 3:Tab3) | G1(4:Tab5, 5:Tab6) | 6:Tab4

                    // No-op (left to right)
                    requestedIndex = 4;
                    firstValidIndex = 1;
                    assertMoveTabGroup(g0, requestedIndex, firstValidIndex);

                    // Simple move (right to left)
                    requestedIndex = 0;
                    assertMoveTabGroup(g0, requestedIndex, requestedIndex);
                    // G0(0:Tab1, 1:Tab2, 2:Tab3) | 3:Tab0 | G1(4:Tab5, 5:Tab6) | 6:Tab4

                    // Requested index is inside group (right to left, insert in front)
                    requestedIndex = 4;
                    assertMoveTabGroup(g0, requestedIndex, firstValidIndex);
                    // 0:Tab0 | G0(1:Tab1, 2:Tab2, 3:Tab3) | G1(4:Tab5, 5:Tab6) | 6:Tab4
                });
    }

    @Test
    @SmallTest
    public void testMoveGroupToIndex_TabGroupOf1() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        List<Tab> g0 = createTabGroup(1, filter); // 1
        createTab(); // 2
        List<Tab> g1 = createTabGroup(3, filter); // 3 4 5
        // 0:Tab0 | G0(1:Tab1) | 2:Tab2 | G1(3:Tab3, 4:Tab4, 5:Tab5)

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(6, mTabModelJni.getCount());

                    int requestedIndex = 4;
                    int expectedFirstIndex = 2;
                    assertMoveTabGroup(g0, requestedIndex, expectedFirstIndex);
                    // 0:Tab0 | 1:Tab2 | G0(2:Tab1) | G1(3:Tab3, 4:Tab4, 5:Tab5)

                    // No-op
                    requestedIndex = 3;
                    assertMoveTabGroup(g0, requestedIndex, expectedFirstIndex);

                    // No-op
                    requestedIndex = 4;
                    assertMoveTabGroup(g0, requestedIndex, expectedFirstIndex);

                    requestedIndex = 5;
                    assertMoveTabGroup(g0, requestedIndex, requestedIndex);
                    // 0:Tab0 | 1:Tab2 | G1(2:Tab3, 3:Tab4, 4:Tab5) | G0(5:Tab1)

                    // No-op
                    requestedIndex = 3;
                    expectedFirstIndex = 5;
                    assertMoveTabGroup(g0, requestedIndex, expectedFirstIndex);

                    // No-op
                    requestedIndex = 4;
                    assertMoveTabGroup(g0, requestedIndex, expectedFirstIndex);

                    requestedIndex = 2;
                    assertMoveTabGroup(g0, requestedIndex, requestedIndex);
                    // 0:Tab0 | 1:Tab2 | G0(2:Tab1) | G1(3:Tab3, 4:Tab4, 5:Tab5)

                    requestedIndex = 0;
                    assertMoveTabGroup(g0, requestedIndex, requestedIndex);
                    // G0(0:Tab1) | 1:Tab0 | 2:Tab2 | G1(3:Tab3, 4:Tab4, 5:Tab5)

                    requestedIndex = 2;
                    assertMoveTabGroup(g0, requestedIndex, requestedIndex);
                    // 0:Tab0 | 1:Tab2 | G0(2:Tab1) | G1(3:Tab3, 4:Tab4, 5:Tab5)

                    assertMoveTabGroup(g1, requestedIndex, requestedIndex);
                    // 0:Tab0 | 1:Tab2 | G1(2:Tab3, 3:Tab4, 4:Tab5) | G0(5:Tab1)

                    requestedIndex = 5;
                    expectedFirstIndex = 3;
                    assertMoveTabGroup(g1, requestedIndex, expectedFirstIndex);
                    // 0:Tab0 | 1:Tab2 | G0(2:Tab1) | G1(3:Tab3, 4:Tab4, 5:Tab5)
                });
    }

    @Test
    @SmallTest
    public void testAddTabsToGroup() {
        createTabs(2);
        // 0:Tab0 | 1:Tab1 | 2:Tab2

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNull(tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());

                    List<Tab> tabsToGroup = new ArrayList<>();
                    tabsToGroup.add(tab1);
                    tabsToGroup.add(tab2);

                    Token groupId = mTabModelJni.addTabsToGroup(null, tabsToGroup);
                    assertNotNull(groupId);

                    assertEquals(groupId, tab1.getTabGroupId());
                    assertEquals(groupId, tab2.getTabGroupId());
                });
    }

    @Test
    @SmallTest
    public void testAddTabsToGroup_existingGroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(2, filter);
        Tab tab3 = createTab();
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab2 | 3:Tab3

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(4, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(tab1.getTabGroupId(), tab2.getTabGroupId());
                    assertNull(tab3.getTabGroupId());

                    Token groupId = tab1.getTabGroupId();
                    List<Tab> tabsToGroup = List.of(tab3);

                    Token returnedGroupId = mTabModelJni.addTabsToGroup(groupId, tabsToGroup);
                    assertEquals(groupId, returnedGroupId);

                    assertEquals(groupId, tab1.getTabGroupId());
                    assertEquals(groupId, tab2.getTabGroupId());
                    assertEquals(groupId, tab3.getTabGroupId());
                });
    }

    @Test
    @SmallTest
    public void testAddTabsToGroup_existingGroup_someTabsAlreadyInGroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(2, filter);
        Tab tab3 = createTab();
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab2 | 3:Tab3

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(4, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(tab1.getTabGroupId(), tab2.getTabGroupId());
                    assertNull(tab3.getTabGroupId());

                    Token groupId = tab1.getTabGroupId();
                    List<Tab> tabsToGroup = List.of(tab2, tab3);

                    Token returnedGroupId = mTabModelJni.addTabsToGroup(groupId, tabsToGroup);
                    assertEquals(groupId, returnedGroupId);

                    assertEquals(groupId, tab1.getTabGroupId());
                    assertEquals(groupId, tab2.getTabGroupId());
                    assertEquals(groupId, tab3.getTabGroupId());
                    assertEquals(3, filter.getTabCountForGroup(groupId));
                });
    }

    @Test
    @SmallTest
    public void testAddTabsToGroup_existingGroup_someTabsInAnotherGroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(2, filter); // Group A: Tab1, Tab2
        createTabGroup(2, filter); // Group B: Tab3, Tab4
        // 0:Tab0 | GroupA: 1:Tab1, 2:Tab2 | GroupB: 3:Tab3, 4:Tab4

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(5, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab3 = mTabModelJni.getTabAt(3);
                    Tab tab4 = mTabModelJni.getTabAt(4);

                    Token groupAId = tab1.getTabGroupId();
                    Token groupBId = tab3.getTabGroupId();
                    assertNotNull(groupAId);
                    assertNotNull(groupBId);
                    assertNotEquals(groupAId, groupBId);
                    assertEquals(groupBId, tab4.getTabGroupId());

                    List<Tab> tabsToGroup = List.of(tab3);
                    Token returnedGroupId = mTabModelJni.addTabsToGroup(groupAId, tabsToGroup);
                    assertEquals(groupAId, returnedGroupId);

                    assertEquals(groupAId, tab3.getTabGroupId());
                    assertEquals(groupBId, tab4.getTabGroupId());
                    assertEquals(3, filter.getTabCountForGroup(groupAId));
                    assertEquals(1, filter.getTabCountForGroup(groupBId));
                });
    }

    @Test
    @SmallTest
    public void testAddTabsToGroup_invalidGroupId() {
        createTabs(2);
        // 0:Tab0 | 1:Tab1 | 2:Tab2

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNull(tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());

                    List<Tab> tabsToGroup = List.of(tab1, tab2);
                    Token invalidGroupId = Token.createRandom();

                    Token returnedGroupId =
                            mTabModelJni.addTabsToGroup(invalidGroupId, tabsToGroup);
                    assertNull(returnedGroupId);

                    assertNull(tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());
                });
    }

    @Test
    @SmallTest
    public void testUngroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(2, filter);
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab2

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(tab1.getTabGroupId(), tab2.getTabGroupId());

                    List<Tab> tabsToUngroup = List.of(tab1, tab2);
                    mTabModelJni.ungroup(tabsToUngroup);

                    assertNull(tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());
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
                    List<Tab> tabs = mTabModelJni.getAllTabs();
                    assertEquals(3, tabs.size());
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
                    List<Tab> tabs = mTabModelJni.getAllTabs();
                    assertEquals(3, tabs.size());

                    int i = 0;
                    for (Tab tab : mTabModelJni) {
                        assertEquals(tabs.get(i), tab);
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

    @Test
    @SmallTest
    public void testCloseIncognitoTabSwitchesToNormalModelAndUpdatesIncognitoIndex() {
        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        TabModel normalTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        Tab regularTab = createTab();
        assertEquals(2, normalTabModel.getCount()); // Initial blank page + new tab
        assertEquals(0, incognitoTabModel.getCount());

        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, List.of(mTestUrl, mTestUrl));
        assertEquals(2, incognitoTabModel.getCount());

        // Switch to the incognito model and select the first incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
                    incognitoTabModel.setIndex(0, TabSelectionType.FROM_USER);
                });
        assertTrue(incognitoTabModel.isActiveModel());
        assertEquals(0, incognitoTabModel.index());

        Tab incognitoTab1 = incognitoTabModel.getTabAt(0);
        Tab incognitoTab2 = incognitoTabModel.getTabAt(1);
        assertNotNull(incognitoTab1);
        assertNotNull(incognitoTab2);

        // Close the first incognito tab (which is currently selected).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    incognitoTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(incognitoTab1)
                                            .recommendedNextTab(regularTab)
                                            .build(),
                                    /* allowDialog= */ false);
                });

        // Verify that the regular model is now active and the regular tab is selected.
        assertFalse(incognitoTabModel.isActiveModel());
        assertTrue(normalTabModel.isActiveModel());
        assertEquals(regularTab, normalTabModel.getCurrentTabSupplier().get());

        assertEquals(1, incognitoTabModel.getCount());
        assertEquals(incognitoTab2, incognitoTabModel.getTabAt(0));
        assertEquals(0, incognitoTabModel.index());
        assertEquals(incognitoTab2, incognitoTabModel.getCurrentTabSupplier().get());
    }

    @Test
    @SmallTest
    public void pinTab_NoExistingPinnedTabs_PinSingleTab() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tabToPin = tabModel.getTabAt(2);
                    tabModel.pinTab(tabToPin.getId());

                    assertTrue(tabToPin.getIsPinned());
                    assertEquals(tabToPin.getId(), tabModel.getTabAt(0).getId());

                    // Cleanup
                    tabModel.unpinTab(tabToPin.getId());
                });
    }

    @Test
    @SmallTest
    public void pinTab_PinMultipleTabs() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tab0 = tabModel.getTabAt(0);
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    tabModel.pinTab(tab1.getId());

                    assertEquals(tab1, tabModel.getTabAt(0));

                    tabModel.pinTab(tab2.getId());

                    assertEquals(tab1, tabModel.getTabAt(0));
                    assertEquals(tab2, tabModel.getTabAt(1));
                    assertEquals(tab0, tabModel.getTabAt(2));
                    assertEquals(4, tabModel.getCount());

                    // Cleanup
                    tabModel.unpinTab(tab1.getId());
                    tabModel.unpinTab(tab2.getId());
                });
    }

    @Test
    @SmallTest
    public void unpinTab_NoExistingUnpinnedTabs_UnpinSingleTab() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

                    // A, B, C, D.
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);
                    Tab tab3 = tabModel.getTabAt(3);

                    // Pin last 3 tabs.
                    // [D], [C], [B], A.
                    tabModel.pinTab(tab3.getId());
                    tabModel.pinTab(tab2.getId());
                    tabModel.pinTab(tab1.getId());

                    // Unpin the middle tab (Tab2).
                    // [D], [B], C, A.
                    tabModel.unpinTab(tab2.getId());
                    assertFalse(tab2.getIsPinned());
                    assertEquals(tab3, tabModel.getTabAt(0));
                    assertEquals(tab1, tabModel.getTabAt(1));
                    assertEquals(tab2, tabModel.getTabAt(2));

                    // Cleanup.
                    tabModel.unpinTab(tab3.getId());
                    tabModel.unpinTab(tab1.getId());
                });
    }

    @Test
    @SmallTest
    public void unpinTab_ExistingUnpinnedTabs_UnpinSingleTab() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

                    // A, B, C, D.
                    Tab tab0 = tabModel.getTabAt(0);
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);
                    Tab tab3 = tabModel.getTabAt(3);

                    // Pin 2 tabs.
                    // [B], [C], A, D.
                    tabModel.pinTab(tab1.getId());
                    tabModel.pinTab(tab2.getId());
                    assertTrue(tab1.getIsPinned());
                    assertTrue(tab2.getIsPinned());

                    // Unpin the first pinned tab.
                    tabModel.unpinTab(tab1.getId());
                    assertFalse(tab1.getIsPinned());

                    // [C], B, A, D.
                    assertEquals(tab2, tabModel.getTabAt(0));
                    assertEquals(tab1, tabModel.getTabAt(1));
                    assertEquals(tab0, tabModel.getTabAt(2));
                    assertEquals(tab3, tabModel.getTabAt(3));

                    // Cleanup.
                    tabModel.unpinTab(tab2.getId());
                });
    }

    @Test
    @SmallTest
    public void pinTab_thenUnpinTab_verifyObserverCalls() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    tabModel.addObserver(mTabModelObserver);

                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    tabModel.pinTab(tab2.getId());

                    verify(mTabModelObserver).didMoveTab(tab2, 0, 2);
                    verify(mTabModelObserver).willChangePinState(tab2);
                    verify(mTabModelObserver).didChangePinState(tab2);

                    tabModel.pinTab(tab1.getId());

                    verify(mTabModelObserver).didMoveTab(tab1, 1, 2);
                    verify(mTabModelObserver).willChangePinState(tab1);
                    verify(mTabModelObserver).didChangePinState(tab1);

                    tabModel.unpinTab(tab2.getId());

                    verify(mTabModelObserver).didMoveTab(tab2, 1, 0);
                    verify(mTabModelObserver, times(2)).willChangePinState(tab2);
                    verify(mTabModelObserver, times(2)).didChangePinState(tab2);

                    tabModel.unpinTab(tab1.getId());

                    verify(mTabModelObserver, times(2)).willChangePinState(tab1);
                    verify(mTabModelObserver, times(2)).didChangePinState(tab1);

                    // Cleanup.
                    tabModel.removeObserver(mTabModelObserver);
                });
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testHighlightTabs() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals("Should start with 3 tabs.", 3, tabModel.getCount());

                    Tab tab0 = tabModel.getTabAt(0);
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    // Initially, only Tab 1 should be in the multi-selection set.
                    assertEquals(
                            "There should be only 1 tab in the selection set",
                            1,
                            tabModel.getMultiSelectedTabsCount());
                    assertFalse(
                            "Tab 0 should not be selected initially.",
                            tabModel.isTabMultiSelected(tab0.getId()));
                    assertFalse(
                            "Tab 1 should not be selected initially.",
                            tabModel.isTabMultiSelected(tab1.getId()));
                    assertTrue(
                            "Tab 2 should be selected initially.",
                            tabModel.isTabMultiSelected(tab2.getId()));

                    // Highlight Tab 1 and Tab 2, and activate Tab 1.
                    List<Tab> tabsToHighlight = new ArrayList<>();
                    tabsToHighlight.add(tab1);
                    tabsToHighlight.add(tab2);
                    mTabModelJni.highlightTabs(tab1, tabsToHighlight);

                    // Verify that Tab 1 is the active tab.
                    assertEquals("The active tab should be at index 1.", 1, tabModel.index());

                    // Verify the multi-selection state.
                    assertFalse(
                            "Tab 0 should not be selected.",
                            tabModel.isTabMultiSelected(tab0.getId()));
                    assertTrue(
                            "Tab 1 should now be selected.",
                            tabModel.isTabMultiSelected(tab1.getId()));
                    assertTrue(
                            "Tab 2 should now be selected.",
                            tabModel.isTabMultiSelected(tab2.getId()));

                    // Verify the destructive nature by highlighting another tab.
                    List<Tab> moreTabs = new ArrayList<>();
                    moreTabs.add(tab0);
                    mTabModelJni.highlightTabs(tab0, moreTabs);

                    // Verify that Tab 0 is now the active tab.
                    assertEquals("The active tab should now be at index 0.", 0, tabModel.index());

                    // Verify that only tab 0 is in the multi-selection set.
                    assertTrue(
                            "Tab 0 should now be selected.",
                            tabModel.isTabMultiSelected(tab0.getId()));
                    assertFalse(
                            "Tab 1 should not be selected.",
                            tabModel.isTabMultiSelected(tab1.getId()));
                    assertFalse(
                            "Tab 2 should not be selected.",
                            tabModel.isTabMultiSelected(tab2.getId()));
                });
    }

    @Test
    @SmallTest
    @RequiresRestart // Avoid having multiple windows mess up the other tests
    public void testLaunchTypeForNewWindow() {
        createTabs(1);

        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        int numTabsBeforeTest = tabModel.getCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Use WindowOpenDisposition.NEW_WINDOW. The other values don't matter.
                    mTabModelJni.openNewTab(
                            tabModel.getTabAt(0),
                            new GURL("https://www.chromium.org"),
                            /* initiatorOrigin= */ null,
                            /* extraHeaders= */ "",
                            /* postData= */ ResourceRequestBody.createFromBytes(new byte[] {}),
                            WindowOpenDisposition.NEW_WINDOW,
                            /* persistParentage= */ false,
                            /* isRendererInitiated= */ false);
                });
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            CriteriaHelper.pollUiThread(
                    () -> MultiWindowUtils.getInstanceCount() == 2,
                    "Expected new window to be created");
        } else {
            assertEquals(
                    "Expected a new tab to be created", numTabsBeforeTest + 1, tabModel.getCount());
        }
    }

    private void assertMoveTabToIndex(
            int oldIndex, int newIndex, int expectedIndex, boolean movingInsideGroup) {
        Tab oldIndexTab = mTabModelJni.getTabAt(oldIndex);
        assert movingInsideGroup || oldIndexTab.getTabGroupId() == null
                : "This is not a single tab movement";
        mTabModelJni.moveTabToIndex(oldIndexTab, newIndex);
        assertEquals(oldIndexTab, mTabModelJni.getTabAt(expectedIndex));
    }

    private void assertMoveTabGroup(List<Tab> tabs, int requestedIndex, int firstValidIndex) {
        Token tabGroupId = tabs.get(0).getTabGroupId();
        mTabModelJni.moveGroupToIndex(tabGroupId, requestedIndex);

        int size = tabs.size();
        for (int i = 0; i < size; i++) {
            Tab movedTab = mTabModelJni.getTabAt(firstValidIndex + i);
            assertEquals(
                    "Tab at index " + (firstValidIndex + i) + " has wrong group ID.",
                    tabGroupId,
                    movedTab.getTabGroupId());
            assertEquals(
                    "Tab at index " + (firstValidIndex + i) + " is not the correct tab.",
                    tabs.get(i),
                    movedTab);
        }
    }

    private List<Tab> createTabGroup(int numberOfTabs, TabGroupModelFilter filter) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < numberOfTabs; i++) tabs.add(createTab());
        ThreadUtils.runOnUiThreadBlocking(
                () -> filter.mergeListOfTabsToGroup(tabs, tabs.get(0), false));
        return tabs;
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
