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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.util.Pair;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationListener;
import org.chromium.chrome.browser.hub.HubLayoutAnimationType;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Objects;
import java.util.function.DoubleConsumer;

/** Unit tests for {@link TabSwitcherPane} and {@link TabSwitcherPaneBase}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabSwitcherPaneUnitTest {
    private static final int TAB_ID = 723849;

    private static class IphCommandMatcher implements ArgumentMatcher<IPHCommand> {
        private final String mFeatureName;

        public IphCommandMatcher(String featureName) {
            mFeatureName = featureName;
        }

        @Override
        public boolean matches(IPHCommand iphCommand) {
            return Objects.equals(iphCommand.featureName, mFeatureName);
        }
    }

    private static IphCommandMatcher surfaceIph() {
        return new IphCommandMatcher(FeatureConstants.TAB_GROUPS_SURFACE);
    }

    private static IphCommandMatcher surfaceOnHideIph() {
        return new IphCommandMatcher(FeatureConstants.TAB_GROUPS_SURFACE_ON_HIDE);
    }

    private static IphCommandMatcher remoteGroupIph() {
        return new IphCommandMatcher(FeatureConstants.TAB_GROUPS_REMOTE_GROUP);
    }

    private static IphCommandMatcher floatingActionButtonIph() {
        return new IphCommandMatcher(FeatureConstants.TAB_SWITCHER_FLOATING_ACTION_BUTTON);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SharedPreferences mSharedPreferences;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;
    @Mock private TabSwitcherDrawable mTabSwitcherDrawable;
    @Mock private HubContainerView mHubContainerView;
    @Mock private View.OnClickListener mNewTabButtonClickListener;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private PaneHubController mPaneHubController;
    @Mock private TabSwitcherCustomViewManager.Delegate mCustomViewManagerDelegate;
    @Mock private View mCustomView;
    @Mock private DoubleConsumer mOnAlphaChange;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mAnchorView;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Runnable mRunnable;
    @Mock private Tab mTab;
    @Mock private SavedTabGroup mSavedTabGroup;

    @Captor private ArgumentCaptor<ObservableSupplier<Boolean>> mIsAnimatingSupplierCaptor;

    @Captor
    private ArgumentCaptor<OnSharedPreferenceChangeListener> mPriceAnnotationsPrefListenerCaptor;

    @Captor private ArgumentCaptor<Callback<Integer>> mOnTabClickedCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mHairlineVisibilityCallbackCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final Token mToken = Token.createRandom();

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mHandleBackPressChangeSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mIsScrollingSupplier = new ObservableSupplierImpl<>();
    private OneshotSupplierImpl<ObservableSupplier<Boolean>> mIsScrollingSupplierSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private TabSwitcherPane mTabSwitcherPane;
    private MockTabModel mTabModel;
    private int mTimesCreated;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        TabSwitcherPaneBase.setShowIphForTesting(true);

        mContext = ApplicationProvider.getApplicationContext();

        TrackerFactory.setTrackerForTests(mTracker);

        when(mHubContainerView.getContext()).thenReturn(mContext);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);

        mActionTester = new UserActionTester();

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileProviderSupplier.set(mProfileProvider);

        mTabModel = new MockTabModel(mProfile, null);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);

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
                        mIsAnimatingSupplierCaptor.capture(),
                        mOnTabClickedCallbackCaptor.capture(),
                        mHairlineVisibilityCallbackCaptor.capture(),
                        anyBoolean(),
                        any(),
                        any());
        when(mTabSwitcherPaneCoordinatorFactory.getTabListMode()).thenReturn(TabListMode.GRID);
        when(mTabSwitcherPaneCoordinator.getHandleBackPressChangedSupplier())
                .thenReturn(mHandleBackPressChangeSupplier);
        mHandleBackPressChangeSupplier.set(false);
        when(mTabSwitcherPaneDrawableCoordinator.getTabSwitcherDrawable())
                .thenReturn(mTabSwitcherDrawable);
        when(mTabSwitcherDrawable.getShowIconNotificationStatus()).thenReturn(true);
        doAnswer(
                        invocation -> {
                            return mHandleBackPressChangeSupplier.get()
                                    ? BackPressResult.SUCCESS
                                    : BackPressResult.FAILURE;
                        })
                .when(mTabSwitcherPaneCoordinator)
                .handleBackPress();
        when(mTabSwitcherPaneCoordinator.getIsScrollingSupplier())
                .thenReturn(mIsScrollingSupplierSupplier);

        mTabSwitcherPane =
                new TabSwitcherPane(
                        mContext,
                        mSharedPreferences,
                        mProfileProviderSupplier,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mTabGroupModelFilter,
                        mNewTabButtonClickListener,
                        mTabSwitcherPaneDrawableCoordinator,
                        mOnAlphaChange,
                        mUserEducationHelper,
                        mEdgeToEdgeSupplier);
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
    public void testInitWithNativeBeforeCoordinatorCreation() {
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    public void testInitWithNativeAfterCoordinatorCreation() {
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator, never()).initWithNative();

        mTabSwitcherPane.initWithNative();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    public void testPaneId() {
        assertEquals(PaneId.TAB_SWITCHER, mTabSwitcherPane.getPaneId());
    }

    @Test
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
    public void testLoadHintColdHotWarm() {
        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
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
    public void testLoadHintColdHot_TabStateNotInitialized() {
        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(false);

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).resetWithTabList(mTabGroupModelFilter);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.GridTabSwitcher.TimeToTabStateInitializedFromShown");
        mTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
        watcher.assertExpected();
    }

    @Test
    public void testLoadHintColdWarmHotCold() {
        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);

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
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());
        verify(coordinator).softCleanup();
        verify(coordinator).hardCleanup();
    }

    @Test
    public void testGetRootView() {
        assertNotNull(mTabSwitcherPane.getRootView());
    }

    @Test
    @EnableFeatures(ANDROID_HUB_FLOATING_ACTION_BUTTON)
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
        verify(mTracker).notifyEvent("tab_switcher_floating_action_button_clicked");
    }

    @Test
    @DisableFeatures(ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testNewTabButton_NonFloating() {
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
        verify(mTracker, never()).notifyEvent(any());
    }

    @Test
    public void testReferenceButton() {
        DisplayButtonData buttonData = mTabSwitcherPane.getReferenceButtonDataSupplier().get();

        assertEquals(
                mContext.getString(R.string.tab_switcher_standard_stack_text),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.accessibility_tab_switcher_standard_stack,
                                mTabModel.getCount(),
                                mTabModel.getCount()),
                buttonData.resolveContentDescription(mContext));
        assertEquals(mTabSwitcherDrawable, buttonData.resolveIcon(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testReferenceButton_WithNotification() {
        DisplayButtonData buttonData = mTabSwitcherPane.getReferenceButtonDataSupplier().get();

        assertEquals(
                mContext.getString(R.string.tab_switcher_standard_stack_text),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                R.plurals
                                        .accessibility_tab_switcher_standard_stack_with_notification,
                                mTabModel.getCount(),
                                mTabModel.getCount()),
                buttonData.resolveContentDescription(mContext));
        assertEquals(mTabSwitcherDrawable, buttonData.resolveIcon(mContext));
    }

    @Test
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
    public void testCreateFadeOutAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    public void testCreateFadeInAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mTabSwitcherPane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
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
    public void testPriceTracking() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        assertNotNull(mTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mTabSwitcherPane.destroyTabSwitcherPaneCoordinator();
        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);

        OnSharedPreferenceChangeListener listener = mPriceAnnotationsPrefListenerCaptor.getValue();
        // Check this doesn't crash if there is no coordinator.
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();

        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);

        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(false);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);

        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(false);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);

        listener.onSharedPreferenceChanged(mSharedPreferences, "foo");
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);

        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        listener.onSharedPreferenceChanged(
                mSharedPreferences, PriceTrackingUtilities.TRACK_PRICES_ON_TABS);
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
    }

    @Test
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
    public void testGetDialogVisibilitySupplier() {
        assertNull(mTabSwitcherPane.getTabGridDialogVisibilitySupplier());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        assertNotNull(mTabSwitcherPane.getTabGridDialogVisibilitySupplier());
        verify(mTabSwitcherPaneCoordinator).getTabGridDialogVisibilitySupplier();
    }

    @Test
    public void testGetCustomViewManager() {
        assertNotNull(mTabSwitcherPane.getTabSwitcherCustomViewManager());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator).getTabSwitcherCustomViewManagerDelegate();

        TabSwitcherCustomViewManager customViewManager =
                mTabSwitcherPane.getTabSwitcherCustomViewManager();
        Runnable r = CallbackUtils.emptyRunnable();
        assertTrue(customViewManager.requestView(mCustomView, r, true));
        verify(mCustomViewManagerDelegate).addCustomView(mCustomView, r, true);

        mTabSwitcherPane.destroyTabSwitcherPaneCoordinator();
        verify(mCustomViewManagerDelegate).removeCustomView(mCustomView);
    }

    @Test
    public void testGetTabListModelSize() {
        assertEquals(0, mTabSwitcherPane.getTabSwitcherTabListModelSize());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        int tabCount = 5;
        when(mTabSwitcherPaneCoordinator.getTabSwitcherTabListModelSize()).thenReturn(tabCount);

        assertEquals(tabCount, mTabSwitcherPane.getTabSwitcherTabListModelSize());
    }

    @Test
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
    public void testResetWithTabList() {
        assertFalse(mTabSwitcherPane.resetWithTabList(null, false));

        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPane.getTabSwitcherPaneCoordinator();

        assertTrue(mTabSwitcherPane.resetWithTabList(null, false));
        verify(coordinator).resetWithTabList(null);

        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mTabSwitcherPane.showAllTabs();
        verify(coordinator, times(2)).resetWithTabList(null);
        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(false);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(3)).resetWithTabList(null);

        when(mTabGroupModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(mTabGroupModelFilter);
    }

    @Test
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

    @Test
    public void testHairlineVisibilitySupplier() {
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        var hairlineVisibilitySupplier = mTabSwitcherPane.getHairlineVisibilitySupplier();
        assertNull(hairlineVisibilitySupplier.get());

        mHairlineVisibilityCallbackCaptor.getValue().onResult(true);
        assertTrue(hairlineVisibilitySupplier.get());

        mHairlineVisibilityCallbackCaptor.getValue().onResult(false);
        assertFalse(hairlineVisibilitySupplier.get());
    }

    @Test
    public void testPriceDropRecord() {
        mTabModel.addTab(TAB_ID);
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasks();

        mTabModel.setIndex(0, TabSelectionType.FROM_USER);
        assertEquals(1, mActionTester.getActionCount("Commerce.TabGridSwitched.NoPriceDrop"));

        mTabModel.addTab(TAB_ID + 1);
        mTabModel.setIndex(1, TabSelectionType.FROM_USER);
        assertEquals(2, mActionTester.getActionCount("Commerce.TabGridSwitched.NoPriceDrop"));

        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasks();

        mTabModel.addTab(TAB_ID + 2);
        mTabModel.setIndex(2, TabSelectionType.FROM_USER);
        assertEquals(2, mActionTester.getActionCount("Commerce.TabGridSwitched.NoPriceDrop"));
    }

    @Test
    public void testAnimationListener() {
        mTabModel.addTab(TAB_ID);
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);

        assertFalse(mIsAnimatingSupplierCaptor.getValue().get());

        HubLayoutAnimationListener listener = mTabSwitcherPane.getHubLayoutAnimationListener();
        listener.beforeStart();
        assertTrue(mIsAnimatingSupplierCaptor.getValue().get());

        listener.afterEnd();
        assertFalse(mIsAnimatingSupplierCaptor.getValue().get());
    }

    @Test
    public void testIphOnTabGroupHide_shown() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        Token groupId = Token.createRandom();
        when(mTabGroupModelFilter.isTabGroupHiding(groupId)).thenReturn(true);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB_ID, groupId, DidRemoveTabGroupReason.CLOSE);
        verify(mUserEducationHelper).requestShowIPH(argThat(surfaceOnHideIph()));
    }

    @Test
    public void testIphOnTabGroupHide_nullButton() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        Token groupId = Token.createRandom();
        when(mTabGroupModelFilter.isTabGroupHiding(groupId)).thenReturn(true);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(null);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB_ID, groupId, DidRemoveTabGroupReason.CLOSE);
        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnTabGroupHide_nullController() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        Token groupId = Token.createRandom();
        when(mTabGroupModelFilter.isTabGroupHiding(groupId)).thenReturn(true);
        mTabSwitcherPane.setPaneHubController(null);
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB_ID, groupId, DidRemoveTabGroupReason.CLOSE);
        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnTabGroupHide_notHiding() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        Token groupId = Token.createRandom();
        when(mTabGroupModelFilter.isTabGroupHiding(groupId)).thenReturn(false);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB_ID, groupId, DidRemoveTabGroupReason.CLOSE);
        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnTabGroupHide_ungroup() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        Token groupId = Token.createRandom();
        when(mTabGroupModelFilter.isTabGroupHiding(groupId)).thenReturn(true);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(null);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB_ID, groupId, DidRemoveTabGroupReason.UNGROUP);
        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testAddAndRemoveTabGroupObserver() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter).addTabGroupObserver(any());

        // Starts off with a useless removeTabGroupObserver from first hint in setup.
        verify(mTabGroupModelFilter, times(1)).removeTabGroupObserver(any());
        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        verify(mTabGroupModelFilter, times(2)).removeTabGroupObserver(any());
    }

    @Test
    public void testIphOnCreation_shown() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();

        verify(mUserEducationHelper).requestShowIPH(argThat(surfaceIph()));
    }

    @Test
    public void testIphOnCreation_nullButton() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(null);

        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnCreation_noGroups() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnCreation_nullController() {
        mTabSwitcherPane.setPaneHubController(null);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testIphOnCreation_animating() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);
        HubLayoutAnimationListener hubLayoutAnimationListener =
                mTabSwitcherPane.getHubLayoutAnimationListener();
        hubLayoutAnimationListener.beforeStart();

        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();
        verify(mUserEducationHelper, never()).requestShowIPH(any());

        hubLayoutAnimationListener.afterEnd();
        mTabSwitcherPane.getOnTabGroupCreationRunnable().run();
        verify(mUserEducationHelper).requestShowIPH(argThat(surfaceIph()));
    }

    @Test
    public void testSurfaceIphOnShown_shown() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getPaneButton(anyInt())).thenReturn(mAnchorView);
        when(mPaneHubController.getFloatingActionButton()).thenReturn(null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper).requestShowIPH(argThat(surfaceIph()));
    }

    @Test
    public void testFloatingActionButtonIphOnShown_shown() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {""});
        when(mPaneHubController.getFloatingActionButton()).thenReturn(mAnchorView);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper).requestShowIPH(argThat(floatingActionButtonIph()));
    }

    @Test
    public void testFloatingActionButtonIphOnShown_nullButton() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getFloatingActionButton()).thenReturn(null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testFloatingActionButtonIphOnShown_nullController() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.setPaneHubController(null);
        when(mPaneHubController.getFloatingActionButton()).thenReturn(mAnchorView);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testFloatingActionButtonIphOnShown_animating() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mPaneHubController.getFloatingActionButton()).thenReturn(mAnchorView);
        HubLayoutAnimationListener hubLayoutAnimationListener =
                mTabSwitcherPane.getHubLayoutAnimationListener();
        hubLayoutAnimationListener.beforeStart();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mUserEducationHelper, never()).requestShowIPH(any());

        hubLayoutAnimationListener.afterEnd();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_onShown() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper).requestShowIPH(argThat(remoteGroupIph()));
    }

    @Test
    public void testRemoteGroupIph_onScrollStop() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        reset(mUserEducationHelper);

        mIsScrollingSupplier.set(false);
        mIsScrollingSupplierSupplier.set(mIsScrollingSupplier);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper).requestShowIPH(argThat(remoteGroupIph()));
    }

    @Test
    public void testRemoteGroupIph_onAddRemoteGroup() {
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        reset(mUserEducationHelper);

        mTabGroupModelFilterObserverCaptor.getValue().didCreateNewGroup(mTab, mTabGroupModelFilter);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper).requestShowIPH(argThat(remoteGroupIph()));
    }

    @Test
    public void testRemoteGroupIph_nullController() {
        mTabSwitcherPane.setPaneHubController(null);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_nullRange() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(null);
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_nullTab() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(null);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_notGroup() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(false);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_nullToken() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_nullSavedGroup() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_notRemote() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(false);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(mAnchorView);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    @Test
    public void testRemoteGroupIph_nullView() {
        mTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getVisibleRange()).thenReturn(new Pair<>(0, 0));
        when(mTabGroupModelFilter.getTabAt(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getViewByIndex(anyInt())).thenReturn(null);

        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUserEducationHelper, never()).requestShowIPH(any());
    }

    private void createSelectedTab() {
        mTabModel.addTab(TAB_ID);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);
    }
}
