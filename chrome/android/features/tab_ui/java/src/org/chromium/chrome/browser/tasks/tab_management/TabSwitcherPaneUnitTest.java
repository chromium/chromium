// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationType;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Unit tests for {@link TabSwitcherPane} and {@link TabSwitcherPaneBase}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneUnitTest {
    private static final int TAB_ID = 723849;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SharedPreferences mSharedPreferences;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;
    @Mock private TabSwitcherDrawable mTabSwitcherDrawable;
    @Mock private HubContainerView mHubContainerView;
    @Mock private View.OnClickListener mNewTabButtonClickListener;
    @Mock private TabModelFilter mTabModelFilter;
    @Mock private PaneHubController mPaneHubController;
    @Mock private TabSwitcherCustomViewManager.Delegate mCustomViewManagerDelegate;
    @Mock private View mCustomView;

    @Captor ArgumentCaptor<OnSharedPreferenceChangeListener> mPriceAnnotationsPrefListenerCaptor;
    @Captor ArgumentCaptor<Callback<Integer>> mOnTabClickedCallbackCaptor;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private Context mContext;
    private ObservableSupplierImpl<Boolean> mHandleBackPressChangeSupplier =
            new ObservableSupplierImpl<>();
    private TabSwitcherPane mTabSwitcherPane;
    private MockTabModel mTabModel;
    private int mTimesCreated;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mHubContainerView.getContext()).thenReturn(mContext);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileProviderSupplier.set(mProfileProvider);

        mTabModel = new MockTabModel(mProfile, null);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);

        Supplier<Boolean> gridDialogVisibilitySupplier = () -> false;
        when(mTabSwitcherPaneCoordinator.getTabSwitcherCustomViewManagerDelegate())
                .thenReturn(mCustomViewManagerDelegate);
        when(mTabSwitcherPaneCoordinator.getTabGridDialogVisibilitySupplier())
                .thenReturn(gridDialogVisibilitySupplier);
        doAnswer(
                        invocation -> {
                            mTimesCreated++;
                            return mTabSwitcherPaneCoordinator;
                        })
                .when(mTabSwitcherPaneCoordinatorFactory)
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        mOnTabClickedCallbackCaptor.capture(),
                        anyBoolean());
        when(mTabSwitcherPaneCoordinatorFactory.getTabListMode()).thenReturn(TabListMode.GRID);
        when(mTabSwitcherPaneCoordinator.getHandleBackPressChangedSupplier())
                .thenReturn(mHandleBackPressChangeSupplier);
        mHandleBackPressChangeSupplier.set(false);
        when(mTabSwitcherPaneDrawableCoordinator.getTabSwitcherDrawable())
                .thenReturn(mTabSwitcherDrawable);
        doAnswer(
                        invocation -> {
                            return mHandleBackPressChangeSupplier.get()
                                    ? BackPressResult.SUCCESS
                                    : BackPressResult.FAILURE;
                        })
                .when(mTabSwitcherPaneCoordinator)
                .handleBackPress();

        mTabSwitcherPane =
                new TabSwitcherPane(
                        mContext,
                        mSharedPreferences,
                        mProfileProviderSupplier,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mTabModelFilter,
                        mNewTabButtonClickListener,
                        mTabSwitcherPaneDrawableCoordinator);
        ShadowLooper.runUiThreadTasks();
        verify(mSharedPreferences)
                .registerOnSharedPreferenceChangeListener(
                        mPriceAnnotationsPrefListenerCaptor.capture());
    }

    @After
    public void tearDown() {
        mTabSwitcherPane.destroy();
        verify(mTabSwitcherPaneCoordinator, times(mTimesCreated)).destroy();
        verify(mSharedPreferences)
                .unregisterOnSharedPreferenceChangeListener(
                        mPriceAnnotationsPrefListenerCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testInitWithNativeBeforeCoordinatorCreation() {
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    @SmallTest
    public void testInitWithNativeAfterCoordinatorCreation() {
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator, never()).initWithNative();

        mTabSwitcherPane.initWithNative();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    @SmallTest
    public void testPaneId() {
        assertEquals(PaneId.TAB_SWITCHER, mTabSwitcherPane.getPaneId());
    }

    @Test
    @SmallTest
    public void testLoadHintColdWarmCold() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator).softCleanup();
        verify(coordinator, never()).hardCleanup();

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());
        verify(coordinator, times(2)).softCleanup();
        verify(coordinator).hardCleanup();
    }

    @Test
    @SmallTest
    public void testLoadHintColdHotWarm() {
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();
        verify(coordinator).resetWithTabList(mTabModelFilter);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator).softCleanup();
        verify(coordinator, never()).hardCleanup();
    }

    @Test
    @SmallTest
    public void testLoadHintColdWarmHotCold() {
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();
        verify(coordinator).resetWithTabList(mTabModelFilter);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());
        verify(coordinator).softCleanup();
        verify(coordinator).hardCleanup();
    }

    @Test
    @SmallTest
    public void testGetRootView() {
        assertNotNull(mTabSwitcherPane.getRootView());
    }

    @Test
    @SmallTest
    public void testNewTabButton() {
        FullButtonData buttonData = mTabSwitcherPane.getActionButtonDataSupplier().get();

        assertEquals(mContext.getString(R.string.button_new_tab), buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.button_new_tab),
                buttonData.resolveContentDescription(mContext));
        assertTrue(
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon)
                        .getConstantState()
                        .equals(buttonData.resolveIcon(mContext).getConstantState()));

        buttonData.getOnPressRunnable().run();
        verify(mNewTabButtonClickListener).onClick(isNull());
    }

    @Test
    @SmallTest
    public void testReferenceButton() {
        DisplayButtonData buttonData = mTabSwitcherPane.getReferenceButtonDataSupplier().get();

        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher_standard_stack),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher_standard_stack),
                buttonData.resolveContentDescription(mContext));
        assertEquals(mTabSwitcherDrawable, buttonData.resolveIcon(mContext));
    }

    @Test
    @SmallTest
    public void testBackPress() {
        ObservableSupplier<Boolean> handlesBackPressSupplier =
                mTabSwitcherPane.getHandleBackPressChangedSupplier();
        assertNull(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.FAILURE, mTabSwitcherPane.handleBackPress());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertFalse(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.FAILURE, mTabSwitcherPane.handleBackPress());

        mHandleBackPressChangeSupplier.set(true);
        assertTrue(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.SUCCESS, mTabSwitcherPane.handleBackPress());
    }

    @Test
    @SmallTest
    public void testCreateFadeOutAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testCreateFadeInAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mTabSwitcherPane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testCreateFadeOutAnimatorListMode() {
        createSelectedTab();
        when(mTabSwitcherPaneCoordinatorFactory.getTabListMode()).thenReturn(TabListMode.LIST);
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testCreateFadeInAnimatorListMode() {
        createSelectedTab();
        when(mTabSwitcherPaneCoordinatorFactory.getTabListMode()).thenReturn(TabListMode.LIST);
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mTabSwitcherPane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testCreateExpandTabAnimator() {
        createSelectedTab();
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertEquals(
                HubLayoutAnimationType.EXPAND_TAB,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testCreateShrinkTabAnimator() {
        createSelectedTab();
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertEquals(
                HubLayoutAnimationType.EXPAND_TAB,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testPriceTracking() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        assertNotNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mTabSwitcherPane.destroyTabSwitcherPaneCoordinator();
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);

        OnSharedPreferenceChangeListener listener = mPriceAnnotationsPrefListenerCaptor.getValue();
        // Check this doesn't crash if there is no coordinator.
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();

        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabModelFilter);

        when(mTabModelFilter.isTabModelRestored()).thenReturn(false);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabModelFilter);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(false);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabModelFilter);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);

        listener.onSharedPreferenceChanged(mSharedPreferences, "foo");
        verify(coordinator).resetWithTabList(mTabModelFilter);

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabModelFilter);
    }

    @Test
    @SmallTest
    public void testShowTabListEditor() {
        MenuOrKeyboardActionHandler handler = mTabSwitcherPane.getMenuOrKeyboardActionHandler();
        assertNotNull(handler);
        assertFalse(
                handler.handleMenuOrKeyboardAction(
                        org.chromium.chrome.tab_ui.R.id.menu_select_tabs, false));

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.initWithNative();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();

        assertFalse(
                handler.handleMenuOrKeyboardAction(
                        org.chromium.chrome.tab_ui.R.id.new_tab_menu_id, false));
        verify(coordinator, never()).showTabListEditor();

        assertTrue(
                handler.handleMenuOrKeyboardAction(
                        org.chromium.chrome.tab_ui.R.id.menu_select_tabs, false));
        verify(coordinator).showTabListEditor();
    }

    @Test
    @SmallTest
    public void testGetDialogVisibilitySupplier() {
        assertNull(mTabSwitcherPane.getTabGridDialogVisibilitySupplier());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        assertNotNull(mTabSwitcherPane.getTabGridDialogVisibilitySupplier());
        verify(mTabSwitcherPaneCoordinator).getTabGridDialogVisibilitySupplier();
    }

    @Test
    @SmallTest
    public void testGetCustomViewManager() {
        assertNotNull(mTabSwitcherPane.getTabSwitcherCustomViewManager());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator).getTabSwitcherCustomViewManagerDelegate();

        TabSwitcherCustomViewManager customViewManager =
                mTabSwitcherPane.getTabSwitcherCustomViewManager();
        Runnable r = () -> {};
        assertTrue(customViewManager.requestView(mCustomView, r, true));
        verify(mCustomViewManagerDelegate).addCustomView(mCustomView, r, true);

        mTabSwitcherPane.destroyTabSwitcherPaneCoordinator();
        verify(mCustomViewManagerDelegate).removeCustomView(mCustomView);
    }

    @Test
    @SmallTest
    public void testGetTabListModelSize() {
        assertEquals(0, mTabSwitcherPane.getTabSwitcherTabListModelSize());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        int tabCount = 5;
        when(mTabSwitcherPaneCoordinator.getTabSwitcherTabListModelSize()).thenReturn(tabCount);

        assertEquals(tabCount, mTabSwitcherPane.getTabSwitcherTabListModelSize());
    }

    @Test
    @SmallTest
    public void testSetRecyclerViewPosition() {
        RecyclerViewPosition position = new RecyclerViewPosition(1, 5);
        mTabSwitcherPane.setTabSwitcherRecyclerViewPosition(position);
        verify(mTabSwitcherPaneCoordinator, never()).setTabSwitcherRecyclerViewPosition(any());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        mTabSwitcherPane.setTabSwitcherRecyclerViewPosition(position);
        verify(mTabSwitcherPaneCoordinator).setTabSwitcherRecyclerViewPosition(position);
    }

    @Test
    @SmallTest
    public void testResetWithTabList() {
        assertFalse(mTabSwitcherPane.resetWithTabList(null, false));

        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();

        assertTrue(mTabSwitcherPane.resetWithTabList(null, false));
        verify(coordinator).resetWithTabList(null);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mTabSwitcherPane.showAllTabs();
        verify(coordinator, times(2)).resetWithTabList(null);
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(false);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(3)).resetWithTabList(null);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(mTabModelFilter);
    }

    @Test
    @SmallTest
    public void testOnTabClickedCallback() {
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        int tabId = 6;
        mOnTabClickedCallbackCaptor.getValue().onResult(tabId);
        verify(mPaneHubController, never()).selectTabAndHideHub(tabId);

        mTabSwitcherPane.setPaneHubController(mPaneHubController);

        mOnTabClickedCallbackCaptor.getValue().onResult(tabId);
        verify(mPaneHubController).selectTabAndHideHub(tabId);
    }

    private void createSelectedTab() {
        mTabModel.addTab(TAB_ID);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER, false);
    }
}
