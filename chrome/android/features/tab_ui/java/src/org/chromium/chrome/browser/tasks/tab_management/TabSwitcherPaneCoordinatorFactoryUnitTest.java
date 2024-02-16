// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
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

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabSwitcherPaneCoordinatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneCoordinatorFactoryUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final String TAB1_TITLE = "Hello world";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>();

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mTabModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private ScrimCoordinator mScrimCoordinator;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabSwitcherResetHandler mResetHandler;
    @Mock private Callback<Integer> mOnTabClickedCallback;

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private Tab mTab1;

    private Activity mActivity;
    private FrameLayout mParentView;
    private TabSwitcherPaneCoordinatorFactory mFactory;

    @Before
    public void setUp() {
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityReady);
    }

    private void onActivityReady(Activity activity) {
        mActivity = activity;
        mParentView = new FrameLayout(activity);
        mFactory =
                new TabSwitcherPaneCoordinatorFactory(
                        activity,
                        mLifecycleDispatcher,
                        mProfileProviderSupplier,
                        mTabModelSelector,
                        mTabContentManager,
                        mTabCreatorManager,
                        mBrowserControlsStateProvider,
                        mMultiWindowModeStateDispatcher,
                        mScrimCoordinator,
                        mSnackbarManager,
                        mModalDialogManager);
    }

    @Test
    @SmallTest
    public void testCreate() {
        assertNotNull(
                mFactory.create(
                        mParentView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        /* isIncognito= */ false));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testTabListMode_HighEnd() {
        assertEquals(TabListMode.GRID, mFactory.getTabListMode());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testTabListMode_LowEnd() {
        assertEquals(TabListMode.LIST, mFactory.getTabListMode());
    }

    @Test
    @SmallTest
    public void testGetTitle_Tab() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);

        PseudoTab tab1 = PseudoTab.fromTab(mTab1);
        assertEquals(TAB1_TITLE, mFactory.getTitle(mActivity, tab1));
    }

    @Test
    @SmallTest
    public void testGetTitle_TabGroup() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        int tabCount = 2;
        when(mTabModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(tabCount);

        PseudoTab tab1 = PseudoTab.fromTab(mTab1);
        assertEquals(
                TabGroupTitleEditor.getDefaultTitle(mActivity, tabCount),
                mFactory.getTitle(mActivity, tab1));
    }

    @Test
    @SmallTest
    public void testCreateScrimCoordinatorForTablet() {
        assertNotNull(TabSwitcherPaneCoordinatorFactory.createScrimCoordinatorForTablet(mActivity));
    }

    @Test
    @SmallTest
    public void testCreateTabModelFilterSupplier_AlreadyCreated() {
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel));

        var supplier = mFactory.createTabModelFilterSupplier(false);
        verify(mTabModelSelector, never()).addObserver(any());
        assertEquals(mTabModelFilter, supplier.get());
    }

    @Test
    @SmallTest
    public void testCreateTabModelFilterSupplier_WaitForChange() {
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());

        var supplier = mFactory.createTabModelFilterSupplier(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertNull(supplier.get());

        TabModelSelectorObserver observer = mTabModelSelectorObserverCaptor.getValue();

        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel));
        observer.onChange();
        verify(mTabModelSelector).removeObserver(observer);
        assertEquals(mTabModelFilter, supplier.get());
    }
}
