// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.MockitoHelper;

/** Tests for {@link TabModelUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelUtilsUnitTest {
    private static final int TAB_ID = 5;
    private static final int ARCHIVED_TAB_ID = 6;
    private static final int INCOGNITO_TAB_ID = 7;
    private static final int UNUSED_TAB_ID = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Tab mTab;
    @Mock private Tab mArchivedTab;
    @Mock private Tab mIncognitoTab;
    @Mock private Callback<TabModelSelector> mTabModelSelectorCallback;
    @Mock private WindowAndroid mWindowAndroid;

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private MockTabModelSelector mTabModelSelector;
    private MockTabModelSelector mArchivedTabModelSelector;
    private MockTabModel mTabModel;
    private MockTabModel mArchivedTabModel;
    private MockTabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        MockitoHelper.forwardBind(mTabModelSelectorCallback);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mArchivedTab.getId()).thenReturn(ARCHIVED_TAB_ID);
        when(mArchivedTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mArchivedTab.getProfile()).thenReturn(mProfile);
        when(mIncognitoTab.getId()).thenReturn(INCOGNITO_TAB_ID);
        when(mIncognitoTab.isIncognito()).thenReturn(true);
        mTabModelSelector = spy(new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null));
        mArchivedTabModelSelector =
                spy(new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null));
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mTabModel = (MockTabModel) mTabModelSelector.getModel(false);
        mTabModel.addTab(
                mTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
        mArchivedTabModel = (MockTabModel) mArchivedTabModelSelector.getModel(false);
        mArchivedTabModel.addTab(
                mArchivedTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
        mIncognitoTabModel = (MockTabModel) mTabModelSelector.getModel(true);
        mIncognitoTabModel.addTab(
                mIncognitoTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
    }

    @Test
    @SmallTest
    public void testSelectTabById() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(mTabModelSelector, TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TAB_ID, mTabModel.getTabAt(mTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdIncognito() {
        assertEquals(TabList.INVALID_TAB_INDEX, mIncognitoTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, INCOGNITO_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(
                INCOGNITO_TAB_ID, mIncognitoTabModel.getTabAt(mIncognitoTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpInvalidTabId() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, Tab.INVALID_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpNotFound() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(mTabModelSelector, UNUSED_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedCallback() {
        mTabModelSelector.markTabStateInitialized();
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelectorCallback).onResult(mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedObserver() {
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabModelSelectorCallback, never()).onResult(mTabModelSelector);
        mTabModelSelector.markTabStateInitialized();
        verify(mTabModelSelectorCallback).onResult(mTabModelSelector);
        verify(mTabModelSelector).removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedRemoveObserverWhenDestroyed() {
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabModelSelector, never())
                .removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
        mTabModelSelector.destroy();
        verify(mTabModelSelector).removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
    }

    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();

    @Test
    @SmallTest
    public void testOnInitializedTabModelSelector_AlreadyInit() {
        mTabModelSelector.markTabStateInitialized();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .onAvailable(mTabModelSelectorCallback);
        shadowOf(getMainLooper()).idle();
        verify(mTabModelSelectorCallback).onResult(any());
    }

    @Test
    @SmallTest
    public void testOnInitializedTabModelSelector_LateSet() {
        mTabModelSelector.markTabStateInitialized();
        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .onAvailable(mTabModelSelectorCallback);
        mTabModelSelectorSupplier.set(mTabModelSelector);
        shadowOf(getMainLooper()).idle();
        verify(mTabModelSelectorCallback).onResult(any());
    }

    @Test
    @SmallTest
    public void testOnInitializedTabModelSelector_LateInit() {
        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .onAvailable(mTabModelSelectorCallback);
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mTabModelSelector.markTabStateInitialized();
        shadowOf(getMainLooper()).idle();
        verify(mTabModelSelectorCallback).onResult(any());
    }

    @Test
    @SmallTest
    public void testOnInitializedTabModelSelector_BothLate() {
        mTabModelSelectorSupplier.set(mTabModelSelector);
        TabModelUtils.onInitializedTabModelSelector(mTabModelSelectorSupplier)
                .onAvailable(mTabModelSelectorCallback);
        mTabModelSelector.markTabStateInitialized();
        shadowOf(getMainLooper()).idle();
        verify(mTabModelSelectorCallback).onResult(any());
    }

    @Test
    @SmallTest
    public void testGetTabModelFilterByTab() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelFilter filter = TabModelUtils.getTabModelFilterByTab(mTab);
        assertEquals(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), filter);
    }

    @Test
    @SmallTest
    public void testGetTabModelFilterByTab_Archived() {
        ArchivedTabModelSelectorHolder.setInstanceFn((profile) -> mArchivedTabModelSelector);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelFilter filter = TabModelUtils.getTabModelFilterByTab(mArchivedTab);
        assertEquals(
                mArchivedTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                filter);
    }
}
