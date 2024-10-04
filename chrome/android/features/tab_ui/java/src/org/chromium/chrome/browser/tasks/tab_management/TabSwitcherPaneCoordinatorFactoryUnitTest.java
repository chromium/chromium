// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
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
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.Tracker;
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
            new ObservableSupplierImpl<>(true);
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<PaneHubController> mPaneHubControllerSupplier =
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
    @Mock private Callback<Boolean> mHairlineVisibilityCallback;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private BackPressManager mBackpressManager;

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverCaptor;

    private Tab mTab1;

    private Activity mActivity;
    private FrameLayout mParentView;
    private TabSwitcherPaneCoordinatorFactory mFactory;
    private ObservableSupplierImpl<TabModelFilter> mTabModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        TrackerFactory.setTrackerForTests(mTracker);
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mProfileProviderSupplier.set(mProfileProvider);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel));
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        mTabModelFilterSupplier.set(mTabModelFilter);
        when(mTabModelFilterProvider.getCurrentTabModelFilterSupplier())
                .thenReturn(mTabModelFilterSupplier);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getProfile()).thenReturn(mProfile);

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
                        mModalDialogManager,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mBackpressManager,
                        /* desktopWindowStateProvider= */ null);
    }

    @Test
    @SmallTest
    public void testCreate_NativeAlreadyInitialized() {
        when(mLifecycleDispatcher.isNativeInitializationFinished()).thenReturn(true);
        TabSwitcherPaneCoordinator coordinator =
                mFactory.create(
                        mParentView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        mHairlineVisibilityCallback,
                        /* isIncognito= */ false,
                        /* onTabGroupCreation= */ null,
                        mEdgeToEdgeSupplier);
        assertNotNull(coordinator);

        TabSwitcherMessageManager messageManager = mFactory.getMessageManagerForTesting();
        assertNotNull(messageManager);
        assertTrue(messageManager.isNativeInitializedForTesting());

        coordinator.destroy();

        assertNull(mFactory.getMessageManagerForTesting());
    }

    @Test
    @SmallTest
    public void testCreateTwoCoordinators_NativeAlreadyInitialized() {
        when(mLifecycleDispatcher.isNativeInitializationFinished()).thenReturn(true);
        TabSwitcherPaneCoordinator coordinator1 =
                mFactory.create(
                        mParentView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        mHairlineVisibilityCallback,
                        /* isIncognito= */ false,
                        /* onTabGroupCreation= */ null,
                        mEdgeToEdgeSupplier);
        assertNotNull(coordinator1);

        TabSwitcherMessageManager messageManager = mFactory.getMessageManagerForTesting();
        assertNotNull(messageManager);
        assertTrue(messageManager.isNativeInitializedForTesting());

        TabSwitcherPaneCoordinator coordinator2 =
                mFactory.create(
                        mParentView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        mHairlineVisibilityCallback,
                        /* isIncognito= */ false,
                        /* onTabGroupCreation= */ null,
                        mEdgeToEdgeSupplier);
        assertNotNull(coordinator2);
        assertEquals(messageManager, mFactory.getMessageManagerForTesting());

        coordinator1.destroy();
        assertNotNull(mFactory.getMessageManagerForTesting());

        coordinator2.destroy();
        assertNull(mFactory.getMessageManagerForTesting());
    }

    @Test
    @SmallTest
    public void testCreate_NativeNotInitialized() {
        when(mLifecycleDispatcher.isNativeInitializationFinished()).thenReturn(false);
        TabSwitcherPaneCoordinator coordinator =
                mFactory.create(
                        mParentView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        mHairlineVisibilityCallback,
                        /* isIncognito= */ false,
                        /* onTabGroupCreation= */ null,
                        mEdgeToEdgeSupplier);
        assertNotNull(coordinator);

        TabSwitcherMessageManager messageManager = mFactory.getMessageManagerForTesting();
        assertNotNull(messageManager);
        assertFalse(messageManager.isNativeInitializedForTesting());
        verify(mLifecycleDispatcher).register(mLifecycleObserverCaptor.capture());

        ((NativeInitObserver) mLifecycleObserverCaptor.getValue()).onFinishNativeInitialization();

        verify(mLifecycleDispatcher).unregister(mLifecycleObserverCaptor.getValue());
        assertTrue(messageManager.isNativeInitializedForTesting());

        coordinator.destroy();

        assertNull(mFactory.getMessageManagerForTesting());
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
