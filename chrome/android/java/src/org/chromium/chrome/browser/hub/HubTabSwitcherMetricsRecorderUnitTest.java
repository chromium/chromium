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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
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

    private MockTabModel mRegularTabModel;
    private MockTabModel mIncognitoTabModel;

    private UserActionTester mActionTester;
    private SettableMonotonicObservableSupplier<Pane> mFocusedPaneSupplier;
    private SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private SettableNonNullObservableSupplier<Boolean> mHubVisibilitySupplier;

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

        when(mRegularTabModel.isTabInTabGroup(regularTab0)).thenReturn(false);
        when(mRegularTabModel.isTabInTabGroup(regularTab1)).thenReturn(false);
        when(mRegularTabModel.representativeIndexOf(regularTab0)).thenReturn(REGULAR_TAB_0_INDEX);
        when(mRegularTabModel.representativeIndexOf(regularTab1)).thenReturn(REGULAR_TAB_1_INDEX);
        when(mIncognitoTabModel.representativeIndexOf(incognitoTab0))
                .thenReturn(INCOGNITO_TAB_0_INDEX);
        when(mIncognitoTabModel.representativeIndexOf(incognitoTab1))
                .thenReturn(INCOGNITO_TAB_1_INDEX);
        when(mIncognitoTabModel.isTabInTabGroup(incognitoTab0)).thenReturn(false);
        when(mIncognitoTabModel.isTabInTabGroup(incognitoTab1)).thenReturn(false);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        mCurrentTabModelSupplier = ObservableSuppliers.createMonotonic();
        mCurrentTabModelSupplier.set(mRegularTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier = ObservableSuppliers.createMonotonic();
        mFocusedPaneSupplier.set(mTabSwitcherPane);

        mHubVisibilitySupplier = ObservableSuppliers.createNonNull(false);

        mActionTester = new UserActionTester();

        mMetricsRecorder =
                new HubTabSwitcherMetricsRecorder(
                        mTabModelSelector, mHubVisibilitySupplier, mFocusedPaneSupplier);
        RobolectricUtil.runAllBackgroundAndUi();
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
        RobolectricUtil.runAllBackgroundAndUi();
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
        RobolectricUtil.runAllBackgroundAndUi();

        mRegularTabModel.setIndex(REGULAR_TAB_0_INDEX, FROM_USER);

        assertEquals(1, mActionTester.getActionCount("MobileTabReturnedToCurrentTab.TabGrid"));
        assertEquals(1, mActionTester.getActionCount("MobileTabReturnedToCurrentTab"));
    }

    @Test
    @SmallTest
    public void testSamePane_ChangedTabs_WithGroup() {
        Tab regularTab1 = mRegularTabModel.getTabAt(REGULAR_TAB_1_INDEX);
        when(mRegularTabModel.isTabInTabGroup(regularTab1)).thenReturn(true);
        mHubVisibilitySupplier.set(true);
        RobolectricUtil.runAllBackgroundAndUi();

        mRegularTabModel.setIndex(REGULAR_TAB_1_INDEX, FROM_USER);

        assertEquals(0, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
    }

    @Test
    @SmallTest
    public void testSamePane_ChangedTabs_WithoutGroup() {
        mHubVisibilitySupplier.set(true);
        RobolectricUtil.runAllBackgroundAndUi();

        mRegularTabModel.setIndex(REGULAR_TAB_1_INDEX, FROM_USER);

        assertEquals(1, mActionTester.getActionCount("MobileTabSwitched.GridTabSwitcher"));
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
        when(mIncognitoTabModel.isTabInTabGroup(incognitoTab1)).thenReturn(true);

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
        mCurrentTabModelSupplier.set(mIncognitoTabModel);
        RobolectricUtil.runAllBackgroundAndUi();
    }
}
