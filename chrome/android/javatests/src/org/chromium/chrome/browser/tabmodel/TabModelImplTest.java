// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.util.ChromeTabUtils.getIndexOnUiThread;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroidJni;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the legacy {@link TabModelImpl} that also apply to {@link TabCollectionTabModelImpl}.
 */
// TODO(crbug.com/454298057): Migrate these tests to TabCollectionTabModelImplTest.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabModelImplTest {
    private static final String TAG = "TabModelImplTest";
    private static final boolean ENABLE_DEBUG_LOGGING = false;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureDevicesDispatcherAndroidJni;

    @Mock private TabModelObserver mTabModelObserver;

    @Mock private TabModelActionListener mTabModelActionListener;

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
        assertEquals(1, getTabCountOnUiThread(normalTabModel));
        assertNotEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(normalTabModel));

        TabModel incognitoTabModel = mPage.getTabModelSelector().getModel(true);
        assertEquals(0, getTabCountOnUiThread(incognitoTabModel));
        assertEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(incognitoTabModel));
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
        assertEquals(2, getTabCountOnUiThread(normalTabModel));
        assertNotEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(normalTabModel));

        // No incognito tabs are restored from a cold start.
        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        assertEquals(0, getTabCountOnUiThread(incognitoTabModel));
        assertEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(incognitoTabModel));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1448777")
    public void validIndexAfterRestored_FromPreviousActivity() {
        mActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel = newActivity.getTabModelSelector().getModel(false);
        assertEquals(1, getTabCountOnUiThread(normalTabModel));
        assertNotEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(normalTabModel));

        TabModel incognitoTabModel = newActivity.getTabModelSelector().getModel(true);
        assertEquals(0, getTabCountOnUiThread(incognitoTabModel));
        assertEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(incognitoTabModel));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/457847264): Change to @Restriction(DeviceFormFactor.PHONE) after launch
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void validIndexAfterRestored_FromPreviousActivity_WithIncognitoTabs() {
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, List.of(mTestUrl));

        mActivityTestRule.recreateActivity();
        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel = newActivity.getTabModelSelector().getModel(false);
        assertEquals(1, getTabCountOnUiThread(normalTabModel));
        assertNotEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(normalTabModel));

        TabModel incognitoTabModel = newActivity.getTabModelSelector().getModel(true);
        assertEquals(1, getTabCountOnUiThread(incognitoTabModel));
        assertNotEquals(TabModel.INVALID_TAB_INDEX, getIndexOnUiThread(incognitoTabModel));
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
                    Tab tab = mTabModelJni.openTabProgrammatically(url, 0);
                    assertNotNull(tab);
                    assertEquals(url, tab.getUrl());
                    assertEquals(2, mTabModelJni.getCount());

                    Tab foundTab = mTabModelJni.getTabAt(0);
                    assertNotNull(foundTab);
                    assertEquals(tab, foundTab);
                    assertEquals(url, foundTab.getUrl());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            tab.getTabLaunchTypeAtCreation());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testDuplicateTab() {
        // 0:Tab0 | 1:Tab1 (tabToDuplicate) | 2:Tab2
        GURL url = new GURL(mTestUrl);
        RegularNewTabPageStation page = mPage.openNewTabFast();
        page.loadWebPageProgrammatically(mTestUrl).openNewTabFast();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    int tabToDuplicateIndex = 1;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(tabToDuplicateIndex);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    assertNull(tabToDuplicate.getTabGroupId());

                    Tab duplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // 0:Tab0 | 1:Tab1 (tabToDuplicate) | 2:Tab3 (duplicated) | 3:Tab2
                    assertEquals(4, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertNull(duplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                    assertEquals(url, duplicatedTab.getUrl());

                    // Verify order.
                    assertEquals(tabToDuplicate, mTabModelJni.getTabAt(tabToDuplicateIndex));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 1));
                    assertEquals(tab2, mTabModelJni.getTabAt(tabToDuplicateIndex + 2));

                    // Duplicate tab again.
                    Tab newestDuplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // 0:Tab0 | 1:Tab1 (tabToDuplicate), 2:Tab4 (newest), 3:Tab3 (oldest), 4:Tab2
                    assertEquals(5, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), newestDuplicatedTab.getParentId());
                    assertNull(newestDuplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                    assertEquals(url, newestDuplicatedTab.getUrl());

                    // Verify order again.
                    assertEquals(tabToDuplicate, mTabModelJni.getTabAt(tabToDuplicateIndex));
                    assertEquals(
                            newestDuplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 1));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 2));
                    assertEquals(tab2, mTabModelJni.getTabAt(tabToDuplicateIndex + 3));
                });
    }

    @Test
    @SmallTest
    public void testDuplicateTab_InsideTabGroup() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        // 0:Tab0 | Group0: 1:Tab1 (tabToDuplicate), 2:Tab2, 3:Tab3
        createTabGroup(3, filter);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(4, mTabModelJni.getCount());
                    int tabToDuplicateIndex = 1;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(tabToDuplicateIndex);
                    Tab tab2 = mTabModelJni.getTabAt(2);
                    Tab tab3 = mTabModelJni.getTabAt(3);
                    assertNotNull(tabToDuplicate.getTabGroupId());

                    Tab duplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // 0:Tab0 | Group0: 1:Tab1 (tabToDuplicate), 2:Tab4 (duplicated), 3:Tab2, 4:Tab3
                    assertEquals(5, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(tabToDuplicate.getTabGroupId(), duplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());

                    // Verify order.
                    assertEquals(tabToDuplicate, mTabModelJni.getTabAt(tabToDuplicateIndex));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 1));
                    assertEquals(tab2, mTabModelJni.getTabAt(tabToDuplicateIndex + 2));
                    assertEquals(tab3, mTabModelJni.getTabAt(tabToDuplicateIndex + 3));

                    // Duplicate tab again.
                    Tab newestDuplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // 0:Tab0 | Group0: 1:Tab1 (tabToDuplicate), 2:Tab5 (newest), 3:Tab4 (oldest),
                    // 4:Tab2, 5:Tab3
                    assertEquals(6, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), newestDuplicatedTab.getParentId());
                    assertEquals(
                            tabToDuplicate.getTabGroupId(), newestDuplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            newestDuplicatedTab.getTabLaunchTypeAtCreation());

                    // Verify order again.
                    assertEquals(tabToDuplicate, mTabModelJni.getTabAt(tabToDuplicateIndex));
                    assertEquals(
                            newestDuplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 1));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 2));
                    assertEquals(tab2, mTabModelJni.getTabAt(tabToDuplicateIndex + 3));
                    assertEquals(tab3, mTabModelJni.getTabAt(tabToDuplicateIndex + 4));
                });

        Tab tab5 = createTab();
        // 0:Tab0 | Group0: 1:Tab1, 2:Tab5, 3:Tab4, 4:Tab2, 5:Tab3 | 6:Tab5

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(7, mTabModelJni.getCount());
                    int tabToDuplicateIndex = 5;
                    Tab tabToDuplicate = mTabModelJni.getTabAt(tabToDuplicateIndex);
                    assertNotNull(tabToDuplicate.getTabGroupId());

                    Tab duplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    //  0:Tab0 | Group0: 1:Tab1, 2:Tab5, 3:Tab4, 4:Tab2, 5:Tab3 (tabToDuplicate),
                    // 6:Tab7 (duplicatedTab) | 7:Tab6
                    assertEquals(8, mTabModelJni.getCount());
                    assertEquals(tab5, mTabModelJni.getTabAt(7));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(tabToDuplicateIndex + 1));
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(tabToDuplicate.getTabGroupId(), duplicatedTab.getTabGroupId());
                    assertNotNull(duplicatedTab.getTabGroupId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testDuplicateTab_PinnedTab() {
        mPage.openNewTabFast();
        // 0:Tab0 | 1:Tab1 (tabToDuplicate)

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(2, mTabModelJni.getCount());
                    Tab tab0 = mTabModelJni.getTabAt(/* index= */ 0);
                    Tab tabToDuplicate = mTabModelJni.getTabAt(/* index= */ 1);

                    mTabModelJni.pinTab(tabToDuplicate);
                    mTabModelJni.pinTab(tab0);
                    // [0:Tab1 (tabToDuplicate)] | [1:Tab0]
                    assertEquals(0, mTabModelJni.indexOf(tabToDuplicate));
                    assertEquals(1, mTabModelJni.indexOf(tab0));
                    assertTrue(tabToDuplicate.getIsPinned());
                    assertTrue(tab0.getIsPinned());

                    Tab duplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // [0:Tab1 (tabToDuplicate)] | [1:Tab2 (duplicatedTab) | [2:Tab0]
                    assertEquals(3, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), duplicatedTab.getParentId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            duplicatedTab.getTabLaunchTypeAtCreation());

                    // Verify order.
                    assertEquals(0, mTabModelJni.indexOf(tabToDuplicate));
                    assertEquals(1, mTabModelJni.indexOf(duplicatedTab));
                    assertEquals(2, mTabModelJni.indexOf(tab0));

                    // Duplicate tab again.
                    Tab newestDuplicatedTab = mTabModelJni.duplicateTab(tabToDuplicate);
                    // [0:Tab1 (tabToDuplicate)] | [1:Tab3 (newest)] | [2:Tab2 (oldest) | [3:Tab0]
                    assertEquals(4, mTabModelJni.getCount());
                    assertEquals(tabToDuplicate.getId(), newestDuplicatedTab.getParentId());
                    assertEquals(
                            TabLaunchType.FROM_TAB_LIST_INTERFACE,
                            newestDuplicatedTab.getTabLaunchTypeAtCreation());

                    // Verify order again
                    assertEquals(tabToDuplicate, mTabModelJni.getTabAt(0));
                    assertEquals(newestDuplicatedTab, mTabModelJni.getTabAt(1));
                    assertEquals(duplicatedTab, mTabModelJni.getTabAt(2));
                    assertEquals(tab0, mTabModelJni.getTabAt(3));

                    // Clean-up (otherwise next tests will fail).
                    mTabModelJni.unpinTab(tabToDuplicate);
                    mTabModelJni.unpinTab(newestDuplicatedTab);
                    mTabModelJni.unpinTab(duplicatedTab);
                    mTabModelJni.unpinTab(tab0);
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
                    filter.mergeListOfTabsToGroup(
                            group0,
                            group0.get(0),
                            TabGroupModelFilter.MergeNotificationType.DONT_NOTIFY);
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
    @DisabledTest(message = "crbug.com/447152102")
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testAddTab_CurrentTabPinned() {
        createTabs(4);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(5, mTabModelJni.getCount());

                    // Detach a tab which will be added again later.
                    Tab tabToRemove = mTabModelJni.getTabAt(4);
                    mTabModelJni.getTabRemover().removeTab(tabToRemove, /* allowDialog= */ false);

                    // Pin first two tabs.
                    Tab tab0 = mTabModelJni.getTabAt(0);
                    Tab tab1 = mTabModelJni.getTabAt(1);

                    mTabModelJni.pinTab(tab0);
                    mTabModelJni.pinTab(tab1);

                    // Select the first tab which is pinned.
                    mTabModelJni.setIndex(0, TabSelectionType.FROM_USER);

                    // Index is passed 1, since hitting "Open in new tab" will trigger it to next
                    // index by default.
                    mTabModelJni.addTab(
                            tabToRemove,
                            1,
                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                            TabCreationState.LIVE_IN_BACKGROUND);

                    // It should move to start of non pinned tabs.
                    assertEquals(mTabModelJni.getTabAt(2), tabToRemove);
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
    @DisableFeatures(ChromeFeatureList.TAB_FREEZING_USES_DISCARD)
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
    // TODO(crbug.com/457847264): Change to @Restriction(DeviceFormFactor.PHONE) after launch
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testCloseIncognitoTabSwitchesToNormalModelAndUpdatesIncognitoIndex() {
        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        TabModel normalTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        Tab regularTab = createTab();
        assertEquals(2, getTabCountOnUiThread(normalTabModel)); // Initial blank page + new tab
        assertEquals(0, getTabCountOnUiThread(incognitoTabModel));

        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, List.of(mTestUrl, mTestUrl));
        assertEquals(2, getTabCountOnUiThread(incognitoTabModel));

        // Switch to the incognito model and select the first incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
                    incognitoTabModel.setIndex(0, TabSelectionType.FROM_USER);
                });
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> incognitoTabModel.isActiveModel()));
        assertEquals(0, getIndexOnUiThread(incognitoTabModel));

        Tab incognitoTab1 = ThreadUtils.runOnUiThreadBlocking(() -> incognitoTabModel.getTabAt(0));
        Tab incognitoTab2 = ThreadUtils.runOnUiThreadBlocking(() -> incognitoTabModel.getTabAt(1));
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(incognitoTabModel.isActiveModel());
                    assertTrue(normalTabModel.isActiveModel());
                    assertEquals(regularTab, normalTabModel.getCurrentTabSupplier().get());

                    assertEquals(1, incognitoTabModel.getCount());
                    assertEquals(incognitoTab2, incognitoTabModel.getTabAt(0));
                    assertEquals(0, incognitoTabModel.index());
                    assertEquals(incognitoTab2, incognitoTabModel.getCurrentTabSupplier().get());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void pinTab_NoExistingPinnedTabs_PinSingleTab() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tabToPin = tabModel.getTabAt(2);
                    tabModel.pinTab(tabToPin.getId(), /* showUngroupDialog= */ false);

                    assertTrue(tabToPin.getIsPinned());
                    assertEquals(tabToPin.getId(), tabModel.getTabAt(0).getId());

                    // Cleanup
                    tabModel.unpinTab(tabToPin.getId());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void pinTab_PinMultipleTabs() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tab0 = tabModel.getTabAt(0);
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);

                    assertEquals(tab1, tabModel.getTabAt(0));

                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);

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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
                    tabModel.pinTab(tab3.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);

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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);
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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void pinTab_thenUnpinTab_verifyObserverCalls() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    tabModel.addObserver(mTabModelObserver);

                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);

                    verify(mTabModelObserver).didMoveTab(tab2, 0, 2);
                    verify(mTabModelObserver).willChangePinState(tab2);
                    verify(mTabModelObserver).didChangePinState(tab2);

                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);

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
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
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
    public void testSetMuteSetting() {
        WebPageStation page = mPage.loadWebPageProgrammatically(mTestUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(1, mTabModelJni.getCount());
                    List<Tab> tabsToMute = mTabModelJni.getAllTabs();
                    Tab tab = tabsToMute.get(0);
                    assertFalse(
                            "Tab should not be muted initially.",
                            tab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ true);
                    assertTrue(
                            "Tab should be muted after setting.",
                            tab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ false);
                    assertFalse(
                            "Tab should be unmuted after setting.",
                            tab.getWebContents().isAudioMuted());
                });

        page.loadWebPageProgrammatically("chrome://version");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Tab> tabsToMute = mTabModelJni.getAllTabs();
                    Tab tab = tabsToMute.get(0);
                    assertFalse(
                            "WebContents should not be muted initially",
                            tab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ true);
                    assertTrue("WebContents should be muted", tab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ false);
                    assertFalse(
                            "WebContents shouldn't be muted", tab.getWebContents().isAudioMuted());
                });
    }

    @Test
    @SmallTest
    public void testSetMuteSetting_MultipleTabs() {
        // First tab is Chrome Scheme to test mute persistence.
        WebPageStation page = mPage.loadWebPageProgrammatically("chrome://version");
        page.openNewTabFast().loadWebPageProgrammatically(mTestUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(2, mTabModelJni.getCount());
                    List<Tab> tabsToMute = mTabModelJni.getAllTabs();

                    Tab tab1 = tabsToMute.get(0);
                    Tab tab2 = tabsToMute.get(1);

                    assertFalse(
                            "Tab 1 should not be muted initially.",
                            tab1.getWebContents().isAudioMuted());
                    assertFalse(
                            "Tab 2 should not be muted initially.",
                            tab2.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ true);

                    // Tab 1 should remain muted because of TabMutedReason.
                    // SoundContentSettingObserver originally would reset the mute setting for
                    // Chrome Schemes if there was no TabMutedReason.
                    assertTrue("Tab 1 should be muted.", tab1.getWebContents().isAudioMuted());
                    assertTrue("Tab 2 should be muted.", tab2.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ false);

                    assertFalse("Tab 1 should be unmuted.", tab1.getWebContents().isAudioMuted());
                    assertFalse("Tab 2 should be unmuted.", tab2.getWebContents().isAudioMuted());
                });
    }

    @Test
    @SmallTest
    public void testSetMuteSetting_SameSite() {
        WebPageStation page = mPage.loadWebPageProgrammatically(mTestUrl);
        String secondUrl =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/simple.html");
        page.openNewTabFast().loadWebPageProgrammatically(secondUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab1 = mTabModelJni.getTabAt(0);
                    Tab tab2 = mTabModelJni.getTabAt(1);

                    assertFalse(
                            "Tab 1 should not be muted initially.",
                            tab1.getWebContents().isAudioMuted());
                    assertFalse(
                            "Tab 2 should not be muted initially.",
                            tab2.getWebContents().isAudioMuted());

                    List<Tab> tabsToMute = List.of(tab1);
                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ true);

                    assertTrue(
                            "Tab 1 should be muted after setting.",
                            tab1.getWebContents().isAudioMuted());
                    assertTrue(
                            "Tab 2 should also be muted as it's the same site.",
                            tab2.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(tabsToMute, /* mute= */ false);

                    assertFalse(
                            "Tab 1 should be unmuted after setting.",
                            tab1.getWebContents().isAudioMuted());
                    assertFalse(
                            "Tab 2 should also be unmuted as it's the same site.",
                            tab2.getWebContents().isAudioMuted());
                });
    }

    @Test
    @SmallTest
    public void testSetMuteSetting_WithWildcardPattern() {
        final String host = "example.com";
        final String subdomainUrl =
                mActivityTestRule.getTestServer().getURLWithHostName("sub." + host, "/test.html");
        final String anotherSubdomainUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURLWithHostName("another." + host, "/test.html");
        final String wildcardPattern = "[*.]" + host;
        final Tab tab = mPage.getTab();

        // Set a wildcard rule to BLOCK sound for all subdomains.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebsitePreferenceBridge.setContentSettingCustomScope(
                                mTabModelJni.getProfile(),
                                ContentSettingsType.SOUND,
                                wildcardPattern,
                                WebsitePreferenceBridge.SITE_WILDCARD,
                                ContentSetting.BLOCK));

        mPage.loadWebPageProgrammatically(subdomainUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Tab should be muted by wildcard rule",
                            tab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(List.of(tab), /* mute= */ false);
                    assertFalse(
                            "Tab should be unmuted by the new, specific exception.",
                            tab.getWebContents().isAudioMuted());

                    @ContentSetting
                    int setting =
                            WebsitePreferenceBridge.getContentSetting(
                                    mTabModelJni.getProfile(),
                                    ContentSettingsType.SOUND,
                                    new GURL(subdomainUrl),
                                    new GURL(subdomainUrl));

                    assertEquals(
                            "Site should block sound by the new, specific exception.",
                            ContentSetting.ALLOW,
                            setting);

                    setting =
                            WebsitePreferenceBridge.getContentSetting(
                                    mTabModelJni.getProfile(),
                                    ContentSettingsType.SOUND,
                                    new GURL(anotherSubdomainUrl),
                                    new GURL(anotherSubdomainUrl));
                    assertEquals(
                            "Another subdomain should still have sound blocked by the wildcard.",
                            ContentSetting.BLOCK,
                            setting);

                    // Cleanup
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            mTabModelJni.getProfile(),
                            ContentSettingsType.SOUND,
                            wildcardPattern,
                            WebsitePreferenceBridge.SITE_WILDCARD,
                            ContentSetting.DEFAULT);

                    setting =
                            WebsitePreferenceBridge.getContentSetting(
                                    mTabModelJni.getProfile(),
                                    ContentSettingsType.SOUND,
                                    new GURL(anotherSubdomainUrl),
                                    new GURL(anotherSubdomainUrl));
                    assertEquals(
                            "Another subdomain should allow sound again after wildcard rule is"
                                    + " removed.",
                            ContentSetting.ALLOW,
                            setting);
                });
    }

    @Test
    @SmallTest
    // TODO(crbug.com/457847264): Change to @Restriction(DeviceFormFactor.PHONE) after launch
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testSetMuteSetting_Incognito() {
        WebPageStation page = mPage.loadWebPageProgrammatically(mTestUrl);
        Journeys.createIncognitoTabsWithWebPages(page, List.of(mTestUrl));
        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab regularTab = mTabModelJni.getTabAt(0);
                    Tab incognitoTab = incognitoTabModel.getTabAt(0);

                    assertFalse(
                            "Regular tab should not be muted initially.",
                            regularTab.getWebContents().isAudioMuted());
                    assertFalse(
                            "Incognito tab should not be muted initially.",
                            incognitoTab.getWebContents().isAudioMuted());

                    List<Tab> incognitoToMute = List.of(incognitoTab);
                    List<Tab> regularToMute = List.of(regularTab);

                    // Muting in non-incognito will influence incognito if there is no specific
                    // content setting for the site in the incognito profile.
                    mTabModelJni.setMuteSetting(regularToMute, /* mute= */ true);
                    assertTrue(
                            "Regular tab should be muted.",
                            regularTab.getWebContents().isAudioMuted());
                    assertTrue(
                            "Incognito tab should be muted because it inherits from regular model.",
                            incognitoTab.getWebContents().isAudioMuted());

                    // For unmuting, same reason as comment above.
                    mTabModelJni.setMuteSetting(regularToMute, /* mute= */ false);
                    assertFalse(
                            "Regular tab should be unmuted.",
                            regularTab.getWebContents().isAudioMuted());
                    assertFalse(
                            "Incognito tab should also be unmuted.",
                            incognitoTab.getWebContents().isAudioMuted());

                    // Muting in incognito should not influence the non-incognito profile content
                    // setting of this site.
                    incognitoTabModel.setMuteSetting(incognitoToMute, /* mute= */ true);
                    assertTrue(
                            "Incognito tab should be muted.",
                            incognitoTab.getWebContents().isAudioMuted());
                    assertFalse(
                            "Regular tab should not be affected by incognito setting.",
                            regularTab.getWebContents().isAudioMuted());

                    mTabModelJni.setMuteSetting(regularToMute, /* mute= */ true);
                    assertTrue(
                            "Regular tab should be muted again.",
                            regularTab.getWebContents().isAudioMuted());

                    // At this point incognito has its specific content setting and will no longer
                    // be in sync with the non-incognito content setting map.
                    mTabModelJni.setMuteSetting(regularToMute, /* mute= */ false);
                    assertTrue(
                            "Incognito tab should remain muted due to its own setting.",
                            incognitoTab.getWebContents().isAudioMuted());
                    assertFalse(
                            "Regular tab should be unmuted.",
                            regularTab.getWebContents().isAudioMuted());

                    // Reset
                    incognitoTabModel.setMuteSetting(incognitoToMute, /* mute= */ false);
                    assertFalse(
                            "Incognito tab should be unmuted after reset.",
                            incognitoTab.getWebContents().isAudioMuted());
                });
    }

    @Test
    @SmallTest
    @RequiresRestart // Avoid having multiple windows mess up the other tests
    public void testLaunchTypeForNewWindow() {
        createTabs(1);

        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        int numTabsBeforeTest = getTabCountOnUiThread(tabModel);

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
                    () ->
                            MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY)
                                    == 2,
                    "Expected new window to be created");
        } else {
            assertEquals(
                    "Expected a new tab to be created",
                    numTabsBeforeTest + 1,
                    getTabCountOnUiThread(tabModel));
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testPinTab_TryPinningExistingPinnedTab() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab0 = mTabModelJni.getTabAt(0);
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);

                    // Pin tab1. Order should be tab1, tab0, tab2
                    mTabModelJni.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                    assertTrue(tab1.getIsPinned());
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(1, mTabModelJni.indexOf(tab0));
                    assertEquals(2, mTabModelJni.indexOf(tab2));

                    // Pin tab1 again. Order should not change.
                    mTabModelJni.pinTab(tab1.getId(), /* showUngroupDialog= */ false);

                    // Pin tab2. It should move to right place
                    mTabModelJni.pinTab(tab2.getId(), /* showUngroupDialog= */ false);
                    assertTrue(tab2.getIsPinned());
                    assertEquals(0, mTabModelJni.indexOf(tab1));
                    assertEquals(1, mTabModelJni.indexOf(tab2));
                    assertEquals(2, mTabModelJni.indexOf(tab0));

                    // Clean-up.
                    mTabModelJni.unpinTab(tab1.getId());
                    mTabModelJni.unpinTab(tab2.getId());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testUnpinTab_AlreadyUnpinned() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mTabModelJni.getCount());
                    Tab tab0 = mTabModelJni.getTabAt(0);
                    Tab tab1 = mTabModelJni.getTabAt(1);
                    Tab tab2 = mTabModelJni.getTabAt(2);

                    // tab1 is not pinned.
                    assertFalse(tab1.getIsPinned());

                    // Unpin tab1. Order should not change.
                    mTabModelJni.unpinTab(tab1.getId());
                    assertEquals(0, mTabModelJni.indexOf(tab0));
                    assertEquals(1, mTabModelJni.indexOf(tab1));
                    assertEquals(2, mTabModelJni.indexOf(tab2));
                });
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.ANDROID_PINNED_TABS,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP
    })
    public void removePinState_WhenFeatureDisabled() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals(3, tabModel.getCount());

                    Tab tab1 = tabModel.getTabAt(/* index= */ 1);
                    tab1.setIsPinned(true);
                    tabModel.getTabRemover().removeTab(tab1, /* allowDialog= */ false);
                    assertEquals(2, tabModel.getCount());

                    tabModel.addTab(
                            tab1,
                            -1,
                            TabLaunchType.FROM_RESTORE,
                            TabCreationState.FROZEN_ON_RESTORE);
                    assertFalse(tab1.getIsPinned());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testGetPinnedTabsCount() {
        createTabs(3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);

                    assertEquals(0, tabModel.getPinnedTabsCount());

                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                    assertEquals(1, tabModel.getPinnedTabsCount());

                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);
                    assertEquals(2, tabModel.getPinnedTabsCount());

                    tabModel.unpinTab(tab1.getId());
                    assertEquals(1, tabModel.getPinnedTabsCount());

                    // Cleanup.
                    tabModel.unpinTab(tab2.getId());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void restoreMultiplePinnedTabs_OrderIsPreserved() {
        createTabs(4); // Creates 5 tabs in total (including the initial one)

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    assertEquals(5, tabModel.getCount());

                    Tab tab1 = tabModel.getTabAt(1);
                    Tab tab2 = tabModel.getTabAt(2);
                    Tab tab3 = tabModel.getTabAt(3);

                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab3.getId(), /* showUngroupDialog= */ false);

                    assertEquals(0, tabModel.indexOf(tab1));
                    assertEquals(1, tabModel.indexOf(tab2));
                    assertEquals(2, tabModel.indexOf(tab3));

                    // Remove the pinned tabs
                    tabModel.getTabRemover().removeTab(tab1, false);
                    tabModel.getTabRemover().removeTab(tab2, false);
                    tabModel.getTabRemover().removeTab(tab3, false);

                    assertEquals(2, tabModel.getCount());

                    // Restore the tabs in the same order
                    tabModel.addTab(
                            tab1,
                            0,
                            TabLaunchType.FROM_RESTORE,
                            TabCreationState.FROZEN_ON_RESTORE);
                    tabModel.addTab(
                            tab2,
                            1,
                            TabLaunchType.FROM_RESTORE,
                            TabCreationState.FROZEN_ON_RESTORE);
                    tabModel.addTab(
                            tab3,
                            2,
                            TabLaunchType.FROM_RESTORE,
                            TabCreationState.FROZEN_ON_RESTORE);

                    assertEquals(5, tabModel.getCount());
                    assertEquals(
                            "Tab 1 should be restored to its original pinned index.",
                            0,
                            tabModel.indexOf(tab1));
                    assertEquals(
                            "Tab 2 should be restored to its original pinned index.",
                            1,
                            tabModel.indexOf(tab2));
                    assertEquals(
                            "Tab 3 should be restored to its original pinned index.",
                            2,
                            tabModel.indexOf(tab3));

                    // Cleanup
                    tabModel.unpinTab(tab1.getId());
                    tabModel.unpinTab(tab2.getId());
                    tabModel.unpinTab(tab3.getId());
                });
    }

    private void assertMoveTabToIndex(
            int oldIndex, int newIndex, int expectedIndex, boolean movingInsideGroup) {
        Tab oldIndexTab = mTabModelJni.getTabAt(oldIndex);
        assertWithMessage("This is not a single tab movement")
                .that(movingInsideGroup || oldIndexTab.getTabGroupId() == null)
                .isTrue();
        if (ENABLE_DEBUG_LOGGING) {
            logTabModelStructure(mTabModelJni, "Before move");
            Log.i(
                    TAG,
                    "Moving "
                            + oldIndexTab.getId()
                            + " from "
                            + oldIndex
                            + " to "
                            + newIndex
                            + " actual valid index "
                            + expectedIndex);
        }
        mTabModelJni.moveTabToIndex(oldIndexTab, newIndex);
        if (ENABLE_DEBUG_LOGGING) {
            logTabModelStructure(mTabModelJni, "After move");
        }
        assertEquals(oldIndexTab, mTabModelJni.getTabAt(expectedIndex));
    }

    private void assertMoveTabGroup(List<Tab> tabs, int requestedIndex, int firstValidIndex) {
        Token tabGroupId = tabs.get(0).getTabGroupId();
        if (ENABLE_DEBUG_LOGGING) {
            logTabModelStructure(mTabModelJni, "Before move");
            Log.i(
                    TAG,
                    "Moving "
                            + tabGroupId
                            + " to "
                            + requestedIndex
                            + " actual valid index "
                            + firstValidIndex);
        }
        mTabModelJni.moveGroupToIndex(tabGroupId, requestedIndex);
        if (ENABLE_DEBUG_LOGGING) {
            logTabModelStructure(mTabModelJni, "After move");
        }

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
                () ->
                        filter.mergeListOfTabsToGroup(
                                tabs,
                                tabs.get(0),
                                TabGroupModelFilter.MergeNotificationType.DONT_NOTIFY));
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

    private void logTabModelStructure(TabModel tabModel, String prefix) {
        StringBuilder sb = new StringBuilder();
        sb.append(prefix).append("\n");
        sb.append("TabModel structure:\n");
        int tabCount = tabModel.getCount();
        sb.append("Tab count: " + tabCount + "\n");
        int i = 0;
        for (; i < tabCount; i++) {
            sb.append("Pinned: [\n");
            Tab tab = tabModel.getTabAt(i);
            if (!tab.getIsPinned()) break;
            printTab(sb, tab, i);
        }
        sb.append("],\n");
        sb.append("Unpinned: [\n");
        for (; i < tabCount; i++) {
            Tab tab = tabModel.getTabAt(i);
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                sb.append("Group: " + tabGroupId + " [\n");
                while (i < tabCount) {
                    Tab groupTab = tabModel.getTabAt(i);
                    if (!tabGroupId.equals(groupTab.getTabGroupId())) {
                        i--;
                        break;
                    }
                    printTab(sb, groupTab, i);
                    i++;
                }
                sb.append("],\n");
            } else {
                printTab(sb, tab, i);
            }
        }
        sb.append("]\n");
        Log.i(TAG, sb.toString());
    }

    private void printTab(StringBuilder sb, Tab tab, int index) {
        sb.append(index).append(": ").append(tab.getId()).append(",\n");
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_PINNED_TABS,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP
    })
    public void testPinUnpinTab_RecordsHistogram() {
        createTabs(2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tabUnderInvestigation = tabModel.getTabAt(0);

                    HistogramWatcher histogram =
                            HistogramWatcher.newBuilder()
                                    .expectAnyRecord("Tab.PinnedDuration")
                                    .build();

                    // Pin and unpin the tab.
                    tabModel.pinTab(tabUnderInvestigation.getId(), /* showUngroupDialog= */ false);
                    tabModel.unpinTab(tabUnderInvestigation.getId());

                    // Verify that the histogram was recorded.
                    histogram.assertExpected();
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testPinTabInGroup_ActionListener_Accept() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(1, filter); // Group with 1 tab.
        // 0:Tab0 | Group0: 1:Tab1

        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModelJni.getTabAt(1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNotNull(tab1.getTabGroupId());

                    mTabModelJni.pinTab(
                            tab1.getId(), /* showUngroupDialog= */ true, mTabModelActionListener);
                });
        onViewWaiting(withText(R.string.delete_tab_group_action), /* checkRootDialog= */ true)
                .perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verify(mTabModelActionListener)
                            .onConfirmationDialogResult(
                                    eq(TabModelActionListener.DialogType.SYNC),
                                    eq(ActionConfirmationResult.CONFIRMATION_POSITIVE));
                    assertTrue(tab1.getIsPinned());
                    assertNull(tab1.getTabGroupId());

                    // [1:Tab1] | 0:Tab0
                    assertEquals(0, mTabModelJni.indexOf(tab1));

                    // Cleanup
                    mTabModelJni.unpinTab(tab1.getId());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testPinTabInGroup_ActionListener_Reject() {
        TabGroupModelFilter filter = mPage.getTabGroupModelFilter();
        createTabGroup(1, filter); // Group with 1 tab.
        // 0:Tab0 | Group0: 1:Tab1

        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModelJni.getTabAt(1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNotNull(tab1.getTabGroupId());

                    mTabModelJni.pinTab(
                            tab1.getId(), /* showUngroupDialog= */ true, mTabModelActionListener);
                });
        onViewWaiting(withText(R.string.cancel), /* checkRootDialog= */ true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    verify(mTabModelActionListener)
                            .onConfirmationDialogResult(
                                    eq(TabModelActionListener.DialogType.SYNC),
                                    eq(ActionConfirmationResult.CONFIRMATION_NEGATIVE));
                    assertEquals(1, mTabModelJni.indexOf(tab1));
                    assertFalse(tab1.getIsPinned());
                    assertNotNull(tab1.getTabGroupId());
                });
    }
}
