// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link TabGroupUiOneshotSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupUiOneshotSupplierUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Activity mActivity;
    @Mock private ViewGroup mViewGroup;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ScrimCoordinator mScrimCoordinator;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private ModalDialogManager mModalDialogManager;

    @Mock private Tab mTab;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabManagementDelegate mTabManagementDelegate;
    @Mock private TabGroupUi mTabGroupUi;
    @Mock private ThemeColorProvider mThemeColorProvider;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Tab>> mActivityTabObserverCaptor;

    private ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>();
    private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private TabGroupUiOneshotSupplier mTabGroupUiOneshotSupplier;

    @Before
    public void setUp() {
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTab.isIncognito()).thenReturn(false);
        mTabGroupUiOneshotSupplier =
                new TabGroupUiOneshotSupplier(
                        mActivityTabProvider,
                        mTabModelSelector,
                        mActivity,
                        mViewGroup,
                        mBrowserControlsStateProvider,
                        mScrimCoordinator,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManager,
                        mThemeColorProvider);
        when(mTabManagementDelegate.createTabGroupUi(
                        any(), any(), any(), any(), any(), any(), any(), any(), any(), any(), any(),
                        any(), any()))
                .thenReturn(mTabGroupUi);
        TabManagementDelegateProvider.setTabManagementDelegateForTesting(mTabManagementDelegate);

        verify(mActivityTabProvider).addObserver(mActivityTabObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        mTabGroupUiOneshotSupplier.destroy();
        verify(mTab).removeObserver(any());
        verify(mActivityTabProvider).removeObserver(any());
    }

    @Test
    public void testNotInGroupWhenFocusedThenInGroup() {
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(false);

        mActivityTabObserverCaptor.getValue().onResult(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verifyNoInteractions(mTabManagementDelegate);
        assertNull(mTabGroupUiOneshotSupplier.get());

        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        mTabObserverCaptor.getValue().onTabGroupIdChanged(mTab, null);

        ShadowLooper.runUiThreadTasks();

        verify(mTabManagementDelegate)
                .createTabGroupUi(
                        mActivity,
                        mViewGroup,
                        mBrowserControlsStateProvider,
                        mScrimCoordinator,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabModelSelector,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManager,
                        mThemeColorProvider);
        assertNotNull(mTabGroupUiOneshotSupplier.get());
    }

    @Test
    public void testInGroupWhenFocused() {
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);

        mActivityTabObserverCaptor.getValue().onResult(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verifyNoInteractions(mTabManagementDelegate);
        assertNull(mTabGroupUiOneshotSupplier.get());

        ShadowLooper.runUiThreadTasks();

        verify(mTabManagementDelegate)
                .createTabGroupUi(
                        mActivity,
                        mViewGroup,
                        mBrowserControlsStateProvider,
                        mScrimCoordinator,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabModelSelector,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManager,
                        mThemeColorProvider);
        assertNotNull(mTabGroupUiOneshotSupplier.get());
    }
}
