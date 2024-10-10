// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

/** Unit tests for {@link HubTabSwitcherMetricsRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubTabSwitcherMetricsRecorderUnitTest {
    private static final int REGULAR_TAB_0_ID = 123;
    private static final int REGULAR_TAB_1_ID = 678;
    private static final int REGULAR_TAB_0_INDEX = 0;
    private static final int REGULAR_TAB_1_INDEX = 1;

    private static final int INCOGNITO_TAB_0_ID = 748932;
    private static final int INCOGNITO_TAB_1_ID = 237398;
    private static final int INCOGNITO_TAB_0_INDEX = 0;
    private static final int INCOGNITO_TAB_1_INDEX = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mRegularProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;

    private MockTabModel mRegularTabModel;
    private MockTabModel mIncognitoTabModel;

    private UserActionTester mActionTester;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private ObservableSupplierImpl<TabModel> mCurrentTabModelSupplier;
    private ObservableSupplierImpl<Boolean> mHubVisibilitySupplier;

    private HubTabSwitcherMetricsRecorder mMetricsRecorder;

    @Before
    public void setUp() {
        when(mRegularProfile.isOffTheRecord()).thenReturn(false);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        mRegularTabModel = spy(new MockTabModel(mRegularProfile, null));
        mRegularTabModel.addTab(REGULAR_TAB_0_ID);
        mRegularTabModel.addTab(REGULAR_TAB_1_ID);
        mRegularTabModel.setIndex(REGULAR_TAB_0_INDEX, FROM_USER);
        mIncognitoTabModel = spy(new MockTabModel(mIncognitoProfile, null));
        mIncognitoTabModel.addTab(INCOGNITO_TAB_0_ID);
        mIncognitoTabModel.addTab(INCOGNITO_TAB_1_ID);
        mIncognitoTabModel.setIndex(INCOGNITO_TAB_0_INDEX, FROM_USER);

        Tab regularTab0 = mRegularTabModel.getTabAt(REGULAR_TAB_0_INDEX);
        Tab regularTab1 = mRegularTabModel.getTabAt(REGULAR_TAB_1_INDEX);
        Tab incognitoTab0 = mIncognitoTabModel.getTabAt(INCOGNITO_TAB_0_INDEX);
        Tab incognitoTab1 = mIncognitoTabModel.getTabAt(INCOGNITO_TAB_1_INDEX);

        when(mRegularTabGroupModelFilter.getTabModel()).thenReturn(mRegularTabModel);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(regularTab0)).thenReturn(false);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(regularTab1)).thenReturn(false);
        when(mRegularTabGroupModelFilter.indexOf(regularTab0)).thenReturn(REGULAR_TAB_0_INDEX);
        when(mRegularTabGroupModelFilter.indexOf(regularTab1)).thenReturn(REGULAR_TAB_1_INDEX);
        when(mIncognitoTabGroupModelFilter.getTabModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabGroupModelFilter.indexOf(incognitoTab0))
                .thenReturn(INCOGNITO_TAB_0_INDEX);
        when(mIncognitoTabGroupModelFilter.indexOf(incognitoTab1))
                .thenReturn(INCOGNITO_TAB_1_INDEX);
        when(mIncognitoTabGroupModelFilter.isTabInTabGroup(incognitoTab0)).thenReturn(false);
        when(mIncognitoTabGroupModelFilter.isTabInTabGroup(incognitoTab1)).thenReturn(false);

        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilter())
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);

        mCurrentTabModelSupplier = new ObservableSupplierImpl<>();
        mCurrentTabModelSupplier.set(mRegularTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier.set(mTabSwitcherPane);

        mHubVisibilitySupplier = new ObservableSupplierImpl<>();
        mHubVisibilitySupplier.set(false);

        mActionTester = new UserActionTester();

        mMetricsRecorder =
                new HubTabSwitcherMetricsRecorder(
                        mTabModelSelector, mHubVisibilitySupplier, mFocusedPaneSupplier);
        ShadowLooper.runUiThreadTasks();
    }

    @After
    public void tearDown() {
        mMetricsRecorder.destroy();

        assertFalse(mHubVisibilitySupplier.hasObservers());
        assertFalse(mFocusedPaneSupplier.hasObservers());
        assertFalse(mCurrentTabModelSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testToggleHubVisibility() {
        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mCurrentTabModelSupplier.hasObservers());
        verify(mRegularTabModel).addObserver(any());
        verify(mIncognitoTabModel).addObserver(any());

        mHubVisibilitySupplier.set(false);
        assertFalse(mCurrentTabModelSupplier.hasObservers());
        // Removed when initially not visible.
        verify(mRegularTabModel, times(2)).removeObserver(any());
        verify(mIncognitoTabModel, times(2)).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testSamePane_NoTabChange() {
        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabOffsetOfSwitch.GridTabSwitcher", 0);
        mRegularTabModel.setIndex(REGULAR_TAB_0_INDEX, FROM_USER);

        assertEquals(1, mActionTester.getActionCount("MobileTabReturnedToCurrentTab.TabGrid"));
        assertEquals(1, mActionTester.getActionCount("MobileTabReturnedToCurrentTab"));
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSamePane_ChangedTabs_WithGroup() {
        Tab regularTab1 = mRegularTabModel.getTabAt(REGULAR_TAB_1_INDEX);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(regularTab1)).thenReturn(true);
        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabOffsetOfSwitch.GridTabSwitcher", -1);
        mRegularTabModel.setIndex(REGULAR_TAB_1_INDEX, FROM_USER);

        assertEquals(0, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSamePane_ChangedTabs_WithoutGroup() {
        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabOffsetOfSwitch.GridTabSwitcher", -1);
        mRegularTabModel.setIndex(REGULAR_TAB_1_INDEX, FROM_USER);

        assertEquals(1, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testNewPane_NoSwitch() {
        mHubVisibilitySupplier.set(true);
        changePanes();

        mIncognitoTabModel.setIndex(INCOGNITO_TAB_0_INDEX, FROM_USER);

        assertEquals(1, mActionTester.getActionCount("MobileTabSwitched"));
        assertEquals(1, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
    }

    @Test
    @SmallTest
    public void testNewPane_ChangedTabs_WithGroup() {
        Tab incognitoTab1 = mIncognitoTabModel.getTabAt(INCOGNITO_TAB_1_INDEX);
        when(mIncognitoTabGroupModelFilter.isTabInTabGroup(incognitoTab1)).thenReturn(true);

        mHubVisibilitySupplier.set(true);
        changePanes();

        mIncognitoTabModel.setIndex(INCOGNITO_TAB_1_INDEX, FROM_USER);

        assertEquals(0, mActionTester.getActionCount("MobileTabSwitched"));
        assertEquals(0, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
    }

    @Test
    @SmallTest
    public void testNewPane_ChangedTabs_WithoutGroup() {
        mHubVisibilitySupplier.set(true);
        changePanes();

        mIncognitoTabModel.setIndex(INCOGNITO_TAB_1_INDEX, FROM_USER);

        assertEquals(0, mActionTester.getActionCount("MobileTabSwitched"));
        assertEquals(1, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
    }

    private void changePanes() {
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilter())
                .thenReturn(mIncognitoTabGroupModelFilter);
        mCurrentTabModelSupplier.set(mIncognitoTabModel);
        ShadowLooper.runUiThreadTasks();
    }
}
